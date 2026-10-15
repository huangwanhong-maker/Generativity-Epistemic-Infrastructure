#include "gr/runtime/history_store.hpp"
#include <algorithm>
#include <set>

namespace gr::runtime {
namespace {
using history::NodeRef;
constexpr std::size_t max_bytes = 16 * 1024 * 1024, max_read = 64 * 1024 * 1024, max_nodes = 1024;
void require(bool condition, const char *code, const std::string &message) {
    if (!condition)
        throw Error(code, message);
}
bool digest(const Json &v) {
    if (!v.is_string())
        return false;
    const auto &s = v.get_ref<const std::string &>();
    return s.size() == 71 && s.starts_with("sha256:") && std::all_of(s.begin() + 7, s.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
bool v2(const Json &manifest) {
    return manifest.at("document").at("type") == runtime_history_type_v2;
}
void profile(const Json &document) {
    require(document.at("id") == runtime_history_id && document.value("required", false), "history_manifest",
            "Runtime history record requires reserved identity and interpretation flag");
    const auto &p = document.at("payload");
    const bool compact = document.at("type") == runtime_history_type_v2;
    const auto field = compact ? "catalogue_digest" : "bundle_digest";
    require(p.is_object() && p.size() == (compact ? 4 : 3) &&
                p.value("format", "") ==
                    (compact ? "gr.runtime.history-manifest/2" : "gr.runtime.history-manifest/1") &&
                p.contains("head") && p.contains(field) && digest(p[field]),
            "history_manifest", "Invalid runtime history manifest payload");
    if (compact) {
        NodeRef::from_json(p["head"]);
        require(p.contains("node_count") && p["node_count"].is_number_unsigned() &&
                    p["node_count"].get<std::uint64_t>() > 0 &&
                    p["node_count"].get<std::uint64_t>() <= max_nodes,
                "history_limit", "Compact manifest node count is invalid");
    } else
        require(digest(p["head"]), "history_manifest", "Invalid semantic head identity");
    require(document.contains("artifacts") && document["artifacts"].is_array() &&
                document["artifacts"].size() == 1 && document["artifacts"][0].at("sha256") == p[field],
            "history_manifest", "Manifest must pin exactly its representation root artifact");
}
void checked(const Json &manifest, const core::TypeRegistry &registry) {
    require(manifest.is_object() && manifest.contains("document") && manifest["document"].contains("type") &&
                manifest["document"]["type"].is_string() &&
                history_manifest_type_supported(manifest["document"]["type"].get<std::string>()),
            "history_manifest", "Unsupported history manifest type");
    auto report = core::verify(manifest, registry);
    require(report.at("valid").get<bool>(), "history_manifest", report.at("errors").dump());
    profile(manifest["document"]);
}
void valid_bundle(const Json &bundle, const core::TypeRegistry &registry) {
    auto report = history::validate(bundle, registry);
    require(report["valid"].get<bool>(), "history_integrity", report["errors"].dump());
}
void mutable_state(const Json &snapshot, const core::TypeRegistry &registry) {
    auto report = core::validate_snapshot(snapshot, true, registry);
    require(report["valid"].get<bool>(), "history_admission", report["errors"].dump());
}
void artifacts(const Json &snapshot, HistoryWrite &write) {
    for (auto field : {"records", "archive"}) {
        if (!snapshot.contains(field))
            continue;
        for (const auto &r : snapshot[field]) {
            const auto &doc = r.at("document");
            require(doc.at("id") != runtime_history_id, "history_manifest",
                    "Semantic history cannot contain its runtime control record");
            if (!doc.contains("artifacts"))
                continue;
            for (const auto &a : doc["artifacts"]) {
                const auto d = a.at("sha256").get<std::string>();
                const auto length = a.at("byte_length").get<std::uint64_t>();
                auto [it, inserted] = write.artifact_lengths.emplace(d, length);
                require(inserted || it->second == length, "artifact_size",
                        "One digest has incompatible declared lengths");
            }
        }
    }
}
struct Session {
    storage::IStore &store;
    std::string workspace;
    std::map<std::string, std::vector<std::uint8_t>> bytes;
    std::set<std::string> existing;
    std::size_t loaded_bytes = 0;
    std::vector<std::uint8_t> load(const std::string &d, std::size_t limit) {
        if (bytes.contains(d)) {
            require(bytes.at(d).size() <= limit, "history_limit",
                    "History object exceeds requested read budget");
            return bytes.at(d);
        }
        auto reader = store.open_blob(workspace, d);
        const auto length = reader->metadata().byte_length;
        require(length <= limit && length <= max_bytes && length <= max_read - loaded_bytes, "history_limit",
                "Retained history exceeds read budget");
        std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
        std::size_t offset = 0;
        while (offset < data.size()) {
            auto n = reader->read(offset, std::span<std::uint8_t>(data).subspan(offset));
            require(n > 0, "history_integrity", "History object reader ended before its declared length");
            offset += n;
        }
        require("sha256:" + sha256(std::span<const std::uint8_t>(data)) == d, "history_integrity",
                "History object hash mismatch");
        loaded_bytes += data.size();
        existing.insert(d);
        bytes.emplace(d, data);
        return data;
    }
    Json object(const std::string &d) { return history::decode_object(load(d, max_bytes)); }
    void add(const history::HistoryObject &o, HistoryWrite &write) {
        if (bytes.contains(o.digest)) {
            require(bytes.at(o.digest) == o.bytes, "history_integrity",
                    "Conflicting immutable object content");
            return;
        }
        require(write.blobs.size() < 128, "history_limit", "Atomic history encoding exceeds 128 new objects");
        std::size_t size = 0;
        for (const auto &b : write.blobs)
            size += b.bytes.size();
        require(o.bytes.size() <= max_read - size, "history_limit", "Atomic history encoding exceeds 64 MiB");
        bytes.emplace(o.digest, o.bytes);
        write.blobs.push_back({o.digest, o.bytes});
    }
    Json root(const Json &manifest) {
        const auto &p = manifest.at("document").at("payload");
        auto d = p.at(v2(manifest) ? "catalogue_digest" : "bundle_digest").get<std::string>();
        auto data = load(d, max_bytes);
        require(data.size() ==
                    manifest.at("document").at("artifacts")[0].at("byte_length").get<std::size_t>(),
                "artifact_size", "History root differs from declared artifact length");
        return history::decode_object(data);
    }
    void finish(HistoryWrite &write) {
        std::set<std::string> required = existing;
        for (const auto &b : write.blobs)
            required.insert(b.digest);
        for (const auto &[d, length] : write.artifact_lengths)
            required.insert(d);
        write.required_blob_digests.assign(required.begin(), required.end());
        write.diagnostics["read_bytes"] = loaded_bytes;
        write.diagnostics["new_objects"] = write.blobs.size();
        std::size_t written = 0;
        for (const auto &b : write.blobs)
            written += b.bytes.size();
        write.diagnostics["new_bytes"] = written;
    }
};
void catalogue_shape(const Json &c) {
    require(c.is_object() && c.size() == 5 && c.value("format", "") == "gr.history-catalogue/1" &&
                c.contains("previous") && (c["previous"].is_null() || digest(c["previous"])) &&
                c.contains("added") && c["added"].is_array() && c["added"].size() <= max_nodes &&
                c.contains("head") && c.contains("node_count") && c["node_count"].is_number_unsigned() &&
                c["node_count"].get<std::size_t>() <= max_nodes,
            "history_catalogue", "Invalid compact history catalogue");
    NodeRef::from_json(c["head"]);
    std::set<std::string> unique;
    for (const auto &r : c["added"])
        require(unique.insert(NodeRef::from_json(r).revision).second, "history_duplicate",
                "Catalogue repeats semantic identity");
}
Json catalogue_root(Session &s, const Json &manifest) {
    auto c = s.root(manifest);
    catalogue_shape(c);
    const auto &p = manifest.at("document").at("payload");
    require(c["head"] == p["head"] && c["node_count"] == p["node_count"], "history_catalogue",
            "Manifest disagrees with catalogue root");
    return c;
}
std::vector<NodeRef> catalogue_inventory(Session &s, const Json &manifest) {
    auto c = catalogue_root(s, manifest);
    auto expected = c["node_count"].get<std::size_t>();
    std::vector<std::vector<NodeRef>> pages;
    std::set<std::string> versions, visited;
    const auto total = expected;
    for (;;) {
        require(pages.size() <= max_nodes, "history_limit", "Catalogue chain exceeds limit");
        catalogue_shape(c);
        require(c["node_count"] == expected && c["added"].size() <= expected, "history_catalogue",
                "Catalogue count does not follow its predecessor");
        std::vector<NodeRef> page;
        for (auto &j : c["added"]) {
            auto ref = NodeRef::from_json(j);
            require(versions.insert(ref.revision).second, "history_duplicate",
                    "Catalogue chain repeats semantic identity");
            page.push_back(ref);
        }
        expected -= page.size();
        pages.push_back(std::move(page));
        if (c["previous"].is_null()) {
            require(expected == 0, "history_catalogue", "Catalogue predecessor is missing");
            break;
        }
        auto prior = c["previous"].get<std::string>();
        require(visited.insert(prior).second, "history_cycle", "Catalogue chain cycles");
        c = s.object(prior);
    }
    std::vector<NodeRef> result;
    for (auto it = pages.rbegin(); it != pages.rend(); ++it)
        result.insert(result.end(), it->begin(), it->end());
    require(result.size() == total, "history_catalogue", "Catalogue count mismatch");
    const auto head = NodeRef::from_json(manifest.at("document").at("payload").at("head"));
    require(std::find(result.begin(), result.end(), head) != result.end(), "history_catalogue",
            "Selected head is absent from retained inventory");
    return result;
}
NodeRef selected_ref(Session &s, const Json &manifest, const std::string &selected) {
    const auto root = catalogue_root(s, manifest);
    auto head = NodeRef::from_json(manifest.at("document").at("payload").at("head"));
    const auto revision = selected.empty() ? head.revision : selected;
    if (revision == head.revision)
        for (const auto &added : root["added"])
            if (NodeRef::from_json(added) == head)
                return head;
    // Ordinary append finds its head in the newest page. Selecting an older
    // retained head requires a catalogue membership proof, not an unrelated ref.
    for (auto &r : catalogue_inventory(s, manifest))
        if (r.revision == revision)
            return r;
    throw Error("history_not_found", "Selected revision is absent from pinned history catalogue");
}
Json manifest_record(const history::HistoryObject &root, const Json &head, std::size_t count,
                     const Json &attribution, bool compact, const core::TypeRegistry &registry) {
    Json payload = compact ? Json{{"format", "gr.runtime.history-manifest/2"},
                                  {"head", head},
                                  {"catalogue_digest", root.digest},
                                  {"node_count", count}}
                           : Json{{"format", "gr.runtime.history-manifest/1"},
                                  {"head", head},
                                  {"bundle_digest", root.digest}};
    return core::construct(
        Json{{"format", "gr.record/1"},
             {"id", runtime_history_id},
             {"type", compact ? runtime_history_type_v2 : runtime_history_type_v1},
             {"required", true},
             {"payload", payload},
             {"attribution", {{"agent", attribution.at("agent")}, {"method", "gr.runtime.transaction@1"}}},
             {"artifacts",
              Json::array({Json{{"sha256", root.digest},
                                {"byte_length", root.bytes.size()},
                                {"media_type", compact ? "application/vnd.gr.history-catalogue+json"
                                                       : "application/vnd.gr.history+json"}}})}},
        registry);
}
Json legacy_bundle(Session &s, const Json &manifest, const core::TypeRegistry &registry) {
    auto bundle = s.root(manifest);
    valid_bundle(bundle, registry);
    return bundle;
}
void retained(const Json &before, const Json &after) {
    std::map<std::string, const Json *> map;
    for (const auto &n : after.at("nodes"))
        map.emplace(n.at("version"), &n);
    for (const auto &n : before.at("nodes")) {
        auto found = map.find(n.at("version").get<std::string>());
        require(found != map.end() && *found->second == n, "history_retention",
                "Import drops or changes a retained semantic node");
    }
}
} // namespace

bool history_manifest_type_supported(const std::string &type) {
    return type == runtime_history_type_v1 || type == runtime_history_type_v2;
}
void register_history_profiles(core::TypeRegistry &registry) {
    require(!registry.known(runtime_history_type_v1) && !registry.known(runtime_history_type_v2),
            "reserved_type", "Runtime history profile is reserved");
    for (auto type : {runtime_history_type_v1, runtime_history_type_v2})
        registry.register_type(
            core::RecordTypeDescriptor{type, "Runtime-managed recording-history manifest", {}}, profile);
}
Json history_manifest_info(const Json &manifest) {
    profile(manifest.at("document"));
    const auto &p = manifest.at("document").at("payload");
    Json out = {{"head", v2(manifest) ? p["head"]["revision"] : p["head"]},
                {"manifest_version", manifest.at("version")},
                {"representation", v2(manifest) ? "compact/1" : "gr.history/1"}};
    if (v2(manifest)) {
        out["catalogue_digest"] = p["catalogue_digest"];
        out["node_count"] = p["node_count"];
    } else
        out["bundle_digest"] = p["bundle_digest"];
    return out;
}
HistoryStore::HistoryStore(std::shared_ptr<storage::IStore> store, std::string workspace,
                           std::shared_ptr<const core::TypeRegistry> registry)
    : store_(std::move(store)), workspace_(std::move(workspace)), registry_(std::move(registry)) {
    require(bool(store_), "history_store", "History store requires a backend");
    if (!registry_) {
        auto r = std::make_shared<core::TypeRegistry>(core::default_types());
        register_history_profiles(*r);
        registry_ = std::move(r);
    }
}
HistoryState HistoryStore::read(const Json &manifest, const std::string &selected) const {
    checked(manifest, *registry_);
    Session s{*store_, workspace_};
    Json snapshot, diagnostics;
    if (v2(manifest)) {
        auto ref = selected_ref(s, manifest, selected);
        history::CompactReader reader([&](const auto &d, auto n) { return s.load(d, n); }, registry_);
        snapshot = reader.reconstruct(ref);
        diagnostics = reader.stats().to_json();
    } else {
        auto bundle = legacy_bundle(s, manifest, *registry_);
        const auto revision =
            selected.empty() ? manifest["document"]["payload"]["head"].get<std::string>() : selected;
        snapshot = history::reconstruct(bundle, revision, *registry_);
        diagnostics = {{"representation", "gr.history/1"}};
    }
    diagnostics["read_bytes"] = s.loaded_bytes;
    return {manifest, snapshot.at("revision"), std::move(snapshot), std::move(diagnostics)};
}
Json HistoryStore::export_v1(const Json &manifest) const {
    checked(manifest, *registry_);
    Session s{*store_, workspace_};
    if (!v2(manifest))
        return legacy_bundle(s, manifest, *registry_);
    auto refs = catalogue_inventory(s, manifest);
    history::CompactReader reader([&](const auto &d, auto n) { return s.load(d, n); }, registry_);
    return reader.export_v1(refs);
}
HistoryWrite HistoryStore::append(const std::optional<Json> &manifest, const Json &candidate,
                                  const Json &attribution, HistoryPolicy policy, bool upgrade) const {
    if (manifest)
        checked(*manifest, *registry_);
    if (manifest && !v2(*manifest)) {
        auto bundle = export_v1(*manifest);
        const auto prior = manifest->at("document").at("payload").at("head").get<std::string>();
        auto node = history::make_node(candidate, Json::array({prior}), attribution, *registry_);
        auto next = history::append(bundle, candidate, Json::array({prior}), attribution, *registry_);
        return import_v1(manifest, next, node.at("version"), attribution, policy, upgrade);
    }
    Session s{*store_, workspace_};
    HistoryWrite result;
    history::CompactReader reader([&](const auto &d, auto n) { return s.load(d, n); }, registry_);
    std::vector<NodeRef> parents;
    Json parent_ids = Json::array(), previous = nullptr;
    std::size_t count = 0;
    if (manifest) {
        auto cat = catalogue_root(s, *manifest);
        count = cat["node_count"].get<std::size_t>();
        previous = manifest->at("document").at("payload").at("catalogue_digest");
        parents.push_back(NodeRef::from_json(cat["head"]));
        auto source = reader.reconstruct(parents.front());
        mutable_state(source, *registry_);
        parent_ids.push_back(parents.front().revision);
    }
    require(count < max_nodes, "history_limit", "Semantic history exceeds 1024 nodes");
    auto node = history::make_node(candidate, parent_ids, attribution, *registry_);
    auto encoded = history::encode_node(node, parents, reader,
                                        parents.empty() ? std::optional<NodeRef>{} : parents.front(), policy);
    for (auto &o : encoded.objects)
        s.add(o, result);
    auto catalogue = history::encode_object(Json{{"format", "gr.history-catalogue/1"},
                                                 {"previous", previous},
                                                 {"added", Json::array({encoded.node.to_json()})},
                                                 {"head", encoded.node.to_json()},
                                                 {"node_count", count + 1}});
    s.add(catalogue, result);
    result.manifest =
        manifest_record(catalogue, encoded.node.to_json(), count + 1, attribution, true, *registry_);
    result.selected_snapshot = reader.reconstruct(encoded.node);
    artifacts(node["snapshot"], result);
    result.diagnostics = reader.stats().to_json();
    result.diagnostics["delta_depth"] = encoded.delta_depth;
    s.finish(result);
    return result;
}
HistoryWrite HistoryStore::import_v1(const std::optional<Json> &manifest, const Json &bundle,
                                     const std::string &head, const Json &attribution, HistoryPolicy policy,
                                     bool upgrade) const {
    valid_bundle(bundle, *registry_);
    auto selected = history::reconstruct(bundle, head, *registry_);
    mutable_state(selected, *registry_);
    if (manifest) {
        checked(*manifest, *registry_);
        retained(export_v1(*manifest), bundle);
    }
    Session s{*store_, workspace_};
    HistoryWrite result;
    for (const auto &n : bundle["nodes"])
        artifacts(n["snapshot"], result);
    if (!upgrade && (!manifest || !v2(*manifest))) {
        auto object = history::encode_object(bundle);
        s.add(object, result);
        result.manifest =
            manifest_record(object, head, bundle["nodes"].size(), attribution, false, *registry_);
    } else {
        history::CompactReader reader([&](const auto &d, auto n) { return s.load(d, n); }, registry_);
        std::map<std::string, NodeRef> refs;
        Json previous = nullptr;
        if (manifest && v2(*manifest)) {
            for (auto &r : catalogue_inventory(s, *manifest))
                refs.emplace(r.revision, r);
            previous = manifest->at("document").at("payload").at("catalogue_digest");
        } else if (manifest) {
            // Retain and verify the original legacy bytes during explicit upgrade.
            s.root(*manifest);
        }
        std::map<std::string, const Json *> pending;
        for (const auto &n : bundle["nodes"])
            if (!refs.contains(n["version"].get<std::string>()))
                pending.emplace(n["version"], &n);
        Json added = Json::array();
        while (!pending.empty()) {
            bool progress = false;
            for (auto it = pending.begin(); it != pending.end();) {
                const auto &n = *it->second;
                std::vector<NodeRef> parents;
                bool ready = true;
                for (const auto &p : n["document"]["parents"]) {
                    auto found = refs.find(p.get<std::string>());
                    if (found == refs.end()) {
                        ready = false;
                        break;
                    }
                    parents.push_back(found->second);
                }
                if (!ready) {
                    ++it;
                    continue;
                }
                auto e = history::encode_node(
                    n, parents, reader, parents.empty() ? std::optional<NodeRef>{} : parents.front(), policy);
                for (auto &o : e.objects)
                    s.add(o, result);
                refs.emplace(e.node.revision, e.node);
                added.push_back(e.node.to_json());
                it = pending.erase(it);
                progress = true;
            }
            require(progress, "history_parent", "Import cannot resolve semantic ancestry");
        }
        auto selected_ref = refs.at(head);
        auto catalogue = history::encode_object(Json{{"format", "gr.history-catalogue/1"},
                                                     {"previous", previous},
                                                     {"added", added},
                                                     {"head", selected_ref.to_json()},
                                                     {"node_count", refs.size()}});
        s.add(catalogue, result);
        result.manifest =
            manifest_record(catalogue, selected_ref.to_json(), refs.size(), attribution, true, *registry_);
        result.diagnostics = reader.stats().to_json();
    }
    result.selected_snapshot = std::move(selected);
    s.finish(result);
    return result;
}
HistoryWrite HistoryStore::upgrade_v1(const Json &manifest, const Json &attribution,
                                      HistoryPolicy policy) const {
    require(!v2(manifest), "history_upgrade", "History is already compact");
    auto bundle = export_v1(manifest);
    return import_v1(manifest, bundle, manifest["document"]["payload"]["head"], attribution, policy, true);
}
} // namespace gr::runtime
