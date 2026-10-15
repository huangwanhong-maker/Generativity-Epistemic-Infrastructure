#pragma once
#include "gr/storage/blob_reader.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace gr::storage {
struct StoredRecord {
    std::string id;
    std::string version;
    Json data;
};
struct Blob {
    std::string digest;
    std::vector<std::uint8_t> bytes;
};
struct Snapshot {
    std::string workspace;
    std::string revision;
    std::vector<StoredRecord> records;
};
struct Commit {
    std::string workspace, expected_revision, transaction_id, actor, reason;
    std::vector<StoredRecord> records; // Upserts; omitted identities remain present.
    std::vector<Blob> blobs;           // Bounded complete uploads; no staged upload protocol.
    std::vector<std::string> required_blob_digests;
};
struct Result {
    std::string revision, parent, current_revision;
    bool replayed = false;
};
struct HistoryEntry {
    std::string revision, parent, transaction_id, actor, reason, request_digest;
};
struct HistoryPage {
    std::vector<HistoryEntry> entries;
    std::string next_cursor;
};
struct TransactionReceipt {
    HistoryEntry entry;
    std::string current_revision;
};
struct StorageLimits {
    std::size_t records_per_workspace = 10000, records_per_commit = 1000;
    std::size_t record_bytes = 1024 * 1024, snapshot_bytes = 32 * 1024 * 1024;
    std::size_t blob_bytes = 16 * 1024 * 1024, blobs_per_commit = 128;
    std::size_t blob_bytes_per_commit = 64 * 1024 * 1024;
    std::size_t blob_bytes_per_workspace = 100 * 1024 * 1024;
};
enum class HistoryModel { linear, dag };
enum class Durability { volatile_memory, local_full_sync };
enum class AtomicityScope { workspace };
struct StorageCapabilities {
    std::string contract = "gr.storage/1", backend;
    Durability durability = Durability::volatile_memory;
    AtomicityScope atomicity_scope = AtomicityScope::workspace;
    HistoryModel history_model = HistoryModel::linear;
    bool read_only = false;
    bool atomic_records_blobs_receipts = true, snapshot_consistency = true;
    bool immutable_versions = true, conditional_commit = true;
    bool verified_blob_ranges = true, blob_read_cancellation = true;
    bool reader_outlives_store = true;
    std::string concurrency;
    StorageLimits limits;
    Json to_json() const;
};
struct StorageRequirements {
    bool persistent = false, writable = false, atomic_commit = false;
    bool consistent_snapshots = false, immutable_versions = false, conditional_commit = false;
    bool verified_blob_ranges = false, blob_read_cancellation = false, reader_outlives_store = false;
    std::optional<HistoryModel> history_model;
};
// Checks the already-selected adapter, without a planner, loader or fallback.
void require_support(const StorageCapabilities &, const StorageRequirements &);
// Validate and hash the exact physical publication request without publishing.
// Coordinators use this identity to verify a recovered durable receipt.
std::string request_digest(const Commit &, StorageLimits limits = {});

// Transactional history, not a universal file/chunk/columnar storage port.
// Attribution is caller-supplied; authorization and payload meaning are external.
class IStore {
  public:
    virtual ~IStore() = default;
    virtual StorageCapabilities describe() const = 0;
    // Optional process-local admission hint. Never a substitute for physical
    // CAS. Wrappers may forward it; an empty hint coordinates only this object.
    virtual std::string coordination_identity() const { return {}; }
    Json capabilities() const { return describe().to_json(); }
    virtual void create_workspace(const std::string &) = 0;
    virtual std::string head(const std::string &) = 0;
    virtual Snapshot snapshot(const std::string &, const std::string &revision = "") = 0;
    virtual StoredRecord record_version(const std::string &, const std::string &, const std::string &) = 0;
    virtual Result commit(const Commit &) = 0;
    virtual HistoryPage history(const std::string &, const std::string &before = "",
                                std::size_t limit = 100) = 0;
    virtual std::optional<TransactionReceipt> receipt(const std::string &,
                                                      const std::string &transaction_id) = 0;
    virtual BlobMetadata blob_info(const std::string &, const std::string &digest) = 0;
    virtual std::unique_ptr<BlobReader> open_blob(const std::string &, const std::string &digest,
                                                  BlobReadOptions options = {}) = 0;
    std::vector<std::uint8_t> blob(const std::string &, const std::string &digest);
};
} // namespace gr::storage
