#include "gr/kernel/history.hpp"
#include "gr/kernel/compact_history.hpp"
#include "internal.hpp"
#include <algorithm>
#include <deque>
#include <set>

namespace gr::history {
namespace {
using core::detail::require;
using Key = std::pair<std::string, std::string>;
using Records = std::map<std::string, Json>;
constexpr std::size_t max_nodes = 1024, max_history_bytes = 64 * 1024 * 1024;

Key key(const Json &record) {
    return {record.at("document").at("id").get<std::string>(), record.at("version").get<std::string>()};
}
Records current(const Json &snapshot) {
    Records out;
    for (const auto &record : snapshot.at("records"))
        out.emplace(key(record).first, record);
    return out;
}
Json normalized(const Json &snapshot) {
    Json out = {{"revision", "0"}, {"records", Json::array()}, {"archive", Json::array()}};
    const auto live = current(snapshot);
    std::map<Key, Json> archived;
    for (const auto &[id, record] : live) {
        (void)id;
        out["records"].push_back(record);
    }
    if (snapshot.contains("archive"))
        for (const auto &record : snapshot["archive"]) {
            auto identity = key(record);
            if (live.contains(identity.first) && live.at(identity.first).at("version") == identity.second)
                continue;
            archived.emplace(std::move(identity), record);
        }
    for (const auto &[identity, record] : archived) {
        (void)identity;
        out["archive"].push_back(record);
    }
    return out;
}
// Ordered fixed-length digest lines avoid aggregate canonical-value limits and
// unframed concatenation ambiguity. Each entry hashes its full (ID, version) pair.
std::string state_digest(const Json &snapshot) {
    std::string bytes = "gr.history-state/1\n";
    for (const auto *section : {"records", "archive"}) {
        bytes += section;
        bytes += '\n';
        for (const auto &r : snapshot.at(section)) {
            auto [id, version] = key(r);
            bytes += sha256(canonical(Json{{"id", id}, {"version", version}}));
            bytes += '\n';
        }
    }
    return "sha256:" + sha256(bytes);
}
void attribution_shape(const Json &attribution) {
    canonical(attribution);
    for (auto field : {"agent", "method", "reason"})
        core::detail::text(attribution, field);
}
struct Graph {
    std::map<std::string, const Json *> nodes;
};
Graph inspect(const Json &bundle, const core::TypeRegistry &registry) {
    require(bundle.is_object() && bundle.size() == 2 &&
                bundle.value("format", std::string{}) == "gr.history/1" && bundle.contains("nodes") &&
                bundle["nodes"].is_array(),
            "history_format", "Expected gr.history/1 bundle with nodes array");
    require(bundle["nodes"].size() <= max_nodes, "history_limit", "History exceeds 1024 nodes");
    Graph graph;
    std::size_t retained_bytes = 0;
    for (const auto &node : bundle["nodes"]) {
        require(node.is_object() && node.size() == 3 && node.contains("document") &&
                    node.contains("snapshot"),
                "history_node", "Node requires document, snapshot and version only");
        const auto version = core::detail::text(node, "version");
        const auto &document = node["document"];
        require(document.is_object() && document.size() == 4 &&
                    document.value("format", std::string{}) == "gr.history-node/1" &&
                    document.contains("parents") && document["parents"].is_array() &&
                    document.contains("attribution"),
                "history_node", "Invalid versioned history node descriptor");
        require(document["parents"].size() <= 32, "history_limit", "At most 32 immediate parents");
        attribution_shape(document["attribution"]);
        const auto descriptor_bytes = canonical(document);
        require(descriptor_bytes.size() <= max_history_bytes - retained_bytes, "history_limit",
                "History metadata exceeds 64 MiB");
        retained_bytes += descriptor_bytes.size();
        require(core::detail::digest(version) && version == "sha256:" + sha256(descriptor_bytes),
                "history_integrity", "History descriptor digest mismatch");
        auto report = core::validate_snapshot(node["snapshot"], false, registry);
        require(report["valid"].get<bool>(), "history_snapshot", report["errors"].dump());
        const auto norm = normalized(node["snapshot"]);
        require(node["snapshot"] == norm, "history_encoding",
                "History checkpoints require sorted records, deduplicated archive and revision 0");
        require(core::detail::text(document, "state_digest") == state_digest(norm), "history_integrity",
                "Checkpoint does not match descriptor state digest");
        for (const auto *section : {"records", "archive"})
            for (const auto &r : norm[section]) {
                auto size = canonical(r["document"]).size();
                require(size <= max_history_bytes - retained_bytes, "history_limit",
                        "History exceeds 64 MiB retained canonical record documents");
                retained_bytes += size;
            }
        require(graph.nodes.emplace(version, &node).second, "history_duplicate",
                "History repeats node identity");
    }
    std::map<std::string, std::size_t> indegree;
    std::map<std::string, std::vector<std::string>> children;
    for (const auto &[version, node] : graph.nodes) {
        std::set<std::string> unique;
        for (const auto &parent : node->at("document").at("parents")) {
            require(parent.is_string(), "history_parent", "Parent must be a revision identity");
            auto p = parent.get<std::string>();
            require(graph.nodes.contains(p), "history_parent", "Missing parent revision: " + p);
            require(unique.insert(p).second, "history_parent", "Repeated parent revision");
            children[p].push_back(version);
        }
        indegree[version] = unique.size();
    }
    std::deque<std::string> ready;
    for (const auto &[version, n] : indegree)
        if (n == 0)
            ready.push_back(version);
    std::size_t visited = 0;
    while (!ready.empty()) {
        auto n = ready.front();
        ready.pop_front();
        ++visited;
        for (const auto &child : children[n])
            if (--indegree[child] == 0)
                ready.push_back(child);
    }
    require(visited == graph.nodes.size(), "history_cycle", "Recording ancestry contains a cycle");
    return graph;
}
const Json &node_at(const Graph &graph, const std::string &revision) {
    auto it = graph.nodes.find(revision);
    require(it != graph.nodes.end(), "history_not_found", "Unknown recording revision: " + revision);
    return *it->second;
}
bool ancestor(const Graph &graph, const std::string &base, const std::string &tip) {
    std::set<std::string> visited;
    std::vector<std::string> pending{tip};
    while (!pending.empty()) {
        auto here = pending.back();
        pending.pop_back();
        if (here == base)
            return true;
        if (!visited.insert(here).second)
            continue;
        for (const auto &p : node_at(graph, here)["document"]["parents"])
            pending.push_back(p.get<std::string>());
    }
    return false;
}
Json version_at(const Records &records, const std::string &id) {
    auto it = records.find(id);
    return it == records.end() ? Json() : it->second.at("version");
}
void mutation_safe(const Json &snapshot, const core::TypeRegistry &registry) {
    auto report = core::validate_snapshot(snapshot, true, registry);
    require(report["valid"].get<bool>(), "history_admission", report["errors"].dump());
}
} // namespace

Json normalized_snapshot(const Json &snapshot) {
    return normalized(snapshot);
}
std::string snapshot_digest(const Json &snapshot) {
    return state_digest(snapshot);
}
Json make_node(const Json &snapshot, const Json &parents, const Json &attribution,
               const core::TypeRegistry &registry) {
    require(parents.is_array() && parents.size() <= 32, "history_parent",
            "parents must be an array of at most 32 revisions");
    std::set<std::string> unique;
    for (const auto &parent : parents) {
        require(parent.is_string() && core::detail::digest(parent.get<std::string>()) &&
                    unique.insert(parent.get<std::string>()).second,
                "history_parent", "Parents require distinct revision identities");
    }
    attribution_shape(attribution);
    mutation_safe(snapshot, registry);
    auto state = normalized(snapshot);
    Json document = {{"format", "gr.history-node/1"},
                     {"parents", parents},
                     {"state_digest", state_digest(state)},
                     {"attribution", attribution}};
    auto version = "sha256:" + sha256(canonical(document));
    return {{"version", version}, {"document", std::move(document)}, {"snapshot", std::move(state)}};
}

Json validate(const Json &bundle, const core::TypeRegistry &registry) {
    Json errors = Json::array();
    try {
        (void)inspect(bundle, registry);
    } catch (const Error &e) {
        errors.push_back({{"code", e.code}, {"message", e.what()}});
    } catch (const std::exception &e) {
        errors.push_back({{"code", "history_format"}, {"message", e.what()}});
    }
    return {{"valid", errors.empty()},
            {"errors", errors},
            {"scope", "Recording ancestry, checkpoint integrity and registered record validation; no "
                      "occurrence order or authority inference"}};
}
Json append(const Json &bundle, const Json &snapshot, const Json &parents, const Json &attribution,
            const core::TypeRegistry &registry) {
    const auto graph = inspect(bundle, registry);
    require(parents.is_array() && parents.size() <= 32, "history_parent",
            "parents must be an array of at most 32 revisions");
    attribution_shape(attribution);
    std::set<std::string> seen;
    for (const auto &parent : parents) {
        require(parent.is_string(), "history_parent", "Parent must be a revision identity");
        auto p = parent.get<std::string>();
        require(seen.insert(p).second, "history_parent", "Repeated parent revision");
        mutation_safe(node_at(graph, p)["snapshot"], registry);
    }
    mutation_safe(snapshot, registry);
    auto state = normalized(snapshot);
    Json descriptor = {{"format", "gr.history-node/1"},
                       {"parents", parents},
                       {"state_digest", state_digest(state)},
                       {"attribution", attribution}};
    const auto revision = "sha256:" + sha256(canonical(descriptor));
    Json node = {{"version", revision}, {"document", std::move(descriptor)}, {"snapshot", std::move(state)}};
    if (graph.nodes.contains(revision)) {
        require(node_at(graph, revision) == node, "history_integrity",
                "Identical revision identity contains different content");
        return bundle;
    }
    auto result = bundle;
    result["nodes"].push_back(std::move(node));
    (void)inspect(result, registry);
    return result;
}
Json reconstruct(const Json &bundle, const std::string &revision, const core::TypeRegistry &registry) {
    const auto graph = inspect(bundle, registry);
    const auto &node = node_at(graph, revision);
    auto result = node["snapshot"];
    result["revision"] = revision;
    result["history"] = {{"parents", node["document"]["parents"]},
                         {"attribution", node["document"]["attribution"]},
                         {"interpretation", "Recording ancestry, not represented event precedence"}};
    return result;
}
Json diff(const Json &bundle, const std::string &left, const std::string &right,
          const core::TypeRegistry &registry) {
    const auto graph = inspect(bundle, registry);
    const auto a = current(node_at(graph, left)["snapshot"]), b = current(node_at(graph, right)["snapshot"]);
    std::set<std::string> ids;
    for (const auto &[id, r] : a) {
        (void)r;
        ids.insert(id);
    }
    for (const auto &[id, r] : b) {
        (void)r;
        ids.insert(id);
    }
    Json added = Json::array(), removed = Json::array(), changed = Json::array();
    for (const auto &id : ids) {
        auto av = version_at(a, id), bv = version_at(b, id);
        if (av == bv)
            continue;
        Json item = {{"id", id}, {"before", av}, {"after", bv}};
        if (av.is_null())
            added.push_back(item);
        else if (bv.is_null())
            removed.push_back(item);
        else
            changed.push_back(item);
    }
    return {{"left", left},
            {"right", right},
            {"added", added},
            {"removed", removed},
            {"changed", changed},
            {"scope", "Differences in current record versions; retained archive content is not erased"}};
}
Json merge(const Json &bundle, const std::string &base, const std::string &left, const std::string &right,
           const Json &resolutions, const Json &attribution, const core::TypeRegistry &registry) {
    const auto graph = inspect(bundle, registry);
    const auto &bn = node_at(graph, base);
    const auto &ln = node_at(graph, left);
    const auto &rn = node_at(graph, right);
    require(ancestor(graph, base, left) && ancestor(graph, base, right), "merge_base",
            "Explicit merge base must be an ancestor of both tips");
    require(resolutions.is_object(), "merge_resolution",
            "Resolutions must map conflicting IDs to complete record versions or null");
    mutation_safe(bn["snapshot"], registry);
    mutation_safe(ln["snapshot"], registry);
    mutation_safe(rn["snapshot"], registry);
    attribution_shape(attribution);
    const auto b = current(bn["snapshot"]), l = current(ln["snapshot"]), r = current(rn["snapshot"]);
    std::set<std::string> ids, conflicting;
    for (const auto *m : {&b, &l, &r})
        for (const auto &[id, record] : *m) {
            (void)record;
            ids.insert(id);
        }
    Records merged;
    Json conflicts = Json::array();
    for (const auto &id : ids) {
        auto bv = version_at(b, id), lv = version_at(l, id), rv = version_at(r, id);
        const Records *selected = nullptr;
        if (lv == rv)
            selected = &l;
        else if (lv == bv)
            selected = &r;
        else if (rv == bv)
            selected = &l;
        else {
            conflicting.insert(id);
            if (!resolutions.contains(id)) {
                conflicts.push_back({{"id", id}, {"base", bv}, {"left", lv}, {"right", rv}});
                continue;
            }
            if (!resolutions[id].is_null()) {
                auto record = core::Record::from_wire(resolutions[id], registry);
                require(record.id() == id, "merge_resolution",
                        "Resolution record identity differs from conflict identity");
                merged.emplace(id, record.wire());
            }
            continue;
        }
        if (selected->contains(id))
            merged.emplace(id, selected->at(id));
    }
    for (auto it = resolutions.begin(); it != resolutions.end(); ++it)
        require(conflicting.contains(it.key()), "merge_resolution",
                "Resolution supplied for an ID that is not in conflict: " + it.key());
    if (!conflicts.empty())
        return {
            {"merged", false}, {"base", base}, {"left", left}, {"right", right}, {"conflicts", conflicts}};
    Json snapshot = {{"revision", "0"}, {"records", Json::array()}, {"archive", Json::array()}};
    for (const auto &[id, record] : merged) {
        (void)id;
        snapshot["records"].push_back(record);
    }
    std::map<Key, Json> retained;
    for (const auto *node : {&bn, &ln, &rn})
        for (const auto *section : {"records", "archive"})
            for (const auto &record : node->at("snapshot").at(section))
                retained.emplace(key(record), record);
    for (const auto &[id, record] : retained) {
        (void)id;
        snapshot["archive"].push_back(record);
    }
    snapshot = normalized(snapshot);
    Json parents = Json::array({left});
    if (right != left)
        parents.push_back(right);
    auto result = append(bundle, snapshot, parents, attribution, registry);
    const auto revision = "sha256:" + sha256(canonical(Json{{"format", "gr.history-node/1"},
                                                            {"parents", parents},
                                                            {"state_digest", state_digest(snapshot)},
                                                            {"attribution", attribution}}));
    return {{"merged", true},
            {"history", std::move(result)},
            {"revision", revision},
            {"conflicts", Json::array()},
            {"resolution_scope",
             "Record-version three-way merge; selected content is not adjudicated factual truth"}};
}
} // namespace gr::history
