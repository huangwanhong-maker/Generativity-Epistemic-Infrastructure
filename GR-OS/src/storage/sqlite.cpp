#include "gr/storage/sqlite_store.hpp"
#include "internal.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <sqlite3.h>

namespace gr::storage {
using namespace detail;
namespace {
constexpr int application_id = 0x47524f53;
constexpr int schema_version = 1;
void sql_check(sqlite3 *db, int code, const char *action) {
    if (code == SQLITE_OK || code == SQLITE_ROW || code == SQLITE_DONE)
        return;
    const int base = code & 0xff;
    throw Error(base == SQLITE_BUSY || base == SQLITE_LOCKED ? "storage_busy" : "storage_error",
                std::string("SQLite operation failed: ") + action,
                {{"sqlite_code", code}, {"extended_code", sqlite3_extended_errcode(db)}});
}
void execute(sqlite3 *db, const char *sql) {
    sql_check(db, sqlite3_exec(db, sql, nullptr, nullptr, nullptr), "execute statement");
}
class Statement {
  public:
    Statement(sqlite3 *db, const char *sql) : db_(db) {
        sql_check(db_, sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr), "prepare statement");
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    Statement &bind(int index, const std::string &value) {
        require(value.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "storage_limit",
                "SQLite text exceeds supported size");
        sql_check(
            db_,
            sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT),
            "bind text");
        return *this;
    }
    Statement &bind(int index, std::int64_t value) {
        sql_check(db_, sqlite3_bind_int64(stmt_, index, value), "bind integer");
        return *this;
    }
    Statement &bind(int index, const std::vector<std::uint8_t> &value) {
        // A null address with zero length would bind SQL NULL, not an empty blob.
        static const std::uint8_t empty = 0;
        sql_check(db_,
                  sqlite3_bind_blob64(stmt_, index, value.empty() ? &empty : value.data(),
                                      static_cast<sqlite3_uint64>(value.size()), SQLITE_TRANSIENT),
                  "bind blob");
        return *this;
    }
    bool step() {
        int rc = sqlite3_step(stmt_);
        sql_check(db_, rc, "step statement");
        return rc == SQLITE_ROW;
    }
    void reset() {
        sql_check(db_, sqlite3_reset(stmt_), "reset statement");
        sql_check(db_, sqlite3_clear_bindings(stmt_), "clear statement bindings");
    }
    std::string text(int column, std::size_t limit = 4096) const {
        require(sqlite3_column_type(stmt_, column) == SQLITE_TEXT, "integrity_error",
                "Stored text has an invalid SQLite type");
        const auto size = sqlite3_column_bytes(stmt_, column);
        require(size >= 0 && static_cast<std::size_t>(size) <= limit, "storage_limit",
                "Stored text exceeds reader byte limit");
        const auto *ptr = reinterpret_cast<const char *>(sqlite3_column_text(stmt_, column));
        return std::string(ptr, static_cast<std::size_t>(size));
    }
    std::int64_t integer(int column) const {
        require(sqlite3_column_type(stmt_, column) == SQLITE_INTEGER, "integrity_error",
                "Stored integer has an invalid SQLite type");
        return sqlite3_column_int64(stmt_, column);
    }

  private:
    sqlite3 *db_;
    sqlite3_stmt *stmt_ = nullptr;
};
class Transaction {
  public:
    explicit Transaction(sqlite3 *db, bool write) : db_(db) {
        execute(db_, write ? "BEGIN IMMEDIATE" : "BEGIN");
    }
    ~Transaction() {
        if (!committed_)
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    void finish() {
        execute(db_, "COMMIT");
        committed_ = true;
    }

  private:
    sqlite3 *db_;
    bool committed_ = false;
};
Json decode(const std::string &value) {
    try {
        auto data = Json::parse(value);
        require(canonical(data) == value, "integrity_error",
                "Stored record is not in the declared canonical encoding");
        return data;
    } catch (const Json::exception &) {
        throw Error("integrity_error", "Stored record is not valid canonical JSON");
    }
}
struct ReadConnection {
    sqlite3 *db = nullptr;
    ~ReadConnection() {
        if (db) {
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            sqlite3_close_v2(db);
        }
    }
};
std::pair<BlobMetadata, std::int64_t> stored_blob_info(sqlite3 *db, const std::string &workspace,
                                                       const std::string &digest,
                                                       const StorageLimits &limits) {
    Statement s(db, "SELECT rowid,length(data),typeof(data) FROM blobs WHERE workspace=? AND digest=?");
    s.bind(1, workspace).bind(2, digest);
    require(s.step(), "not_found", "Blob does not belong to this workspace");
    require(s.text(2) == "blob", "integrity_error", "Stored blob has an invalid SQLite type");
    auto size = s.integer(1);
    require(size >= 0 && static_cast<std::uint64_t>(size) <= limits.blob_bytes &&
                size <= std::numeric_limits<int>::max(),
            "storage_limit", "Stored blob exceeds reader byte limit");
    return {{digest, static_cast<std::uint64_t>(size), "unverified"}, s.integer(0)};
}
class SQLiteBlobReader final : public BlobReader {
  public:
    SQLiteBlobReader(sqlite3 *db, std::shared_ptr<ReadConnection> connection, const std::string &workspace,
                     const std::string &digest, const StorageLimits &limits, BlobReadOptions options)
        : connection_(std::move(connection)), db_(db), options_(options) {
        validate_options(options_);
        auto [info, row] = stored_blob_info(db_, workspace, digest, limits);
        info_ = std::move(info);
        sqlite3_blob *handle = nullptr;
        sql_check(db_, sqlite3_blob_open(db_, "main", "blobs", "data", row, 0, &handle),
                  "open immutable blob");
        blob_.reset(handle);
        require(sqlite3_blob_bytes(blob_.get()) == static_cast<int>(info_.byte_length), "integrity_error",
                "Blob length changed during reader creation");
        verify_reader(*this, options_.stop);
        info_.integrity = "sha256-verified";
    }
    const BlobMetadata &metadata() const noexcept override { return info_; }
    std::size_t read(std::uint64_t offset, std::span<std::uint8_t> destination,
                     std::stop_token stop) override {
        check_cancelled(stop);
        check_cancelled(options_.stop);
        const auto n = read_size(info_, offset, destination.size(), options_.chunk_bytes);
        if (n)
            sql_check(db_,
                      sqlite3_blob_read(blob_.get(), destination.data(), static_cast<int>(n),
                                        static_cast<int>(offset)),
                      "read immutable blob range");
        return n;
    }

  private:
    // Members destroy in reverse order: blob closes before its owning connection.
    std::shared_ptr<ReadConnection> connection_;
    sqlite3 *db_;
    BlobReadOptions options_;
    BlobMetadata info_;
    std::unique_ptr<sqlite3_blob, decltype(&sqlite3_blob_close)> blob_{nullptr, sqlite3_blob_close};
};

} // namespace
struct SQLiteStore::Impl {
    sqlite3 *db = nullptr;
    std::string path;
    bool read_only;
    StorageLimits limits;
    std::mutex mutex;
    ~Impl() {
        if (db)
            sqlite3_close_v2(db);
    }
    void writable() const { require(!read_only, "read_only", "Store was opened read-only"); }
    std::string get_head(const std::string &workspace) {
        identifier(workspace, "Workspace");
        Statement s(db, "SELECT head,(SELECT COALESCE(MAX(seq),0) FROM revisions WHERE "
                        "workspace=workspaces.id) FROM workspaces WHERE id=?");
        s.bind(1, workspace);
        require(s.step(), "not_found", "Workspace does not exist");
        auto selected = s.text(0);
        // The active statement pins this read view while sequence() checks the
        // selected row. A stale/corrupt head cannot masquerade as current state.
        const auto latest = s.integer(1);
        require(sequence(workspace, selected) == latest, "integrity_error",
                "Workspace head is not the latest ledger revision");
        return selected;
    }
    std::int64_t sequence(const std::string &workspace, const std::string &revision) {
        if (revision == "0")
            return 0;
        Statement s(db, "SELECT revision,parent,transaction_id,actor,reason,request_digest,seq FROM "
                        "revisions WHERE workspace=? AND revision=?");
        s.bind(1, workspace).bind(2, revision);
        require(s.step(), "not_found", "Revision does not belong to this workspace");
        HistoryEntry entry{s.text(0), s.text(1), s.text(2), s.text(3), s.text(4), s.text(5)};
        verify_entry(workspace, entry);
        const auto seq = s.integer(6);
        require(seq > 0 && seq < std::numeric_limits<std::int64_t>::max(), "integrity_error",
                "Stored ledger sequence is invalid or exhausted");
        if (seq == 1)
            require(entry.parent == "0", "integrity_error", "First revision has an invalid parent");
        else {
            Statement parent(db, "SELECT revision FROM revisions WHERE workspace=? AND seq=?");
            parent.bind(1, workspace).bind(2, seq - 1);
            require(parent.step() && parent.text(0) == entry.parent, "integrity_error",
                    "Ledger parent does not precede its child");
        }
        return seq;
    }
    Snapshot read_snapshot(const std::string &workspace, const std::string &selected) {
        Snapshot out{workspace, selected, {}};
        if (selected == "0")
            return out;
        (void)sequence(workspace, selected);
        Statement s(db, "SELECT s.record_id,s.version,v.data FROM snapshot_records s JOIN record_versions v "
                        "ON v.workspace=s.workspace AND v.record_id=s.record_id AND v.version=s.version "
                        "WHERE s.workspace=? AND s.revision=? ORDER BY s.record_id");
        s.bind(1, workspace).bind(2, selected);
        std::size_t total = 0;
        while (s.step()) {
            require(out.records.size() < limits.records_per_workspace, "storage_limit",
                    "Snapshot exceeds record count limit");
            auto encoded = s.text(2, limits.record_bytes);
            require(encoded.size() <= limits.record_bytes, "storage_limit",
                    "Stored record exceeds reader byte limit");
            add_size(total, encoded.size(), limits.snapshot_bytes, "Snapshot exceeds byte limit");
            StoredRecord r{s.text(0), s.text(1), decode(encoded)};
            // decode already checked exact canonical bytes. Hash those bytes
            // directly rather than serializing the same document again.
            require(r.version == "sha256:" + sha256(encoded), "integrity_error",
                    "Stored record content failed integrity verification");
            out.records.push_back(std::move(r));
        }
        Statement count(db, "SELECT count(*) FROM snapshot_records WHERE workspace=? AND revision=?");
        count.bind(1, workspace).bind(2, selected);
        count.step();
        require(count.integer(0) == static_cast<std::int64_t>(out.records.size()), "integrity_error",
                "Snapshot references unavailable record versions");
        Statement inventory(db, "SELECT snapshot_digest FROM revisions WHERE workspace=? AND revision=?");
        inventory.bind(1, workspace).bind(2, selected);
        require(inventory.step() && inventory.text(0) == snapshot_digest(out), "integrity_error",
                "Snapshot membership failed integrity verification");
        return out;
    }
    void verify_blob(const std::string &workspace, const std::string &digest) {
        // Uses the current write transaction, including newly inserted blobs.
        SQLiteBlobReader reader(db, nullptr, workspace, digest, limits, {});
    }
};

SQLiteStore::SQLiteStore(const std::string &path, bool read_only, StorageLimits limits)
    : impl_(std::make_unique<Impl>()) {
    validate_limits(limits);
    require(!path.empty() && path != ":memory:" && !path.starts_with("file:") &&
                path.find('\0') == std::string::npos,
            "invalid_argument", "SQLite persistence requires a local file path, not a URI");
    impl_->read_only = read_only;
    impl_->limits = limits;
    const int flags = (read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) |
                      SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(path.c_str(), &impl_->db, flags, nullptr);
    if (rc != SQLITE_OK)
        throw Error("storage_error", "Could not open SQLite store", {{"sqlite_code", rc}});
    impl_->path = sqlite3_db_filename(impl_->db, "main");
    sqlite3_extended_result_codes(impl_->db, 1);
    sqlite3_busy_timeout(impl_->db, 5000);
    execute(impl_->db, "PRAGMA foreign_keys=ON");
    // Opening a foreign or newer database must never silently initialize it.
    int app_id, version;
    {
        Statement s(impl_->db, "PRAGMA application_id");
        s.step();
        app_id = static_cast<int>(s.integer(0));
    }
    {
        Statement s(impl_->db, "PRAGMA user_version");
        s.step();
        version = static_cast<int>(s.integer(0));
    }
    if (app_id == 0 && version == 0) {
        impl_->writable();
        Transaction transaction(impl_->db, true);
        // Recheck after obtaining the write lock: another opener may initialize.
        {
            Statement s(impl_->db, "PRAGMA application_id");
            s.step();
            app_id = static_cast<int>(s.integer(0));
        }
        {
            Statement s(impl_->db, "PRAGMA user_version");
            s.step();
            version = static_cast<int>(s.integer(0));
        }
        if (app_id == 0 && version == 0) {
            Statement objects(impl_->db, "SELECT count(*) FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'");
            objects.step();
            require(objects.integer(0) == 0, "storage_version",
                    "Refusing to initialize an unrelated SQLite database");
            execute(impl_->db, R"SQL(
CREATE TABLE store_meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
INSERT INTO store_meta VALUES('contract','gr.storage/1');
INSERT INTO store_meta VALUES('canonicalization','gr.canonical-json/1');
CREATE TABLE workspaces(id TEXT PRIMARY KEY,head TEXT NOT NULL);
CREATE TABLE revisions(workspace TEXT NOT NULL,revision TEXT NOT NULL,seq INTEGER NOT NULL,parent TEXT NOT NULL,transaction_id TEXT NOT NULL,actor TEXT NOT NULL,reason TEXT NOT NULL,request_digest TEXT NOT NULL,snapshot_digest TEXT NOT NULL,PRIMARY KEY(workspace,revision),UNIQUE(workspace,seq),UNIQUE(workspace,transaction_id),FOREIGN KEY(workspace) REFERENCES workspaces(id));
CREATE TABLE record_versions(workspace TEXT NOT NULL,record_id TEXT NOT NULL,version TEXT NOT NULL,data TEXT NOT NULL,PRIMARY KEY(workspace,record_id,version),FOREIGN KEY(workspace) REFERENCES workspaces(id));
CREATE TABLE snapshot_records(workspace TEXT NOT NULL,revision TEXT NOT NULL,record_id TEXT NOT NULL,version TEXT NOT NULL,PRIMARY KEY(workspace,revision,record_id),FOREIGN KEY(workspace,revision) REFERENCES revisions(workspace,revision),FOREIGN KEY(workspace,record_id,version) REFERENCES record_versions(workspace,record_id,version));
CREATE TABLE blobs(workspace TEXT NOT NULL,digest TEXT NOT NULL,data BLOB NOT NULL,PRIMARY KEY(workspace,digest),FOREIGN KEY(workspace) REFERENCES workspaces(id));
CREATE TABLE revision_blobs(workspace TEXT NOT NULL,revision TEXT NOT NULL,digest TEXT NOT NULL,PRIMARY KEY(workspace,revision,digest),FOREIGN KEY(workspace,revision) REFERENCES revisions(workspace,revision),FOREIGN KEY(workspace,digest) REFERENCES blobs(workspace,digest));
PRAGMA application_id=1196576595;
PRAGMA user_version=1;
)SQL");
            app_id = application_id;
            version = schema_version;
        }
        require(app_id == application_id && version == schema_version, "storage_version",
                "Unsupported SQLite storage version");
        transaction.finish();
    }
    require(app_id == application_id && version == schema_version, "storage_version",
            "Unsupported SQLite storage version");
    {
        Statement s(impl_->db, "SELECT value FROM store_meta WHERE key='contract'");
        require(s.step() && s.text(0) == "gr.storage/1", "storage_version",
                "Unsupported SQLite storage contract");
    }
    {
        Statement s(impl_->db, "SELECT value FROM store_meta WHERE key='canonicalization'");
        require(s.step() && s.text(0) == "gr.canonical-json/1", "storage_version",
                "Unsupported SQLite canonicalization contract");
    }
    if (!read_only) {
        execute(impl_->db, "PRAGMA journal_mode=WAL");
        execute(impl_->db, "PRAGMA synchronous=FULL");
    } else
        execute(impl_->db, "PRAGMA query_only=ON");
}
SQLiteStore::~SQLiteStore() = default;
StorageCapabilities SQLiteStore::describe() const {
    return capabilities_for("sqlite", true, impl_->read_only, impl_->limits);
}
std::string SQLiteStore::coordination_identity() const {
    // SQLite returns an absolute filename. Alternate aliases can miss this
    // optimization; physical workspace CAS still protects every publication.
    return "sqlite:" + impl_->path;
}
void SQLiteStore::create_workspace(const std::string &workspace) {
    identifier(workspace, "Workspace");
    std::lock_guard lock(impl_->mutex);
    impl_->writable();
    Transaction tx(impl_->db, true);
    {
        Statement find(impl_->db, "SELECT 1 FROM workspaces WHERE id=?");
        find.bind(1, workspace);
        require(!find.step(), "conflict", "Workspace already exists");
    }
    Statement insert(impl_->db, "INSERT INTO workspaces(id,head) VALUES(?,'0')");
    insert.bind(1, workspace);
    insert.step();
    tx.finish();
}
std::string SQLiteStore::head(const std::string &workspace) {
    std::lock_guard lock(impl_->mutex);
    return impl_->get_head(workspace);
}
Snapshot SQLiteStore::snapshot(const std::string &workspace, const std::string &revision) {
    std::lock_guard lock(impl_->mutex);
    Transaction tx(impl_->db, false);
    auto current = impl_->get_head(workspace);
    auto result = impl_->read_snapshot(workspace, revision.empty() ? current : revision);
    tx.finish();
    return result;
}
StoredRecord SQLiteStore::record_version(const std::string &workspace, const std::string &id,
                                         const std::string &version) {
    identifier(id, "Record id");
    digest_id(version);
    std::lock_guard lock(impl_->mutex);
    (void)impl_->get_head(workspace);
    Statement s(impl_->db,
                "SELECT data FROM record_versions WHERE workspace=? AND record_id=? AND version=?");
    s.bind(1, workspace).bind(2, id).bind(3, version);
    require(s.step(), "not_found", "Record version does not belong to this workspace and identity");
    auto encoded = s.text(0, impl_->limits.record_bytes);
    StoredRecord record{id, version, decode(encoded)};
    require(version == "sha256:" + sha256(encoded), "integrity_error",
            "Stored record content failed integrity verification");
    return record;
}
Result SQLiteStore::commit(const Commit &request) {
    const auto prepared = prepare(request, impl_->limits);
    std::lock_guard lock(impl_->mutex);
    impl_->writable();
    Transaction tx(impl_->db, true);
    const auto current = impl_->get_head(request.workspace);
    {
        Statement replay(impl_->db, "SELECT revision,parent,actor,request_digest FROM revisions WHERE "
                                    "workspace=? AND transaction_id=?");
        replay.bind(1, request.workspace).bind(2, request.transaction_id);
        if (replay.step()) {
            (void)impl_->sequence(request.workspace, replay.text(0));
            require(replay.text(2) == request.actor && replay.text(3) == prepared.request_digest,
                    "transaction_conflict", "Transaction id was already used by a different request");
            Result result{replay.text(0), replay.text(1), current, true};
            tx.finish();
            return result;
        }
    }
    require(current == request.expected_revision, "conflict", "Expected revision is stale");
    const auto previous = impl_->read_snapshot(request.workspace, current);
    std::map<std::string, StoredRecord> records;
    for (const auto &r : previous.records)
        records.emplace(r.id, r);
    for (const auto &r : prepared.records)
        records.insert_or_assign(r.id, r);
    Snapshot candidate{request.workspace, "", {}};
    for (const auto &[id, r] : records) {
        (void)id;
        candidate.records.push_back(r);
    }
    verify_snapshot(candidate, impl_->limits);
    for (const auto &b : request.blobs) {
        Statement exists(impl_->db, "SELECT 1 FROM blobs WHERE workspace=? AND digest=?");
        exists.bind(1, request.workspace).bind(2, b.digest);
        if (exists.step())
            impl_->verify_blob(request.workspace, b.digest);
        else {
            Statement insert(impl_->db, "INSERT INTO blobs(workspace,digest,data) VALUES(?,?,?)");
            insert.bind(1, request.workspace).bind(2, b.digest).bind(3, b.bytes);
            insert.step();
        }
    }
    {
        Statement sum(impl_->db, "SELECT COALESCE(sum(length(data)),0) FROM blobs WHERE workspace=?");
        sum.bind(1, request.workspace);
        sum.step();
        require(sum.integer(0) >= 0 &&
                    static_cast<std::uint64_t>(sum.integer(0)) <= impl_->limits.blob_bytes_per_workspace,
                "storage_limit", "Retained blob budget exceeded");
    }
    for (const auto &digest : request.required_blob_digests)
        impl_->verify_blob(request.workspace, digest);
    auto entry = entry_for(request, prepared);
    const auto previous_sequence = impl_->sequence(request.workspace, current);
    require(previous_sequence < std::numeric_limits<std::int64_t>::max() - 1, "storage_limit",
            "Ledger sequence is exhausted");
    const auto seq = previous_sequence + 1;
    {
        Statement insert(impl_->db, "INSERT INTO "
                                    "revisions(workspace,revision,seq,parent,transaction_id,actor,reason,"
                                    "request_digest,snapshot_digest) VALUES(?,?,?,?,?,?,?,?,?)");
        insert.bind(1, request.workspace)
            .bind(2, entry.revision)
            .bind(3, seq)
            .bind(4, entry.parent)
            .bind(5, entry.transaction_id)
            .bind(6, entry.actor)
            .bind(7, entry.reason)
            .bind(8, entry.request_digest)
            .bind(9, snapshot_digest(candidate));
        insert.step();
    }
    for (const auto &r : prepared.records) {
        Statement existing(
            impl_->db, "SELECT data FROM record_versions WHERE workspace=? AND record_id=? AND version=?");
        existing.bind(1, request.workspace).bind(2, r.id).bind(3, r.version);
        if (existing.step())
            require(existing.text(0, impl_->limits.record_bytes) == prepared.encoded.at(r.id),
                    "integrity_error", "Immutable version content differs");
        else {
            Statement insert(impl_->db,
                             "INSERT INTO record_versions(workspace,record_id,version,data) VALUES(?,?,?,?)");
            insert.bind(1, request.workspace)
                .bind(2, r.id)
                .bind(3, r.version)
                .bind(4, prepared.encoded.at(r.id));
            insert.step();
        }
    }
    {
        Statement insert(
            impl_->db, "INSERT INTO snapshot_records(workspace,revision,record_id,version) VALUES(?,?,?,?)");
        for (const auto &[id, r] : records) {
            insert.bind(1, request.workspace).bind(2, entry.revision).bind(3, id).bind(4, r.version);
            insert.step();
            insert.reset();
        }
    }
    if (current != "0") {
        Statement inherit(impl_->db,
                          "INSERT INTO revision_blobs(workspace,revision,digest) SELECT workspace,?,digest "
                          "FROM revision_blobs WHERE workspace=? AND revision=?");
        inherit.bind(1, entry.revision).bind(2, request.workspace).bind(3, current);
        inherit.step();
    }
    {
        Statement insert(impl_->db,
                         "INSERT OR IGNORE INTO revision_blobs(workspace,revision,digest) VALUES(?,?,?)");
        for (const auto &digest : request.required_blob_digests) {
            insert.bind(1, request.workspace).bind(2, entry.revision).bind(3, digest);
            insert.step();
            insert.reset();
        }
    }
    Statement advance(impl_->db, "UPDATE workspaces SET head=? WHERE id=? AND head=?");
    advance.bind(1, entry.revision).bind(2, request.workspace).bind(3, current);
    advance.step();
    require(sqlite3_changes(impl_->db) == 1, "conflict", "Workspace head changed during publication");
    tx.finish();
    return {entry.revision, entry.parent, entry.revision, false};
}
HistoryPage SQLiteStore::history(const std::string &workspace, const std::string &before, std::size_t limit) {
    history_limit(limit);
    std::lock_guard lock(impl_->mutex);
    Transaction tx(impl_->db, false);
    (void)impl_->get_head(workspace);
    auto bound =
        before.empty() ? std::numeric_limits<std::int64_t>::max() : impl_->sequence(workspace, before);
    require(before != "0", "not_found", "Empty revision is not a history entry");
    Statement s(impl_->db, "SELECT revision,parent,transaction_id,actor,reason,request_digest FROM revisions "
                           "WHERE workspace=? AND seq<? ORDER BY seq DESC LIMIT ?");
    s.bind(1, workspace).bind(2, bound).bind(3, static_cast<std::int64_t>(limit + 1));
    HistoryPage out;
    while (s.step()) {
        if (out.entries.size() == limit) {
            out.next_cursor = out.entries.back().revision;
            break;
        }
        HistoryEntry entry{s.text(0), s.text(1), s.text(2), s.text(3), s.text(4), s.text(5)};
        (void)impl_->sequence(workspace, entry.revision);
        out.entries.push_back(std::move(entry));
    }
    tx.finish();
    return out;
}

std::optional<TransactionReceipt> SQLiteStore::receipt(const std::string &workspace,
                                                       const std::string &transaction_id) {
    identifier(transaction_id, "Transaction id");
    std::lock_guard lock(impl_->mutex);
    Transaction tx(impl_->db, false);
    const auto current = impl_->get_head(workspace);
    Statement s(impl_->db, "SELECT revision,parent,transaction_id,actor,reason,request_digest FROM revisions "
                           "WHERE workspace=? AND transaction_id=?");
    s.bind(1, workspace).bind(2, transaction_id);
    if (!s.step()) {
        tx.finish();
        return std::nullopt;
    }
    HistoryEntry entry{s.text(0), s.text(1), s.text(2), s.text(3), s.text(4), s.text(5)};
    (void)impl_->sequence(workspace, entry.revision);
    TransactionReceipt result{std::move(entry), current};
    tx.finish();
    return result;
}

BlobMetadata SQLiteStore::blob_info(const std::string &workspace, const std::string &digest) {
    digest_id(digest);
    std::lock_guard lock(impl_->mutex);
    (void)impl_->get_head(workspace);
    return stored_blob_info(impl_->db, workspace, digest, impl_->limits).first;
}

std::unique_ptr<BlobReader> SQLiteStore::open_blob(const std::string &workspace, const std::string &digest,
                                                   BlobReadOptions options) {
    identifier(workspace, "Workspace");
    digest_id(digest);
    validate_options(options);
    // Reopen the resolved filename, never an identifier or a caller-controlled URI.
    auto connection = std::make_shared<ReadConnection>();
    const int rc = sqlite3_open_v2(impl_->path.c_str(), &connection->db,
                                   SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
    sql_check(connection->db, rc, "open blob reader connection");
    sqlite3_extended_result_codes(connection->db, 1);
    sqlite3_busy_timeout(connection->db, 5000);
    execute(connection->db, "PRAGMA query_only=ON");
    execute(connection->db, "BEGIN");
    // Validate file identity again: external replacement must not silently serve an unrelated database.
    {
        Statement s(connection->db, "PRAGMA application_id");
        s.step();
        require(s.integer(0) == application_id, "storage_version",
                "Blob reader opened an unrelated database");
    }
    {
        Statement s(connection->db, "PRAGMA user_version");
        s.step();
        require(s.integer(0) == schema_version, "storage_version", "Unsupported blob reader storage version");
    }
    {
        Statement s(connection->db, "SELECT 1 FROM workspaces WHERE id=?");
        s.bind(1, workspace);
        require(s.step(), "not_found", "Workspace does not exist");
    }
    return std::make_unique<SQLiteBlobReader>(connection->db, connection, workspace, digest, impl_->limits,
                                              options);
}

} // namespace gr::storage
