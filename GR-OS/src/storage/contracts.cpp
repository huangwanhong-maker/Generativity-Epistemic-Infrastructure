#include "internal.hpp"
#include <algorithm>
#include <limits>
#include <picosha2.h>
#include <set>

namespace gr::storage::detail {
void validate_limits(const StorageLimits &l) {
    require(l.record_bytes <= 1024 * 1024 && l.records_per_commit <= 1000 && l.blobs_per_commit <= 128,
            "invalid_limits", "Storage configuration exceeds the canonical metadata contract");
    require(l.blob_bytes <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
                l.blob_bytes_per_workspace <=
                    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
            "invalid_limits", "Storage configuration exceeds supported object/aggregate sizes");
}
StorageCapabilities capabilities_for(const char *backend, bool persistent, bool read_only,
                                     const StorageLimits &limits) {
    StorageCapabilities c;
    c.backend = backend;
    c.read_only = read_only;
    c.limits = limits;
    c.durability = persistent ? Durability::local_full_sync : Durability::volatile_memory;
    c.concurrency = persistent ? "independent local connections; one SQLite writer; pinned readers"
                               : "thread-safe serialized store calls";
    return c;
}
void check_cancelled(std::stop_token token) {
    require(!token.stop_requested(), "storage_cancelled", "Blob read was cancelled");
}
void validate_options(const BlobReadOptions &options) {
    require(options.chunk_bytes > 0 && options.chunk_bytes <= 1024 * 1024, "invalid_argument",
            "Blob chunks must be between one byte and one MiB");
    check_cancelled(options.stop);
}
std::size_t read_size(const BlobMetadata &info, std::uint64_t offset, std::size_t destination,
                      std::size_t chunk) {
    require(offset <= info.byte_length, "blob_range", "Blob offset is beyond the end of the object");
    return static_cast<std::size_t>(std::min<std::uint64_t>({info.byte_length - offset, destination, chunk}));
}
void verify_reader(BlobReader &reader, std::stop_token stop) {
    picosha2::hash256_one_by_one hasher;
    std::vector<std::uint8_t> buffer(64 * 1024);
    std::uint64_t offset = 0;
    while (offset < reader.metadata().byte_length) {
        check_cancelled(stop);
        const auto n = reader.read(offset, buffer, stop);
        require(n > 0 && n <= buffer.size(), "integrity_error",
                "Blob stream ended before its declared length");
        hasher.process(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(n));
        offset += n;
    }
    check_cancelled(stop);
    hasher.finish();
    require("sha256:" + picosha2::get_hash_hex_string(hasher) == reader.metadata().digest, "integrity_error",
            "Stored blob failed whole-object integrity verification");
}
void require(bool condition, const std::string &code, const std::string &message) {
    if (!condition)
        throw Error(code, message);
}
void identifier(const std::string &value, const char *label, std::size_t max) {
    require(!value.empty() && value.size() <= max, "invalid_argument",
            std::string(label) + " has invalid length");
    for (unsigned char c : value)
        require(c >= 32 && c != 127, "invalid_argument",
                std::string(label) + " contains a control character");
    (void)canonical(Json(value)); // Validate UTF-8 using the shared wire contract.
}
void digest_id(const std::string &value) {
    require(value.size() == 71 && value.starts_with("sha256:") &&
                std::all_of(value.begin() + 7, value.end(),
                            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
            "invalid_argument", "Expected a lowercase sha256: content identifier");
}
std::string bytes_digest(const std::vector<std::uint8_t> &bytes) {
    return "sha256:" + sha256(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}
void add_size(std::size_t &total, std::size_t value, std::size_t limit, const char *message) {
    require(value <= limit && total <= limit - value, "storage_limit", message);
    total += value;
}
void history_limit(std::size_t value) {
    require(value > 0 && value <= 1000, "invalid_argument", "History page size must be between 1 and 1000");
}
Prepared prepare(const Commit &request, const StorageLimits &limits) {
    identifier(request.workspace, "Workspace");
    identifier(request.expected_revision, "Expected revision");
    identifier(request.transaction_id, "Transaction id");
    identifier(request.actor, "Actor");
    identifier(request.reason, "Reason", 4096);
    require(request.records.size() <= limits.records_per_commit, "storage_limit", "Too many record upserts");
    require(request.blobs.size() <= limits.blobs_per_commit, "storage_limit", "Too many supplied blobs");
    require(request.required_blob_digests.size() <= 10000, "storage_limit", "Too many required blobs");
    Prepared out;
    Json records = Json::array(), blobs = Json::array();
    std::set<std::string> record_ids, blob_ids, required;
    std::size_t record_total = 0, blob_total = 0;
    for (const auto &item : request.records) {
        identifier(item.id, "Record id");
        require(record_ids.insert(item.id).second, "invalid_argument", "Duplicate record upsert");
        auto encoded = canonical(item.data);
        require(encoded.size() <= limits.record_bytes, "storage_limit", "Record exceeds byte limit");
        add_size(record_total, encoded.size(), limits.snapshot_bytes, "Record input exceeds snapshot limit");
        const auto version = "sha256:" + sha256(encoded);
        require(item.version.empty() || item.version == version, "integrity_error",
                "Record version does not match canonical content");
        out.records.push_back({item.id, version, item.data});
        out.encoded.emplace(item.id, std::move(encoded));
        records.push_back({{"id", item.id}, {"version", item.version}, {"data_digest", version}});
    }
    for (const auto &digest : request.required_blob_digests) {
        digest_id(digest);
        require(required.insert(digest).second, "invalid_argument", "Duplicate required blob digest");
    }
    for (const auto &item : request.blobs) {
        digest_id(item.digest);
        require(blob_ids.insert(item.digest).second, "invalid_argument", "Duplicate supplied blob digest");
        require(item.bytes.size() <= limits.blob_bytes, "storage_limit", "Blob exceeds byte limit");
        add_size(blob_total, item.bytes.size(), limits.blob_bytes_per_commit,
                 "Supplied blob bytes exceed transaction limit");
        require(item.digest == bytes_digest(item.bytes), "integrity_error",
                "Blob digest does not match supplied bytes");
        require(required.contains(item.digest), "invalid_argument",
                "Supplied blob is absent from the explicit closure inventory");
        blobs.push_back({{"digest", item.digest}, {"byte_length", item.bytes.size()}});
    }
    // Hash the complete request contract. Blobs are represented by their verified
    // exact-byte SHA-256 and length, never by a host path or caller-trusted hash.
    const Json envelope = {
        {"contract", "gr.storage/1"},
        {"workspace", request.workspace},
        {"expected_revision", request.expected_revision},
        {"transaction_id", request.transaction_id},
        {"actor", request.actor},
        {"reason", request.reason},
        {"records_digest", sha256(canonical(records))},
        {"blobs_digest", sha256(canonical(blobs))},
        {"required_blob_digests_digest", sha256(canonical(Json(request.required_blob_digests)))}};
    out.request_digest = "sha256:" + sha256(canonical(envelope));
    return out;
}
HistoryEntry entry_for(const Commit &request, const Prepared &prepared) {
    return {"r:" + sha256(canonical(Json{{"workspace", request.workspace},
                                         {"parent", request.expected_revision},
                                         {"request_digest", prepared.request_digest}})),
            request.expected_revision,
            request.transaction_id,
            request.actor,
            request.reason,
            prepared.request_digest};
}
void verify_entry(const std::string &workspace, const HistoryEntry &entry) {
    require(entry.revision == "r:" + sha256(canonical(Json{{"workspace", workspace},
                                                           {"parent", entry.parent},
                                                           {"request_digest", entry.request_digest}})),
            "integrity_error", "Stored ledger revision failed identity verification");
    // This checks the v1 content-addressed linkage, not actor/reason attestation.
    // V1 does not retain the canonical request summary needed to reconstruct it.
    require(entry.request_digest.size() == 71 && entry.request_digest.starts_with("sha256:") &&
                std::all_of(entry.request_digest.begin() + 7, entry.request_digest.end(),
                            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
            "integrity_error", "Stored ledger request digest is malformed");
}
void verify_record(const StoredRecord &record, const StorageLimits &limits) {
    const auto bytes = canonical(record.data);
    require(bytes.size() <= limits.record_bytes, "storage_limit", "Stored record exceeds reader byte limit");
    require(record.version == "sha256:" + sha256(bytes), "integrity_error",
            "Stored record content failed integrity verification");
}
void verify_snapshot(const Snapshot &snapshot, const StorageLimits &limits) {
    require(snapshot.records.size() <= limits.records_per_workspace, "storage_limit",
            "Snapshot exceeds record limit");
    std::size_t total = 0;
    for (const auto &item : snapshot.records) {
        const auto bytes = canonical(item.data);
        require(bytes.size() <= limits.record_bytes, "storage_limit",
                "Stored record exceeds reader byte limit");
        require(item.version == "sha256:" + sha256(bytes), "integrity_error",
                "Stored record content failed integrity verification");
        add_size(total, bytes.size(), limits.snapshot_bytes, "Snapshot exceeds byte limit");
    }
}
std::string snapshot_digest(const Snapshot &snapshot) {
    // Length-framed canonical entries avoid imposing the metadata-value limit
    // on an entire snapshot inventory. Identity and membership are both hashed.
    std::string bytes = "gr.snapshot-inventory/1\n";
    const auto scope = canonical(Json(snapshot.workspace));
    bytes += std::to_string(scope.size()) + ":" + scope;
    for (const auto &record : snapshot.records) {
        const auto item = canonical(Json{{"id", record.id}, {"version", record.version}});
        bytes += std::to_string(item.size()) + ":" + item;
    }
    return "sha256:" + sha256(bytes);
}

} // namespace gr::storage::detail

namespace gr::storage {
std::string request_digest(const Commit &request, StorageLimits limits) {
    detail::validate_limits(limits);
    return detail::prepare(request, limits).request_digest;
}
Json StorageCapabilities::to_json() const {
    const bool persistent = durability != Durability::volatile_memory;
    return {{"contract", contract},
            {"backend", backend},
            {"persistent", persistent},
            {"durability", persistent ? "local-full-sync" : "volatile"},
            {"atomicity_scope", "workspace"},
            {"history_model", history_model == HistoryModel::linear ? "linear" : "dag"},
            {"read_only", read_only},
            {"atomic_records_blobs_receipts", atomic_records_blobs_receipts},
            {"snapshot_consistency", snapshot_consistency},
            {"immutable_versions", immutable_versions},
            {"conditional_commit", conditional_commit},
            {"durable_replay", persistent},
            {"verified_blob_ranges", verified_blob_ranges},
            {"blob_read_cancellation", blob_read_cancellation},
            {"reader_outlives_store", reader_outlives_store},
            {"concurrency", concurrency},
            {"deletions", false},
            {"authorization", false},
            {"staged_uploads", false},
            {"blob_closure", "explicit required_blob_digests; no payload interpretation"},
            {"revision_identity", "opaque, workspace-scoped; empty head is 0"},
            {"retention", "all admitted versions, receipts and blobs retained; no physical disk quota"},
            {"limits",
             {{"records_per_workspace", limits.records_per_workspace},
              {"records_per_commit", limits.records_per_commit},
              {"record_bytes", limits.record_bytes},
              {"snapshot_bytes", limits.snapshot_bytes},
              {"blob_bytes", limits.blob_bytes},
              {"blobs_per_commit", limits.blobs_per_commit},
              {"blob_bytes_per_commit", limits.blob_bytes_per_commit},
              {"blob_bytes_per_workspace", limits.blob_bytes_per_workspace},
              {"history_page", 1000},
              {"blob_chunk_bytes", 1024 * 1024}}}};
}
void require_support(const StorageCapabilities &c, const StorageRequirements &r) {
    Json missing = Json::array();
    auto need = [&](bool wanted, bool supported, const char *name) {
        if (wanted && !supported)
            missing.push_back(name);
    };
    need(r.persistent, c.durability != Durability::volatile_memory, "persistent");
    need(r.writable, !c.read_only, "writable");
    need(r.atomic_commit, c.atomic_records_blobs_receipts, "atomic_commit");
    need(r.consistent_snapshots, c.snapshot_consistency, "consistent_snapshots");
    need(r.immutable_versions, c.immutable_versions, "immutable_versions");
    need(r.conditional_commit, c.conditional_commit, "conditional_commit");
    need(r.verified_blob_ranges, c.verified_blob_ranges, "verified_blob_ranges");
    need(r.blob_read_cancellation, c.blob_read_cancellation, "blob_read_cancellation");
    need(r.reader_outlives_store, c.reader_outlives_store, "reader_outlives_store");
    if (r.history_model && *r.history_model != c.history_model)
        missing.push_back("history_model");
    if (!missing.empty())
        throw Error("unsupported_storage_capability",
                    "Selected storage backend cannot satisfy required guarantees", {{"missing", missing}});
}
std::vector<std::uint8_t> IStore::blob(const std::string &workspace, const std::string &digest) {
    auto reader = open_blob(workspace, digest);
    const auto size = reader->metadata().byte_length;
    detail::require(size <= describe().limits.blob_bytes && size <= std::numeric_limits<std::size_t>::max(),
                    "storage_limit", "Blob exceeds convenience-copy limit");
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    std::size_t offset = 0;
    while (offset < result.size()) {
        const auto n = reader->read(offset, std::span<std::uint8_t>(result).subspan(offset));
        detail::require(n > 0, "integrity_error", "Blob stream ended unexpectedly");
        offset += n;
    }
    return result;
}
} // namespace gr::storage
