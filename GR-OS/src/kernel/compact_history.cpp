#include "gr/kernel/compact_history.hpp"
#include "internal.hpp"
#include <algorithm>
#include <set>

namespace gr::history {
namespace {
using core::detail::require;
using Key = std::pair<std::string, std::string>;
Key key(const Json &r) {
    return {r.at("document").at("id"), r.at("version")};
}
void bounded(std::size_t &count, std::size_t size, std::size_t limit) {
    require(size <= limit && count <= limit - size, "history_limit",
            "Compact history resource limit exceeded");
    count += size;
}
void descriptor(const Json &doc, const std::string &version) {
    require(doc.is_object() && doc.size() == 4 && doc.value("format", "") == "gr.history-node/1" &&
                doc.contains("parents") && doc["parents"].is_array() && doc["parents"].size() <= 32 &&
                doc.contains("attribution"),
            "history_node", "Invalid semantic history descriptor");
    for (auto field : {"agent", "method", "reason"})
        core::detail::text(doc["attribution"], field);
    require(core::detail::digest(core::detail::text(doc, "state_digest")) && core::detail::digest(version) &&
                "sha256:" + sha256(canonical(doc)) == version,
            "history_integrity", "Semantic descriptor identity mismatch");
    std::set<std::string> unique;
    for (const auto &p : doc["parents"])
        require(p.is_string() && core::detail::digest(p.get<std::string>()) &&
                    unique.insert(p.get<std::string>()).second,
                "history_parent", "Invalid or duplicate semantic parent");
}
void state(const Json &s, const Json &doc, const core::TypeRegistry &registry,
           core::ValidationSession *session = nullptr) {
    auto report =
        session ? session->validate_snapshot(s, false) : core::validate_snapshot(s, false, registry);
    require(report.at("valid").get<bool>(), "history_snapshot", report.at("errors").dump());
    require(s == normalized_snapshot(s), "history_encoding",
            "Compact state must use normalized v1 inventory");
    require(snapshot_digest(s) == doc.at("state_digest").get<std::string>(), "history_integrity",
            "Reconstructed checkpoint identity mismatch");
}
Json delta(const Json &before, const Json &after, const NodeRef &base) {
    Json out = {{"format", "gr.history-delta/1"},  {"base", base.to_json()},
                {"records_upsert", Json::array()}, {"records_remove", Json::array()},
                {"archive_add", Json::array()},    {"archive_remove", Json::array()}};
    std::map<std::string, Json> a, b;
    std::map<Key, Json> aa, bb;
    for (auto &r : before.at("records"))
        a.emplace(key(r).first, r);
    for (auto &r : after.at("records"))
        b.emplace(key(r).first, r);
    for (auto &[id, r] : b)
        if (!a.contains(id) || a.at(id) != r)
            out["records_upsert"].push_back(r);
    for (auto &[id, r] : a)
        if (!b.contains(id))
            out["records_remove"].push_back(id);
    for (auto &r : before.at("archive"))
        aa.emplace(key(r), r);
    for (auto &r : after.at("archive"))
        bb.emplace(key(r), r);
    for (auto &[k, r] : bb)
        if (!aa.contains(k))
            out["archive_add"].push_back(r);
    for (auto &[k, r] : aa)
        if (!bb.contains(k))
            out["archive_remove"].push_back(Json{{"id", k.first}, {"version", k.second}});
    return out;
}
Json apply_delta(const Json &before, const Json &d) {
    require(d.is_object() && d.size() == 6 && d.value("format", "") == "gr.history-delta/1" &&
                d.contains("base"),
            "history_encoding", "Invalid delta object");
    for (auto field : {"records_upsert", "records_remove", "archive_add", "archive_remove"})
        require(d.contains(field) && d[field].is_array(), "history_encoding",
                "Delta inventories must be arrays");
    std::map<std::string, Json> records;
    std::map<Key, Json> archive;
    for (auto &r : before.at("records"))
        records.emplace(key(r).first, r);
    for (auto &r : before.at("archive"))
        archive.emplace(key(r), r);
    std::set<std::string> touched;
    for (auto &id : d["records_remove"]) {
        require(id.is_string() && touched.insert(id.get<std::string>()).second &&
                    records.erase(id.get<std::string>()) == 1,
                "history_encoding", "Delta removes absent or duplicate current identity");
    }
    for (auto &r : d["records_upsert"]) {
        const auto id = key(r).first;
        require(touched.insert(id).second, "history_encoding", "Delta repeats current identity");
        records[id] = r;
    }
    std::set<Key> changed;
    for (auto &r : d["archive_remove"]) {
        require(r.is_object() && r.size() == 2, "history_encoding", "Invalid removed archive identity");
        Key k{core::detail::text(r, "id"), core::detail::text(r, "version")};
        require(changed.insert(k).second && archive.erase(k) == 1, "history_encoding",
                "Delta removes absent or duplicate archive identity");
    }
    for (auto &r : d["archive_add"]) {
        auto k = key(r);
        require(changed.insert(k).second && archive.emplace(k, r).second, "history_encoding",
                "Delta repeats archived identity");
    }
    Json out = {{"revision", "0"}, {"records", Json::array()}, {"archive", Json::array()}};
    for (auto &[id, r] : records)
        out["records"].push_back(r);
    for (auto &[id, r] : archive)
        out["archive"].push_back(r);
    return out;
}
Json presented(Json snapshot, const NodeRef &ref, const Json &doc) {
    snapshot["revision"] = ref.revision;
    snapshot["history"] = {{"parents", doc.at("parents")},
                           {"attribution", doc.at("attribution")},
                           {"interpretation", "Recording ancestry, not represented event precedence"}};
    return snapshot;
}
} // namespace

Json NodeRef::to_json() const {
    return {{"revision", revision}, {"object_digest", object_digest}};
}
NodeRef NodeRef::from_json(const Json &j) {
    require(j.is_object() && j.size() == 2, "history_reference",
            "Expected semantic revision and physical object digest");
    NodeRef r{core::detail::text(j, "revision"), core::detail::text(j, "object_digest")};
    require(core::detail::digest(r.revision) && core::detail::digest(r.object_digest), "history_reference",
            "Invalid history reference digest");
    return r;
}
HistoryObject encode_object(const Json &value, std::size_t max_bytes) {
    std::string bytes;
    try {
        bytes = value.dump(-1, ' ', false, Json::error_handler_t::strict);
    } catch (const Json::exception &e) {
        throw Error("history_encoding", e.what());
    }
    require(bytes.size() <= max_bytes, "history_limit", "History object exceeds byte limit");
    return {"sha256:" + sha256(bytes), {bytes.begin(), bytes.end()}};
}
Json decode_object(std::span<const std::uint8_t> bytes, std::size_t max_bytes) {
    require(bytes.size() <= max_bytes, "history_limit", "History object exceeds byte limit");
    std::vector<std::set<std::string>> keys;
    std::size_t values = 0;
    try {
        return Json::parse(bytes.begin(), bytes.end(), [&](int depth, Json::parse_event_t event, Json &v) {
            require(depth <= 80, "history_limit", "History object nesting exceeds 80");
            if (event == Json::parse_event_t::object_start)
                keys.emplace_back();
            else if (event == Json::parse_event_t::key)
                require(keys.back().insert(v.get<std::string>()).second, "duplicate_key",
                        "History object repeats a key");
            else if (event == Json::parse_event_t::object_end)
                keys.pop_back();
            if (event == Json::parse_event_t::value || event == Json::parse_event_t::object_end ||
                event == Json::parse_event_t::array_end)
                require(++values <= 2000000, "history_limit", "History object has too many values");
            return true;
        });
    } catch (const Json::exception &e) {
        throw Error("history_encoding", e.what());
    }
}
Json CompactReadStats::to_json() const {
    return {{"objects", objects}, {"bytes", bytes}, {"reconstructed_nodes", reconstructed_nodes}};
}
struct CompactReader::Impl {
    ObjectLoader loader;
    std::shared_ptr<const core::TypeRegistry> registry;
    std::unique_ptr<core::ValidationSession> validation;
    CompactLimits limits;
    CompactReadStats stats;
    std::map<std::string, Json> objects, snapshots;
    std::map<std::string, std::string> identities;
    std::set<std::string> active;
    std::size_t retained_snapshot_bytes = 0;
    Json object(const std::string &digest) {
        require(core::detail::digest(digest), "history_reference", "Invalid object digest");
        if (objects.contains(digest))
            return objects.at(digest);
        require(stats.objects < limits.read_objects, "history_limit", "Too many history objects");
        auto bytes = loader(digest, std::min(limits.object_bytes, limits.read_bytes - stats.bytes));
        bounded(stats.bytes, bytes.size(), limits.read_bytes);
        require("sha256:" + sha256(std::span<const std::uint8_t>(bytes)) == digest, "history_integrity",
                "History object digest mismatch");
        auto value = decode_object(bytes, limits.object_bytes);
        ++stats.objects;
        objects.emplace(digest, value);
        return value;
    }
    Json header(const NodeRef &ref) {
        NodeRef::from_json(ref.to_json());
        auto [where, added] = identities.emplace(ref.revision, ref.object_digest);
        require(added || where->second == ref.object_digest, "history_reference",
                "One semantic node has conflicting physical references");
        require(identities.size() <= limits.nodes, "history_limit", "Too many semantic history nodes");
        auto h = object(ref.object_digest);
        require(h.is_object() && h.size() == 6 && h.value("format", "") == "gr.history-object-node/1" &&
                    h.contains("version") && h["version"] == ref.revision && h.contains("document") &&
                    h.contains("parents") && h["parents"].is_array() && h.contains("state") &&
                    h.contains("delta_depth") && h["delta_depth"].is_number_unsigned(),
                "history_encoding", "Invalid compact node header");
        descriptor(h["document"], ref.revision);
        require(h["parents"].size() == h["document"]["parents"].size(), "history_parent",
                "Physical and semantic parent counts differ");
        for (std::size_t i = 0; i < h["parents"].size(); ++i)
            require(NodeRef::from_json(h["parents"][i]).revision ==
                        h["document"]["parents"][i].get<std::string>(),
                    "history_parent", "Physical and semantic parent identities differ");
        require(core::detail::digest(core::detail::text(h, "state")) &&
                    h["delta_depth"].get<std::size_t>() <= limits.delta_depth,
                "history_limit", "Invalid payload identity or excessive delta depth");
        return h;
    }
    Json reconstruct(const NodeRef &ref, std::size_t depth) {
        require(depth <= limits.delta_depth, "history_limit", "Delta reconstruction exceeds depth limit");
        auto h = header(ref);
        if (snapshots.contains(ref.object_digest))
            return snapshots.at(ref.object_digest);
        require(active.insert(ref.object_digest).second, "history_cycle", "Cyclic physical delta bases");
        Json snapshot;
        try {
            auto payload = object(h["state"]);
            if (payload.value("format", "") == "gr.history-checkpoint/1") {
                require(payload.is_object() && payload.size() == 2 && payload.contains("snapshot") &&
                            h["delta_depth"] == 0,
                        "history_encoding", "Invalid checkpoint payload or depth");
                snapshot = payload["snapshot"];
            } else {
                require(payload.contains("base"), "history_encoding", "Delta lacks base reference");
                auto base = NodeRef::from_json(payload["base"]);
                auto bh = header(base);
                require(h["delta_depth"].get<std::size_t>() == bh["delta_depth"].get<std::size_t>() + 1,
                        "history_encoding", "Delta depth does not follow its base");
                snapshot = apply_delta(reconstruct(base, depth + 1), payload);
            }
            state(snapshot, h["document"], *registry, validation.get());
            std::size_t retained = 0;
            for (auto section : {"records", "archive"})
                for (const auto &r : snapshot.at(section)) {
                    bounded(retained, validation->canonical_bytes(r), limits.reconstructed_bytes);
                    bounded(retained, 128, limits.reconstructed_bytes);
                }
            bounded(retained_snapshot_bytes, retained, limits.reconstructed_bytes);
        } catch (...) {
            active.erase(ref.object_digest);
            throw;
        }
        active.erase(ref.object_digest);
        ++stats.reconstructed_nodes;
        snapshots.emplace(ref.object_digest, snapshot);
        return snapshot;
    }
};
CompactReader::CompactReader(ObjectLoader loader, std::shared_ptr<const core::TypeRegistry> registry,
                             CompactLimits limits)
    : impl_(std::make_unique<Impl>()) {
    require(bool(loader) && limits.object_bytes > 0 && limits.read_bytes > 0 && limits.read_objects > 0 &&
                limits.nodes > 0,
            "history_limit", "History reader requires a loader and positive budgets");
    impl_->loader = std::move(loader);
    impl_->registry =
        registry ? std::move(registry) : std::make_shared<core::TypeRegistry>(core::default_types());
    impl_->validation = std::make_unique<core::ValidationSession>(*impl_->registry);
    impl_->limits = limits;
}
CompactReader::~CompactReader() = default;
Json CompactReader::header(const NodeRef &r) {
    try {
        return impl_->header(r);
    } catch (const Json::exception &e) {
        throw Error("history_encoding", e.what());
    }
}
Json CompactReader::reconstruct(const NodeRef &r) {
    try {
        auto h = impl_->header(r);
        return presented(impl_->reconstruct(r, 0), r, h["document"]);
    } catch (const Json::exception &e) {
        throw Error("history_encoding", e.what());
    }
}
const core::TypeRegistry &CompactReader::registry() const {
    return *impl_->registry;
}
CompactReadStats CompactReader::stats() const {
    return impl_->stats;
}
std::vector<std::string> CompactReader::loaded_digests() const {
    std::vector<std::string> result;
    for (auto &[digest, value] : impl_->objects)
        result.push_back(digest);
    return result;
}
bool CompactReader::ancestor(const NodeRef &base, const NodeRef &tip) {
    header(base);
    std::set<std::string> visited;
    std::vector<NodeRef> pending{tip};
    while (!pending.empty()) {
        auto here = pending.back();
        pending.pop_back();
        auto h = header(here);
        if (here.revision == base.revision)
            return true;
        if (!visited.insert(here.revision).second)
            continue;
        for (auto &p : h["parents"])
            pending.push_back(NodeRef::from_json(p));
    }
    return false;
}
Json CompactReader::export_v1(const std::vector<NodeRef> &nodes) {
    require(nodes.size() <= impl_->limits.nodes, "history_limit", "Export exceeds node budget");
    Json result = {{"format", "gr.history/1"}, {"nodes", Json::array()}};
    for (auto &ref : nodes) {
        auto h = header(ref);
        result["nodes"].push_back({{"version", ref.revision},
                                   {"document", h["document"]},
                                   {"snapshot", impl_->reconstruct(ref, 0)}});
    }
    auto report = validate(result, registry());
    require(report["valid"].get<bool>(), "history_integrity", report["errors"].dump());
    return result;
}
EncodedNode encode_node(const Json &node, const std::vector<NodeRef> &parents, CompactReader &reader,
                        std::optional<NodeRef> base, CompactPolicy policy) {
    require(policy.checkpoint_interval > 0 && policy.max_delta_depth > 0 && policy.max_delta_depth <= 64,
            "history_policy", "Checkpoint interval and maximum delta depth must be positive and bounded");
    require(node.is_object() && node.size() == 3 && node.contains("snapshot") && node.contains("document"),
            "history_node", "Expected exact v1 semantic node");
    auto revision = core::detail::text(node, "version");
    descriptor(node["document"], revision);
    state(node["snapshot"], node["document"], reader.registry());
    require(parents.size() == node["document"]["parents"].size(), "history_parent",
            "Missing physical parents");
    Json parent_refs = Json::array();
    for (std::size_t i = 0; i < parents.size(); ++i) {
        reader.header(parents[i]);
        require(parents[i].revision == node["document"]["parents"][i].get<std::string>(), "history_parent",
                "Physical parent differs from semantic descriptor");
        parent_refs.push_back(parents[i].to_json());
    }
    auto payload = encode_object(Json{{"format", "gr.history-checkpoint/1"}, {"snapshot", node["snapshot"]}});
    std::size_t depth = 0;
    if (base) {
        auto bh = reader.header(*base);
        auto proposed = bh["delta_depth"].get<std::size_t>() + 1;
        if (proposed < policy.checkpoint_interval && proposed <= policy.max_delta_depth) {
            auto before = normalized_snapshot(reader.reconstruct(*base));
            auto change = encode_object(delta(before, node["snapshot"], *base));
            if (change.bytes.size() < payload.bytes.size()) {
                payload = std::move(change);
                depth = proposed;
            }
        }
    }
    auto h = encode_object(Json{{"format", "gr.history-object-node/1"},
                                {"version", revision},
                                {"document", node["document"]},
                                {"parents", parent_refs},
                                {"state", payload.digest},
                                {"delta_depth", depth}});
    NodeRef ref{revision, h.digest};
    return {ref, {std::move(payload), std::move(h)}, depth};
}
} // namespace gr::history
