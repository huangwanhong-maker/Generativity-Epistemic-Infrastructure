#include "gr/runtime/history_store.hpp"
#include "gr/storage.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>

using gr::Json;
namespace {
int checks = 0;
void check(bool c, const char *m) {
    ++checks;
    if (!c)
        throw std::runtime_error(m);
}
template <class F> void fails(F &&f, const char *m) {
    bool failed = false;
    try {
        f();
    } catch (const gr::Error &) {
        failed = true;
    }
    check(failed, m);
}
Json record(int value) {
    return gr::core::construct({{"format", "gr.record/1"},
                                {"id", "sample"},
                                {"type", "gr.assertion@1"},
                                {"payload", {{"value", value}}},
                                {"attribution", {{"agent", "test:observer"}, {"method", "reported"}}}});
}
Json snapshot(int n) {
    return {{"revision", "0"}, {"records", Json::array({record(n)})}};
}
Json attribution(int n) {
    return {{"agent", "test:recorder"}, {"method", "declared"}, {"reason", "step " + std::to_string(n)}};
}
gr::storage::Result publish(const std::shared_ptr<gr::storage::IStore> &store,
                            const gr::runtime::HistoryWrite &w, int n) {
    gr::storage::Commit c{"test", store->head("test"), "tx:" + std::to_string(n), "test:recorder",
                          "history fixture"};
    for (const auto &r : w.selected_snapshot.at("records"))
        c.records.push_back({r["document"]["id"], r["version"], r["document"]});
    c.records.push_back({gr::runtime::runtime_history_id, w.manifest["version"], w.manifest["document"]});
    c.blobs = w.blobs;
    c.required_blob_digests = w.required_blob_digests;
    return store->commit(c);
}
void exercise(const std::shared_ptr<gr::storage::IStore> &store) {
    store->create_workspace("test");
    gr::runtime::HistoryStore history(store, "test");
    auto first = history.append({}, snapshot(0), attribution(0));
    check(first.blobs.size() == 3, "Fresh compact history prepares payload/header/catalogue");
    check(store->head("test") == "0", "Preparing history has no persistence side effects");
    auto physical = publish(store, first, 0);
    auto original = first;
    std::string root_semantic = first.selected_snapshot["revision"];
    for (int i = 1; i < 20; ++i) {
        auto next = history.append(first.manifest, snapshot(i), attribution(i), {4, 3});
        check(next.blobs.size() <= 3, "Append publishes bounded object count independent of prior history");
        publish(store, next, i);
        first = std::move(next);
    }
    check(history.read(first.manifest, root_semantic).snapshot == original.selected_snapshot,
          "Old semantic node remains selectable from newest manifest");
    check(history.read(original.manifest).snapshot == original.selected_snapshot,
          "Pinned old manifest remains readable after later publications");
    auto exported = history.export_v1(first.manifest);
    check(exported["nodes"].size() == 20 && gr::history::validate(exported)["valid"],
          "Complete compact export retains every node");
    auto branch =
        gr::history::append(exported, snapshot(900), Json::array({root_semantic}), attribution(900));
    const auto branch_head = branch["nodes"].back()["version"].get<std::string>();
    auto imported = history.import_v1(first.manifest, branch, branch_head, attribution(900));
    publish(store, imported, 900);
    check(history.export_v1(imported.manifest)["nodes"].size() == 21,
          "Branch import retains unselected former head and all custody");
    check(history.read(imported.manifest, first.selected_snapshot["revision"]).snapshot ==
              first.selected_snapshot,
          "Unselected former head remains reconstructible after branch publication");
    auto dropped = Json{{"format", "gr.history/1"}, {"nodes", Json::array({exported["nodes"][0]})}};
    fails([&] { history.import_v1(imported.manifest, dropped, root_semantic, attribution(901)); },
          "Import cannot abandon retained branches");
    const auto before = store->head("test");
    auto prepared = history.append(imported.manifest, snapshot(1000), attribution(1000));
    auto broken = prepared;
    broken.required_blob_digests.push_back("sha256:" + std::string(64, '0'));
    fails([&] { publish(store, broken, 1000); }, "Missing custody object aborts whole publication");
    check(store->head("test") == before && !store->receipt("test", "tx:1000"),
          "Failed publication leaves head and receipt unchanged");
    fails([&] { store->blob("test", prepared.blobs.back().digest); },
          "Failed publication leaves no new history object");
    auto good = publish(store, prepared, 1000);
    check(store->receipt("test", "tx:1000")->entry.revision == good.revision,
          "Successful history publication has durable receipt");
    store->create_workspace("other");
    gr::runtime::HistoryStore other(store, "other");
    fails([&] { other.read(prepared.manifest); }, "History object references cannot cross workspace custody");

    store->create_workspace("legacy");
    gr::runtime::HistoryStore legacy(store, "legacy");
    Json v1 = {{"format", "gr.history/1"}, {"nodes", Json::array()}};
    v1 = gr::history::append(v1, snapshot(1), Json::array(), attribution(1));
    auto legacy_write = legacy.import_v1({}, v1, v1["nodes"][0]["version"], attribution(1));
    auto publish_legacy = [&](const gr::runtime::HistoryWrite &w, int tx) {
        gr::storage::Commit c{"legacy", store->head("legacy"), "legacy:" + std::to_string(tx),
                              "test:recorder", "legacy fixture"};
        c.records = {{gr::runtime::runtime_history_id, w.manifest["version"], w.manifest["document"]}};
        c.blobs = w.blobs;
        c.required_blob_digests = w.required_blob_digests;
        store->commit(c);
    };
    publish_legacy(legacy_write, 1);
    auto old_bytes = store->blob("legacy", legacy_write.blobs[0].digest);
    auto next = legacy.append(legacy_write.manifest, snapshot(2), attribution(2));
    auto expected =
        gr::history::append(v1, snapshot(2), Json::array({v1["nodes"][0]["version"]}), attribution(2));
    check(next.manifest["document"]["type"] == gr::runtime::runtime_history_type_v1 &&
              next.blobs[0].bytes == gr::history::encode_object(expected).bytes,
          "Existing v1 append retains byte-identical legacy bundle encoding");
    publish_legacy(next, 2);
    auto upgraded = legacy.upgrade_v1(next.manifest, attribution(3));
    publish_legacy(upgraded, 3);
    auto recovered = legacy.export_v1(upgraded.manifest);
    check(recovered == expected,
          "Explicit atomic upgrade preserves exact semantic descriptors and snapshots");
    check(store->blob("legacy", legacy_write.blobs[0].digest) == old_bytes,
          "Upgrade preserves original v1 artifact bytes");
    fails([&] { legacy.upgrade_v1(upgraded.manifest, attribution(4)); },
          "Repeated implicit downgrade/upgrade is not accepted");

    auto registry = std::make_shared<gr::core::TypeRegistry>();
    gr::runtime::register_history_profiles(*registry);
    registry->register_type("fixture.required@1", [](const Json &) {});
    auto unknown_document = record(1)["document"];
    unknown_document["type"] = "fixture.required@1";
    unknown_document["required"] = true;
    Json constrained = {{"revision", "0"},
                        {"records", Json::array({gr::core::construct(unknown_document, *registry)})}};
    store->create_workspace("constrained");
    gr::runtime::HistoryStore extended(store, "constrained", registry);
    auto constrained_write = extended.append({}, constrained, attribution(40));
    gr::storage::Commit c{"constrained", "0", "constrained:0", "test:recorder", "required profile fixture"};
    c.records = {{gr::runtime::runtime_history_id, constrained_write.manifest["version"],
                  constrained_write.manifest["document"]}};
    c.blobs = constrained_write.blobs;
    c.required_blob_digests = constrained_write.required_blob_digests;
    store->commit(c);
    gr::runtime::HistoryStore unavailable(store, "constrained");
    check(unavailable.read(constrained_write.manifest).snapshot == constrained_write.selected_snapshot,
          "Compact state with unknown required profile remains readable");
    fails([&] { unavailable.append(constrained_write.manifest, snapshot(50), attribution(50)); },
          "Unavailable source constraint prevents mutation even if candidate replaces it");

    // A correct header/state digest alone cannot prove catalogue membership.
    store->create_workspace("forged");
    auto catalogue = gr::history::decode_object(original.blobs.back().bytes);
    catalogue["added"] = Json::array();
    auto root = gr::history::encode_object(catalogue);
    auto document = original.manifest["document"];
    document["payload"]["catalogue_digest"] = root.digest;
    document["artifacts"][0]["sha256"] = root.digest;
    document["artifacts"][0]["byte_length"] = root.bytes.size();
    auto forged_manifest = gr::core::construct(document, *registry);
    gr::storage::Commit forged{"forged", "0", "forged:0", "test:recorder", "invalid catalogue fixture"};
    forged.records = {
        {gr::runtime::runtime_history_id, forged_manifest["version"], forged_manifest["document"]}};
    forged.blobs = {{root.digest, root.bytes}};
    forged.required_blob_digests = {root.digest};
    for (std::size_t i = 0; i + 1 < original.blobs.size(); ++i) {
        forged.blobs.push_back(original.blobs[i]);
        forged.required_blob_digests.push_back(original.blobs[i].digest);
    }
    store->commit(forged);
    gr::runtime::HistoryStore forged_history(store, "forged");
    fails([&] { forged_history.read(forged_manifest); },
          "Selected head outside retained catalogue inventory is rejected even with valid object custody");
}
void bounded_upgrade() {
    auto store = std::make_shared<gr::storage::MemoryStore>();
    store->create_workspace("limit");
    gr::runtime::HistoryStore history(store, "limit");
    Json bundle = {{"format", "gr.history/1"}, {"nodes", Json::array()}};
    Json parents = Json::array();
    for (int i = 0; i < 65; ++i) {
        bundle = gr::history::append(bundle, snapshot(i), parents, attribution(i));
        parents = Json::array({bundle["nodes"].back()["version"]});
    }
    auto legacy = history.import_v1({}, bundle, parents[0], attribution(100));
    gr::storage::Commit c{"limit", "0", "limit:0", "test:recorder", "bounded upgrade fixture"};
    c.records = {{gr::runtime::runtime_history_id, legacy.manifest["version"], legacy.manifest["document"]}};
    c.blobs = legacy.blobs;
    c.required_blob_digests = legacy.required_blob_digests;
    const auto physical = store->commit(c).revision;
    fails([&] { history.upgrade_v1(legacy.manifest, attribution(101)); },
          "Oversized atomic upgrade fails rather than partially publishing");
    check(store->head("limit") == physical && history.export_v1(legacy.manifest) == bundle,
          "Failed bounded upgrade preserves original complete legacy history");
}
} // namespace
int main() {
    auto path = std::filesystem::temp_directory_path() /
                ("gr-os-history-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite");
    try {
        exercise(std::make_shared<gr::storage::MemoryStore>());
        bounded_upgrade();
        {
            auto sqlite = std::make_shared<gr::storage::SQLiteStore>(path.string());
            exercise(sqlite);
        }
        {
            auto reopened = std::make_shared<gr::storage::SQLiteStore>(path.string());
            auto physical = reopened->snapshot("test");
            Json manifest;
            for (const auto &r : physical.records)
                if (r.id == gr::runtime::runtime_history_id)
                    manifest = {{"version", r.version}, {"document", r.data}};
            gr::runtime::HistoryStore history(reopened, "test");
            check(history.export_v1(manifest)["nodes"].size() == 22,
                  "Complete compact history survives SQLite close/reopen");
        }
        std::filesystem::remove(path);
        std::cout << "history_store: " << checks << " checks passed across memory and SQLite\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "history_store failure: " << e.what() << "; fixture " << path.string() << '\n';
        return 1;
    }
}
