#pragma once
#include "gr/storage/contracts.hpp"
namespace gr::storage {
class SQLiteStore final : public IStore {
  public:
    // Operator-supplied local file path; never derived from workspace/record IDs.
    explicit SQLiteStore(const std::string &path, bool read_only = false, StorageLimits limits = {});
    ~SQLiteStore() override;
    SQLiteStore(const SQLiteStore &) = delete;
    SQLiteStore &operator=(const SQLiteStore &) = delete;
    StorageCapabilities describe() const override;
    std::string coordination_identity() const override;
    void create_workspace(const std::string &) override;
    std::string head(const std::string &) override;
    Snapshot snapshot(const std::string &, const std::string &revision = "") override;
    StoredRecord record_version(const std::string &, const std::string &, const std::string &) override;
    Result commit(const Commit &) override;
    HistoryPage history(const std::string &, const std::string &before = "",
                        std::size_t limit = 100) override;
    std::optional<TransactionReceipt> receipt(const std::string &, const std::string &) override;
    BlobMetadata blob_info(const std::string &, const std::string &) override;
    std::unique_ptr<BlobReader> open_blob(const std::string &, const std::string &,
                                          BlobReadOptions options = {}) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace gr::storage
