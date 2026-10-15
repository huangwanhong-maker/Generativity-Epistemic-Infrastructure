#include "storage_support.hpp"
namespace {
void common_contract(IStore &store) {
    check(store.capabilities().at("contract") == "gr.storage/1", "contract identifier");
    error("not_found", [&] { store.head("test"); });
    store.create_workspace("test");
    store.create_workspace("other");
    store.create_workspace("../opaque/workspace"); // An identifier is never a filesystem path.
    check(store.head("../opaque/workspace") == "0", "opaque workspace identity");
    error("conflict", [&] { store.create_workspace("test"); });
    check(store.snapshot("test").records.empty(), "initial empty snapshot");
    auto first = request("first");
    first.records = {{"subject", "", Json{{"claim", "first account"}, {"qualifier", "contested"}}},
                     {"relation", "", Json{{"endpoints", Json::array({"subject", "subject"})}}}};
    const auto binary = material({0, 1, 127, 255, 0});
    first.blobs = {binary};
    first.required_blob_digests = {binary.digest};
    auto accepted = store.commit(first);
    check(!accepted.replayed && accepted.parent == "0" && accepted.current_revision == accepted.revision,
          "first result");
    check(store.head("test") == accepted.revision, "published head");
    auto old = store.snapshot("test", accepted.revision);
    check(old.records.size() == 2 && old.records[0].id == "relation", "stable id ordering");
    auto subject_version = content_version(first.records[0].data);
    check(store.record_version("test", "subject", subject_version).data == first.records[0].data,
          "immutable version lookup");
    check(store.blob("test", binary.digest) == binary.bytes, "exact binary roundtrip");
    check(store.snapshot("test", "0").records.empty(), "empty initial revision remains addressable");
    error("not_found", [&] { store.snapshot("other", accepted.revision); });
    error("not_found", [&] { store.record_version("other", "subject", subject_version); });
    error("not_found", [&] { store.record_version("test", "relation", subject_version); });
    error("not_found", [&] { store.blob("other", binary.digest); });
    auto second = request("second", accepted.revision);
    second.records = {{"subject", "", Json{{"claim", "revised account"}}}};
    second.required_blob_digests = {binary.digest}; // Refer to previously admitted bytes.
    auto revised = store.commit(second);
    check(store.snapshot("test").records.size() == 2, "upsert preserves omitted records");
    check(store.record_version("test", "subject", subject_version).data == first.records[0].data,
          "old version retained");
    check(store.snapshot("test", accepted.revision).records[1].data == first.records[0].data,
          "pinned historical snapshot");
    auto replay = store.commit(first);
    check(replay.replayed && replay.revision == accepted.revision &&
              replay.current_revision == revised.revision,
          "replay before stale base rejection");
    auto other_actor = first;
    other_actor.actor = "different recorder";
    error("transaction_conflict", [&] { store.commit(other_actor); });
    auto changed = first;
    changed.reason = "changed metadata";
    error("transaction_conflict", [&] { store.commit(changed); });
    auto changed_data = first;
    changed_data.records[0].data["claim"] = "changed bytes";
    error("transaction_conflict", [&] { store.commit(changed_data); });
    error("conflict", [&] { store.commit(request("stale", accepted.revision)); });
    auto bad_version = request("bad-version", revised.revision);
    bad_version.records = {{"subject", content_version("wrong"), Json{{"claim", "something"}}}};
    error("integrity_error", [&] { store.commit(bad_version); });
    auto bad_blob = request("bad-blob", revised.revision);
    bad_blob.blobs = {binary};
    bad_blob.blobs[0].bytes[0] = 9;
    bad_blob.required_blob_digests = {binary.digest};
    error("integrity_error", [&] { store.commit(bad_blob); });
    auto duplicate = request("duplicate", revised.revision);
    duplicate.records = {{"dup", "", 1}, {"dup", "", 2}};
    error("invalid_argument", [&] { store.commit(duplicate); });
    const auto orphan = material({7, 8, 9});
    auto incomplete = request("incomplete", revised.revision);
    incomplete.records = {{"must-not-publish", "", true}};
    incomplete.blobs = {orphan};
    incomplete.required_blob_digests = {orphan.digest, content_version("missing")};
    error("not_found", [&] { store.commit(incomplete); });
    check(store.head("test") == revised.revision, "failed closure leaves head intact");
    error("not_found", [&] { store.blob("test", orphan.digest); });
    error("not_found", [&] { store.record_version("test", "must-not-publish", content_version(true)); });
    check(store.history("test").entries.size() == 2, "failed commit does not publish receipt");
    const auto empty = material({});
    auto third = request("third", revised.revision);
    third.blobs = {empty};
    third.required_blob_digests = {empty.digest};
    auto latest = store.commit(third);
    check(store.blob("test", empty.digest).empty(), "empty blob is not NULL");
    auto page = store.history("test", "", 2);
    check(page.entries.size() == 2 && page.entries[0].revision == latest.revision &&
              page.next_cursor == revised.revision,
          "first history page");
    auto tail = store.history("test", page.next_cursor, 2);
    check(tail.entries.size() == 1 && tail.entries[0].revision == accepted.revision &&
              tail.next_cursor.empty(),
          "history continuation");
    check(store.history("test", accepted.revision).entries.empty(), "oldest exclusive cursor");
    error("not_found", [&] { store.history("other", accepted.revision); });
    error("invalid_argument", [&] { store.history("test", "", 0); });
    error("invalid_argument", [&] { store.history("test", "", 1001); });
}
void bounded_contract(IStore &store) {
    store.create_workspace("test");
    auto c = request("one");
    c.records = {{"a", "", "x"}, {"b", "", "y"}};
    auto blob = material({1, 2, 3, 4});
    c.blobs = {blob};
    c.required_blob_digests = {blob.digest};
    auto first = store.commit(c);
    auto too_many = request("too-many", first.revision);
    too_many.records = {{"c", "", "z"}};
    error("storage_limit", [&] { store.commit(too_many); });
    auto too_big = request("too-big", first.revision);
    too_big.records = {{"a", "", std::string(30, 'x')}};
    error("storage_limit", [&] { store.commit(too_big); });
    auto extra = request("extra", first.revision);
    auto b = material({5});
    extra.blobs = {b};
    extra.required_blob_digests = {b.digest};
    error("storage_limit", [&] { store.commit(extra); });
    check(store.head("test") == first.revision && store.history("test").entries.size() == 1,
          "limits preserve complete prior state");
    auto same = request("dedupe", first.revision);
    same.blobs = {blob};
    same.required_blob_digests = {blob.digest};
    check(!store.commit(same).replayed, "duplicate bytes do not consume retained quota twice");
}

void reader_contract(IStore &store) {
    store.create_workspace("reader");
    auto c = request("read", "0", "reader");
    const auto b = material({0, 1, 2, 3, 4, 5, 6, 7, 8, 255});
    c.blobs = {b};
    c.required_blob_digests = {b.digest};
    const auto result = store.commit(c);
    auto info = store.blob_info("reader", b.digest);
    check(info.byte_length == b.bytes.size() && info.integrity == "unverified",
          "cheap metadata makes no integrity claim");
    auto reader = store.open_blob("reader", b.digest, {4, {}});
    check(reader->metadata().digest == b.digest && reader->metadata().integrity == "sha256-verified",
          "reader verifies whole object before exposure");
    std::array<std::uint8_t, 12> buffer;
    buffer.fill(99);
    check(reader->read(2, buffer) == 4 && buffer[0] == 2 && buffer[3] == 5 && buffer[4] == 99,
          "range respects chunk and destination boundaries");
    check(reader->read(8, buffer) == 2 && buffer[0] == 8 && buffer[1] == 255, "short final range");
    check(reader->read(10, buffer) == 0 && reader->read(0, {}) == 0, "EOF and empty destination");
    error("blob_range", [&] { reader->read(11, buffer); });
    error("blob_range", [&] { reader->read(UINT64_MAX, buffer); });
    error("invalid_argument", [&] { store.open_blob("reader", b.digest, {0, {}}); });
    error("invalid_argument", [&] { store.open_blob("reader", b.digest, {1024 * 1024 + 1, {}}); });
    std::stop_source cancelled;
    cancelled.request_stop();
    buffer.fill(99);
    error("storage_cancelled", [&] { reader->read(0, buffer, cancelled.get_token()); });
    check(std::all_of(buffer.begin(), buffer.end(), [](auto x) { return x == 99; }),
          "pre-cancel leaves caller memory intact");
    error("storage_cancelled", [&] { store.open_blob("reader", b.digest, {4, cancelled.get_token()}); });
    std::stop_source lifetime;
    auto cancellable = store.open_blob("reader", b.digest, {4, lifetime.get_token()});
    lifetime.request_stop();
    error("storage_cancelled", [&] { cancellable->read(0, buffer); });
    auto receipt = store.receipt("reader", "read");
    check(receipt && receipt->entry.revision == result.revision &&
              receipt->current_revision == result.revision && receipt->entry.actor == c.actor,
          "receipt lookup preserves result and attribution");
    check(!store.receipt("reader", "absent"), "missing receipt is optional");
    store.create_workspace("reader-other");
    check(!store.receipt("reader-other", "read"), "receipts are workspace scoped");
    error("not_found", [&] { store.blob_info("reader-other", b.digest); });
    error("not_found", [&] { store.open_blob("reader-other", b.digest); });
    StorageRequirements requirements;
    requirements.atomic_commit = requirements.consistent_snapshots = requirements.immutable_versions = true;
    requirements.conditional_commit = requirements.verified_blob_ranges =
        requirements.blob_read_cancellation = requirements.reader_outlives_store = true;
    requirements.history_model = HistoryModel::linear;
    require_support(store.describe(), requirements);
    requirements.history_model = HistoryModel::dag;
    error("unsupported_storage_capability", [&] { require_support(store.describe(), requirements); });
}

using Factory = std::function<std::unique_ptr<IStore>()>;
void reader_lifetime(const Factory &factory) {
    std::unique_ptr<BlobReader> reader;
    const auto b = material({5, 4, 3, 2, 1});
    {
        auto store = factory();
        store->create_workspace("test");
        auto c = request("lifetime");
        c.blobs = {b};
        c.required_blob_digests = {b.digest};
        store->commit(c);
        reader = store->open_blob("test", b.digest, {2, {}});
    }
    std::array<std::uint8_t, 5> bytes{};
    check(reader->read(0, bytes) == 2 && bytes[0] == 5 && bytes[1] == 4, "reader survives store destruction");
    check(reader->read(4, bytes) == 1 && bytes[0] == 1, "surviving reader remains seekable");
}

// The model contains only expected user values and saved revisions. It does not
// reproduce implementation data structures, SQL, receipt hashing or CAS logic.
void seeded_model(IStore &store) {
    store.create_workspace("model");
    std::mt19937 rng(0x47524f53);
    std::map<std::string, Json> values;
    std::vector<std::pair<std::string, std::map<std::string, Json>>> saved;
    std::vector<Commit> admitted;
    std::string head = "0";
    for (int step = 0; step < 72; ++step) {
        auto c = request("seed-" + std::to_string(step), head, "model");
        const auto id = "record-" + std::to_string(rng() % 9);
        const Json value = {{"step", step}, {"value", static_cast<int>(rng() % 10000)}};
        c.records = {{id, "", value}};
        const auto accepted = store.commit(c);
        head = accepted.revision;
        values[id] = value;
        saved.emplace_back(head, values);
        admitted.push_back(c);
        auto current = store.snapshot("model");
        check(current.revision == head && current.records.size() == values.size(),
              "model current revision and cardinality");
        for (const auto &record : current.records)
            check(record.data == values.at(record.id), "model current value");
        const auto selected = static_cast<std::size_t>(rng() % saved.size());
        auto historical = store.snapshot("model", saved[selected].first);
        check(historical.records.size() == saved[selected].second.size(), "model historical cardinality");
        for (const auto &record : historical.records) {
            check(record.data == saved[selected].second.at(record.id), "model immutable historical value");
            check(store.record_version("model", record.id, record.version).data == record.data,
                  "model immutable version lookup");
        }
        const auto replay = store.commit(admitted[selected]);
        check(replay.replayed && replay.revision == saved[selected].first && replay.current_revision == head,
              "model old retry after subsequent writes");
        auto stale = request("stale-" + std::to_string(step), "0", "model");
        error("conflict", [&] { store.commit(stale); });
        check(!store.receipt("model", stale.transaction_id), "model failed CAS has no receipt");
    }
    std::vector<std::string> history;
    std::string cursor;
    do {
        auto page = store.history("model", cursor, 7);
        cursor = page.next_cursor;
        for (const auto &entry : page.entries)
            history.push_back(entry.revision);
    } while (!cursor.empty());
    check(history.size() == saved.size(), "model history has no duplicates or omissions");
    for (std::size_t i = 0; i < saved.size(); ++i)
        check(history[i] == saved[saved.size() - i - 1].first, "model history newest first");
}
} // namespace

int main() {
    const auto folder = temporary_folder("conformance");
    try {
        {
            MemoryStore store;
            common_contract(store);
            reader_contract(store);
            seeded_model(store);
        }
        {
            SQLiteStore store((folder / "contract.sqlite3").string());
            common_contract(store);
            reader_contract(store);
            seeded_model(store);
        }
        StorageLimits limits;
        limits.records_per_workspace = 2;
        limits.records_per_commit = 2;
        limits.record_bytes = 16;
        limits.snapshot_bytes = 128;
        limits.blob_bytes = 4;
        limits.blob_bytes_per_commit = 4;
        limits.blob_bytes_per_workspace = 4;
        {
            MemoryStore store(limits);
            bounded_contract(store);
        }
        {
            SQLiteStore store((folder / "bounded.sqlite3").string(), false, limits);
            bounded_contract(store);
        }
        reader_lifetime([] { return std::make_unique<MemoryStore>(); });
        reader_lifetime(
            [&] { return std::make_unique<SQLiteStore>((folder / "lifetime.sqlite3").string()); });
        StorageRequirements persistent;
        persistent.persistent = true;
        {
            MemoryStore store;
            error("unsupported_storage_capability", [&] { require_support(store.describe(), persistent); });
        }
        {
            SQLiteStore store((folder / "persistent.sqlite3").string());
            require_support(store.describe(), persistent);
        }
        StorageLimits incompatible;
        incompatible.record_bytes = 1024 * 1024 + 1;
        error("invalid_limits", [&] { MemoryStore rejected(incompatible); });
        error("invalid_limits",
              [&] { SQLiteStore rejected((folder / "invalid.sqlite3").string(), false, incompatible); });
        error("invalid_argument", [&] { SQLiteStore rejected(":memory:"); });
        cleanup(folder);
        std::cout << "Shared storage conformance: CAS/replay, snapshots, receipt lookup, model history, "
                     "limits, ranges, cancellation and reader lifetime passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Storage conformance failed: " << e.what() << "\nArtifacts: " << folder << '\n';
        return 1;
    }
}
