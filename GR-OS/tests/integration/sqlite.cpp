#include "../conformance/storage_support.hpp"
#include <sqlite3.h>
namespace {
void raw_sql(const std::filesystem::path &path, const char *sql) {
    sqlite3 *db = nullptr;
    check(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK, "open test fault injector");
    const auto rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    std::string message = sqlite3_errmsg(db);
    sqlite3_close(db);
    check(rc == SQLITE_OK, "fault injection SQL: " + message);
}
void rollback_and_reopen(const std::filesystem::path &path) {
    Commit c = request("durable");
    c.records = {{"r", "", Json{{"value", "preserved"}}}};
    const auto b = material({1, 2, 3});
    c.blobs = {b};
    c.required_blob_digests = {b.digest};
    std::string revision;
    {
        SQLiteStore store(path.string());
        store.create_workspace("test");
        raw_sql(path, "CREATE TRIGGER inject_failure BEFORE UPDATE OF head ON workspaces BEGIN SELECT "
                      "RAISE(ABORT,'injected pre-publication failure'); END;");
        error("storage_error", [&] { store.commit(c); });
        check(store.head("test") == "0" && store.history("test").entries.empty(),
              "late SQL error rolled back revision/receipt");
        error("not_found", [&] { store.blob("test", b.digest); });
        error("not_found", [&] { store.record_version("test", "r", content_version(c.records[0].data)); });
        raw_sql(path, "DROP TRIGGER inject_failure;");
        revision = store.commit(c).revision;
    }
    {
        SQLiteStore reopened(path.string());
        check(reopened.head("test") == revision && reopened.commit(c).replayed,
              "durable receipt survives reopen");
        check(reopened.blob("test", b.digest) == b.bytes, "blob survives reopen");
    }
    {
        SQLiteStore readonly(path.string(), true);
        check(readonly.snapshot("test").revision == revision, "read-only snapshot");
        check(readonly.receipt("test", "durable")->entry.revision == revision, "read-only receipt lookup");
        StorageRequirements writable;
        writable.writable = true;
        error("unsupported_storage_capability", [&] { require_support(readonly.describe(), writable); });
        error("read_only", [&] { readonly.commit(c); });
        error("read_only", [&] { readonly.create_workspace("blocked"); });
    }
}
void concurrent_connections(const std::filesystem::path &path) {
    {
        SQLiteStore setup(path.string());
        setup.create_workspace("test");
    }
    std::barrier start(3);
    std::atomic<int> successes = 0, conflicts = 0, failures = 0;
    auto write = [&](int id) {
        try {
            SQLiteStore store(path.string());
            start.arrive_and_wait();
            auto c = request("writer-" + std::to_string(id));
            c.records = {{"r", "", id}};
            try {
                (void)store.commit(c);
                ++successes;
            } catch (const gr::Error &e) {
                if (e.code == "conflict")
                    ++conflicts;
                else
                    ++failures;
            }
        } catch (...) {
            ++failures;
            start.arrive_and_drop();
        }
    };
    std::thread a(write, 1), b(write, 2);
    start.arrive_and_wait();
    a.join();
    b.join();
    check(successes == 1 && conflicts == 1 && failures == 0,
          "concurrent independent connections publish exactly one CAS winner");
    SQLiteStore store(path.string());
    check(store.history("test").entries.size() == 1, "loser leaves no receipt");
    auto shared = request("identical", store.head("test"));
    shared.records = {{"r", "", "same request"}};
    std::barrier replay_start(3);
    successes = 0;
    std::atomic<int> replays = 0;
    failures = 0;
    auto retry = [&] {
        try {
            SQLiteStore connection(path.string());
            replay_start.arrive_and_wait();
            const auto result = connection.commit(shared);
            if (result.replayed)
                ++replays;
            else
                ++successes;
        } catch (...) {
            ++failures;
            replay_start.arrive_and_drop();
        }
    };
    std::thread c(retry), d(retry);
    replay_start.arrive_and_wait();
    c.join();
    d.join();
    check(successes == 1 && replays == 1 && failures == 0,
          "concurrent identical requests publish once then replay");
    check(store.history("test").entries.size() == 2, "duplicate concurrent request adds one receipt");
}
void corruption_and_versions(const std::filesystem::path &folder) {
    auto path = folder / "corrupt-record.sqlite3";
    {
        SQLiteStore store(path.string());
        store.create_workspace("test");
        auto c = request("one");
        c.records = {{"r", "", "value"}};
        store.commit(c);
        raw_sql(path, "UPDATE record_versions SET data='\"tampered\"';");
        error("integrity_error", [&] { store.snapshot("test"); });
        error("integrity_error", [&] { store.record_version("test", "r", content_version("value")); });
    }
    path = folder / "corrupt-membership.sqlite3";
    {
        SQLiteStore store(path.string());
        store.create_workspace("test");
        auto c = request("one");
        c.records = {{"r", "", "value"}};
        store.commit(c);
        raw_sql(path, "DELETE FROM snapshot_records;");
        error("integrity_error", [&] { store.snapshot("test"); });
    }
    path = folder / "corrupt-blob.sqlite3";
    {
        SQLiteStore store(path.string());
        store.create_workspace("test");
        auto c = request("one");
        const auto b = material({1, 2, 3});
        c.blobs = {b};
        c.required_blob_digests = {b.digest};
        auto r = store.commit(c);
        raw_sql(path, "UPDATE blobs SET data=X'010204';");
        error("integrity_error", [&] { store.blob("test", b.digest); });
        auto next = request("bad-closure", r.revision);
        next.required_blob_digests = {b.digest};
        error("integrity_error", [&] { store.commit(next); });
        check(store.head("test") == r.revision, "corrupt closure cannot advance history");
    }
    path = folder / "future-version.sqlite3";
    {
        SQLiteStore store(path.string());
    }
    raw_sql(path, "PRAGMA user_version=99;");
    error("storage_version", [&] { SQLiteStore rejected(path.string()); });
    path = folder / "foreign.sqlite3";
    raw_sql(path, "CREATE TABLE foreign_data(value TEXT); INSERT INTO foreign_data VALUES('preserve');");
    error("storage_version", [&] { SQLiteStore rejected(path.string()); });
}

void pinned_reader_and_corruption(const std::filesystem::path &path) {
    const auto original = material({0, 1, 2, 3, 4, 5});
    std::unique_ptr<BlobReader> reader;
    {
        SQLiteStore store(path.string());
        store.create_workspace("test");
        auto c = request("pinned");
        c.blobs = {original};
        c.required_blob_digests = {original.digest};
        const auto result = store.commit(c);
        reader = store.open_blob("test", original.digest, {2, {}});
        // WAL permits a writer while this independent reader pins its read view.
        auto next = request("later", result.revision);
        next.records = {{"r", "", "later"}};
        store.commit(next);
        raw_sql(path, "UPDATE blobs SET data=X'090909090909';");
        check(store.blob_info("test", original.digest).integrity == "unverified",
              "metadata never labels a corrupt object verified");
        error("integrity_error", [&] { store.open_blob("test", original.digest); });
    }
    std::vector<std::uint8_t> actual(original.bytes.size());
    for (std::size_t offset = 0; offset < actual.size();)
        offset += reader->read(offset, std::span<std::uint8_t>(actual).subspan(offset));
    check(actual == original.bytes,
          "reader pins the verified original bytes across writes and store destruction");
}

void ledger_guards(const std::filesystem::path &folder) {
    const auto path = folder / "ledger.sqlite3";
    SQLiteStore store(path.string());
    store.create_workspace("test");
    auto first = request("first");
    first.records = {{"record", "", std::string(64, 'x')}};
    const auto one = store.commit(first);
    const auto two = store.commit(request("second", one.revision));
    raw_sql(path, "UPDATE revisions SET parent='bad-parent' WHERE transaction_id='second';");
    error("integrity_error", [&] { store.head("test"); });
    error("integrity_error", [&] { store.receipt("test", "second"); });
    error("integrity_error", [&] { store.history("test"); });
    // Restore via exact known identity, then damage sequence order independently.
    const auto restore = "UPDATE revisions SET parent='" + one.revision + "' WHERE transaction_id='second';";
    raw_sql(path, restore.c_str());
    raw_sql(path, "UPDATE revisions SET seq=3 WHERE transaction_id='second';");
    error("integrity_error", [&] { store.snapshot("test"); });
    raw_sql(path, "UPDATE revisions SET seq=9223372036854775807 WHERE transaction_id='second';");
    error("integrity_error", [&] { store.commit(request("overflow", two.revision)); });
    raw_sql(path, "UPDATE revisions SET seq=2 WHERE transaction_id='second';");
    check(store.head("test") == two.revision,
          "valid legacy ledger remains readable after restoring injected corruption");
    raw_sql(path, "UPDATE workspaces SET head='0';");
    error("integrity_error", [&] { store.head("test"); });
    const auto old_head = "UPDATE workspaces SET head='" + one.revision + "';";
    raw_sql(path, old_head.c_str());
    error("integrity_error", [&] { store.head("test"); });
    const auto restore_head = "UPDATE workspaces SET head='" + two.revision + "';";
    raw_sql(path, restore_head.c_str());
    StorageLimits small;
    small.record_bytes = 16;
    SQLiteStore bounded(path.string(), true, small);
    error("storage_limit", [&] { bounded.snapshot("test"); });
    error("storage_limit",
          [&] { bounded.record_version("test", "record", content_version(first.records[0].data)); });
    // V1 does not retain the canonical request summary. Read attribution is not
    // authenticated by the revision-link checks; this limitation is deliberate.
    raw_sql(path, "UPDATE revisions SET actor='external mutation' WHERE transaction_id='first';");
    check(store.receipt("test", "first")->entry.actor == "external mutation",
          "v1 attribution integrity limitation is explicit");
}

void large_blob_bounded_reads(const std::filesystem::path &path) {
    std::vector<std::uint8_t> data(2 * 1024 * 1024 + 3);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<std::uint8_t>(i % 251);
    const auto blob = material(std::move(data));
    SQLiteStore store(path.string());
    store.create_workspace("test");
    auto c = request("large");
    c.blobs = {blob};
    c.required_blob_digests = {blob.digest};
    store.commit(c);
    auto reader = store.open_blob("test", blob.digest, {257, {}});
    std::array<std::uint8_t, 1024> buffer{};
    for (const std::size_t offset : {std::size_t{0}, std::size_t{65531}, blob.bytes.size() - 4}) {
        const auto n = reader->read(offset, buffer);
        check(n == std::min<std::size_t>(257, blob.bytes.size() - offset), "bounded SQLite range count");
        check(std::equal(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(n),
                         blob.bytes.begin() + static_cast<std::ptrdiff_t>(offset)),
              "SQLite range bytes across chunk/page boundaries");
    }
}
} // namespace

int main() {
    const auto folder = temporary_folder("sqlite");
    try {
        rollback_and_reopen(folder / "recovery.sqlite3");
        concurrent_connections(folder / "concurrent.sqlite3");
        corruption_and_versions(folder);
        pinned_reader_and_corruption(folder / "reader-pin.sqlite3");
        ledger_guards(folder);
        large_blob_bounded_reads(folder / "large.sqlite3");
        cleanup(folder);
        std::cout << "SQLite integration: rollback/reopen, concurrent CAS/replay, corruption, pinned WAL "
                     "readers, ledger guards and bounded object ranges passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "SQLite integration failed: " << e.what() << "\nArtifacts: " << folder << '\n';
        return 1;
    }
}
