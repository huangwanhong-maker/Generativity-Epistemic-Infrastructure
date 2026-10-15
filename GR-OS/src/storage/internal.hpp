#pragma once
#include "gr/storage/contracts.hpp"
#include <map>

namespace gr::storage::detail {
struct Prepared {
    std::string request_digest;
    std::vector<StoredRecord> records;
    std::map<std::string, std::string> encoded;
};
void require(bool, const std::string &code, const std::string &message);
void identifier(const std::string &, const char *label, std::size_t max = 256);
void digest_id(const std::string &);
std::string bytes_digest(const std::vector<std::uint8_t> &);
void add_size(std::size_t &, std::size_t, std::size_t, const char *);
void history_limit(std::size_t);
void validate_limits(const StorageLimits &);
StorageCapabilities capabilities_for(const char *, bool persistent, bool read_only, const StorageLimits &);
Prepared prepare(const Commit &, const StorageLimits &);
HistoryEntry entry_for(const Commit &, const Prepared &);
void verify_entry(const std::string &workspace, const HistoryEntry &);
void verify_record(const StoredRecord &, const StorageLimits &);
void verify_snapshot(const Snapshot &, const StorageLimits &);
std::string snapshot_digest(const Snapshot &);
void validate_options(const BlobReadOptions &);
void check_cancelled(std::stop_token);
std::size_t read_size(const BlobMetadata &, std::uint64_t offset, std::size_t destination, std::size_t chunk);
void verify_reader(BlobReader &, std::stop_token);
} // namespace gr::storage::detail
