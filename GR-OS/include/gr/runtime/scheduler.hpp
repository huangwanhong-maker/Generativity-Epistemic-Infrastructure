#pragma once
#include "gr/runtime/planner.hpp"
#include "gr/runtime/transaction.hpp"
#include <chrono>
#include <optional>
#include <set>

namespace gr::runtime {
enum class JobStatus {
    queued,
    running,
    committing,
    succeeded,
    failed,
    cancelled,
    deadline_exceeded,
    outcome_unknown
};
std::string to_string(JobStatus);
struct RuntimePolicy {
    // Pure execution permission is separate from module registration.
    bool allow_all_pure = true;
    std::set<std::string> allowed_operations;
    std::set<std::string> writable_workspaces;
};
struct RuntimeConfig {
    std::size_t workers = 0; // Auto: min(4, reported logical CPU capacity).
    std::size_t max_live_jobs = 64;
    std::size_t max_reserved_payload_bytes = 256 * 1024 * 1024;
    RuntimePolicy policy;
};
struct JobOptions {
    int priority = 0;    // -16..16; dispatch-count aging, no callback preemption.
    bool serial = false; // Stable ready order within this job, not global trace determinism.
    std::size_t max_retained_bytes = 64 * 1024 * 1024;
    std::optional<std::chrono::milliseconds> timeout;
    std::size_t max_trace_events = 8192;
};
struct ArtifactBinding {
    OutputRef output;
    Json record_document;
    std::string media_type = "application/octet-stream";
};
struct Publication {
    std::shared_ptr<TransactionCoordinator> coordinator;
    TransactionOptions options;
    Stage stage;
    std::optional<HistoryPublication> history;
    std::vector<ArtifactBinding> artifacts;
};
class Job;
struct JobResult {
    std::string id;
    JobStatus status = JobStatus::queued;
    std::vector<kernel::Value> outputs;
    Json publication = nullptr;
    Json error = nullptr;
    Json diagnostics = Json::object();
    // Exported native results keep their admission lease until destroyed.
    std::shared_ptr<const void> retention;
    // Only uncertain results retain control for receipt recovery/frozen retry.
    std::shared_ptr<Job> recovery;
};
// Owning result/control lease; completed results continue to count against live
// job and payload admission until the last Job and JobResult leases are released.
// Extracted Value aliases become the caller's responsibility, outside this quota.
class Job {
  public:
    struct State;
    Job() = default;
    std::string id() const;
    Json status() const;
    bool wait_for(std::chrono::milliseconds) const;
    JobResult wait() const;
    void cancel() const;
    // Only for a terminal uncertain publication. Retry uses the frozen request.
    Json recover_publication(bool retry = false) const;
    void release() noexcept { state_.reset(); }
    explicit operator bool() const noexcept { return static_cast<bool>(state_); }

  private:
    explicit Job(std::shared_ptr<State> state) : state_(std::move(state)) {}
    std::shared_ptr<State> state_;
    friend class Runtime;
};
// Owns one bounded fixed worker pool. All active work is cancelled/joined by
// shutdown/destruction. No native callback is forcibly interrupted or sandboxed.
class Runtime {
  public:
    explicit Runtime(RuntimeConfig = {});
    ~Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    Job submit(std::shared_ptr<const PreparedPlan>, JobOptions = {}, std::optional<Publication> = {});
    JobResult run(std::shared_ptr<const PreparedPlan>, JobOptions = {}, std::optional<Publication> = {});
    Json diagnostics() const;
    void shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace gr::runtime
