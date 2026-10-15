#pragma once
#include "gr/kernel/records.hpp"
#include "gr/storage/contracts.hpp"
#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>

namespace gr::runtime {
inline constexpr const char *history_manifest_id = "gr:runtime:history";
inline constexpr const char *history_manifest_type = "gr.runtime.history-manifest@1";

enum class Isolation { workspace_snapshot_cas };
enum class AdmissionPolicy { optimistic, serialized };
enum class TransactionState {
    active,
    preparing,
    publishing,
    committed,
    rolled_back,
    conflicted,
    failed,
    outcome_unknown,
    not_observed
};
std::string to_string(TransactionState);
struct TransactionOptions {
    std::string workspace, expected_revision, transaction_id, actor, reason;
    bool read_only = false, track_history = false;
    bool upgrade_history = false;
    AdmissionPolicy admission = AdmissionPolicy::optimistic;
    Isolation isolation = Isolation::workspace_snapshot_cas;
    storage::StorageRequirements requirements;
    std::size_t max_staged_bytes = 64 * 1024 * 1024;
};
struct TransactionControl {
    std::stop_token stop;
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
struct Stage {
    Json records = Json::array(); // Complete record wrappers; each identity staged once.
    std::vector<storage::Blob> blobs;
    std::vector<std::string> required_blob_digests;
};
struct HistoryPublication {
    Json bundle;
    std::string head;
};
struct TransactionResult {
    TransactionState state = TransactionState::active;
    std::string workspace, transaction_id, base_revision, request_digest;
    std::optional<storage::Result> publication;
    Json history = Json::object();
    Json diagnostics = Json::object();
    std::string recovery_observation;
    Json to_json() const;
};

// Thread-safe logical transaction. No database transaction remains open while
// caller computation/staging runs. Rollback discards only unpublished local work.
class Transaction {
  public:
    ~Transaction();
    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;
    TransactionState state() const;
    TransactionOptions options() const;
    TransactionResult result() const;
    Json snapshot() const;
    void stage(const Stage &);
    void stage_history(const HistoryPublication &);
    TransactionResult commit(TransactionControl = {});
    TransactionResult retry(TransactionControl = {});
    TransactionResult recover();
    void rollback();

  private:
    struct Impl;
    explicit Transaction(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
    friend class TransactionCoordinator;
};

class TransactionCoordinator {
  public:
    explicit TransactionCoordinator(std::shared_ptr<storage::IStore>,
                                    std::shared_ptr<const core::TypeRegistry> = {});
    std::shared_ptr<Transaction> begin(TransactionOptions, TransactionControl = {});
    // Semantic snapshot with direct pinned-reference hydration and technical
    // runtime-history manifest excluded from the user's current record set.
    Json snapshot(const std::string &workspace, const std::string &revision = "") const;
    Json history_snapshot(const std::string &workspace, const std::string &semantic_revision = "",
                          const std::string &physical_revision = "") const;
    Json history_export_v1(const std::string &workspace, const std::string &physical_revision = "") const;
    TransactionResult commit(TransactionOptions, const Stage &, TransactionControl = {});
    std::optional<storage::TransactionReceipt> receipt(const std::string &workspace,
                                                       const std::string &transaction_id) const;
    Json capabilities() const;

  private:
    std::shared_ptr<storage::IStore> store_;
    std::shared_ptr<const core::TypeRegistry> registry_;
};
} // namespace gr::runtime
