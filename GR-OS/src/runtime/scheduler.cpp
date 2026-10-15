#include "gr/runtime/scheduler.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace gr::runtime {
namespace {
using Clock = std::chrono::steady_clock;
struct Control;
thread_local const void *executing_runtime = nullptr;
void require(bool condition, const char *code, const char *message) {
    if (!condition)
        throw Error(code, message);
}
struct Admission {
    std::atomic<std::size_t> jobs{0}, bytes{0};
};
struct Lease {
    std::shared_ptr<Admission> admission;
    std::size_t bytes = 0;
    bool armed = false;
    ~Lease() {
        if (armed) {
            admission->bytes.fetch_sub(bytes);
            admission->jobs.fetch_sub(1);
        }
    }
};
bool terminal(JobStatus s) {
    return s == JobStatus::succeeded || s == JobStatus::failed || s == JobStatus::cancelled ||
           s == JobStatus::deadline_exceeded || s == JobStatus::outcome_unknown;
}
} // namespace
struct Job::State {
    std::shared_ptr<Control> control;
    std::shared_ptr<Lease> lease;
    std::shared_ptr<const PreparedPlan> plan;
    JobOptions options;
    std::string id;
    JobStatus status = JobStatus::queued;
    std::stop_source stop;
    bool cancelled = false, expired = false, finalizing = false, recovering = false;
    Clock::time_point admitted = Clock::now();
    std::optional<Clock::time_point> deadline;
    std::uint64_t last_dispatch = 0;
    std::size_t running = 0, remaining = 0, retained = 0, peak_retained = 0, dropped_trace = 0;
    std::vector<std::size_t> pending;
    std::vector<std::vector<std::size_t>> children;
    std::deque<std::size_t> ready;
    std::vector<std::shared_ptr<const kernel::Invocation>> values;
    std::vector<kernel::Value> outputs;
    std::optional<Publication> publication;
    std::shared_ptr<Transaction> transaction;
    std::set<std::string> staged_blobs;
    Json receipt = nullptr, error = nullptr, trace = Json::array();
    Json emergency_error = {{"code", "runtime_bookkeeping"},
                            {"message", "Native runtime bookkeeping failed; inspect publication receipt"}};
};
namespace {
struct Control {
    RuntimeConfig config;
    std::shared_ptr<Admission> admission = std::make_shared<Admission>();
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::shared_ptr<Job::State>> active;
    bool stopping = false;
    std::stop_source admission_stop;
    std::size_t slots_used = 0, peak_slots = 0;
    std::uint64_t dispatch_clock = 0, next_id = 0, submitted = 0, completed = 0, instructions = 0;
};
void trace(Job::State &job, const char *event, const std::string &node = "",
           Json details = nullptr) noexcept {
    try {
        if (job.trace.size() >= job.options.max_trace_events) {
            ++job.dropped_trace;
            return;
        }
        Json entry = {
            {"event", event},
            {"elapsed_us",
             std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - job.admitted).count()}};
        if (!node.empty())
            entry["node"] = node;
        if (!details.is_null())
            entry["details"] = std::move(details);
        job.trace.push_back(std::move(entry));
    } catch (...) {
        ++job.dropped_trace;
    }
}
Json error_json(const Error &error) {
    return {{"code", error.code}, {"message", error.what()}, {"details", error.details}};
}
Json status_locked(const Job::State &job) {
    return {{"id", job.id},
            {"status", to_string(job.status)},
            {"done", terminal(job.status)},
            {"cancel_requested", job.cancelled},
            {"deadline_expired", job.expired},
            {"running_nodes", job.running},
            {"remaining_nodes", job.remaining},
            {"plan", job.plan->id()},
            {"publication", job.receipt},
            {"error", job.error}};
}
JobResult result_locked(const Job::State &job) {
    return {job.id,
            job.status,
            job.status == JobStatus::succeeded ? job.outputs : std::vector<kernel::Value>{},
            job.receipt,
            job.error,
            {{"plan", job.plan->id()},
             {"serial", job.options.serial},
             {"priority", job.options.priority},
             {"reservation_bytes", job.lease->bytes},
             {"peak_retained_payload_bytes", job.peak_retained},
             {"buffer_accounting",
              "Per value: max(logical bytes, distinct retained owner bytes) plus descriptors; "
              "owner aliases between values are conservatively charged again"},
             {"trace", job.trace},
             {"dropped_trace_events", job.dropped_trace},
             {"scope", "Runtime/result-owned retained payload; excludes in-flight callback temporaries, "
                       "native extracted aliases, metadata and process memory"}},
            job.lease,
            {}};
}
void finish_locked(Control &control, Job::State &job, JobStatus status) {
    job.status = status;
    job.finalizing = false;
    job.ready.clear();
    trace(job, "terminal", "", {{"status", to_string(status)}});
    ++control.completed;
    control.changed.notify_all();
}
void abort_locked(Control &control, Job::State &job) {
    job.outputs.clear();
    if (job.transaction) {
        // This path never runs after publication has started.
        try {
            job.transaction->rollback();
        } catch (...) {
        }
        job.receipt = job.transaction->result().to_json();
    }
    auto status = !job.error.is_null() ? JobStatus::failed
                  : job.expired        ? JobStatus::deadline_exceeded
                                       : JobStatus::cancelled;
    if (job.error.is_null())
        job.error = {
            {"code", job.expired ? "runtime_deadline" : "runtime_cancelled"},
            {"message", job.expired ? "Cooperative job deadline expired" : "Job cancellation requested"},
            {"details", Json::object()}};
    finish_locked(control, job, status);
}
struct Work {
    std::shared_ptr<Job::State> job;
    std::size_t node = 0, slots = 1, output_limit = 0;
    bool finalize = false, released = false;
    kernel::Invocation inputs;
};
std::optional<Work> take_locked(Control &control) {
    for (auto &job : control.active) {
        if (!terminal(job->status) && !job->finalizing && !job->running &&
            (job->cancelled || job->expired || !job->error.is_null()))
            abort_locked(control, *job);
    }
    std::erase_if(control.active, [](const auto &job) { return terminal(job->status); });
    std::shared_ptr<Job::State> selected;
    std::int64_t best = std::numeric_limits<std::int64_t>::min();
    for (auto &job : control.active) {
        if (job->finalizing || job->cancelled || job->expired || !job->error.is_null())
            continue;
        const bool finalize = job->remaining == 0 && job->running == 0;
        if (!finalize && (job->ready.empty() || (job->options.serial && job->running)))
            continue;
        const auto slots = finalize ? 1 : job->plan->nodes()[job->ready.front()].cpu_slots;
        if (slots > control.config.workers - control.slots_used)
            continue;
        // Priority is bounded; waiting age grows without a bounded priority cap.
        // Continuous high-priority arrivals cannot permanently bypass a ready job.
        const auto age = std::min<std::uint64_t>(control.dispatch_clock - job->last_dispatch,
                                                 static_cast<std::uint64_t>(INT64_MAX / 2));
        const auto score = static_cast<std::int64_t>(age) + job->options.priority;
        if (!selected || score > best) {
            selected = job;
            best = score;
        }
    }
    if (!selected)
        return {};
    Work work;
    work.job = selected;
    work.finalize = selected->remaining == 0;
    if (work.finalize) {
        selected->finalizing = true;
        if (selected->publication) {
            selected->status = JobStatus::committing;
            trace(*selected, "publication_barrier");
        }
    } else {
        work.node = selected->ready.front();
        selected->ready.pop_front();
        work.slots = selected->plan->nodes()[work.node].cpu_slots;
        work.output_limit = std::min(selected->plan->nodes()[work.node].max_output_bytes,
                                     selected->options.max_retained_bytes - selected->retained);
        try {
            // Only completed predecessors are inspected, while result slots are locked.
            work.inputs = selected->plan->assemble_inputs(work.node, selected->values);
        } catch (const std::exception &e) {
            selected->error = {{"code", "runtime_inputs"}, {"message", e.what()}};
            return {};
        }
        ++selected->running;
        selected->status = JobStatus::running;
        trace(*selected, "node_start", selected->plan->nodes()[work.node].id,
              selected->plan->nodes()[work.node].selection);
    }
    selected->last_dispatch = ++control.dispatch_clock;
    control.slots_used += work.slots;
    control.peak_slots = std::max(control.peak_slots, control.slots_used);
    return work;
}
std::size_t locate(const PreparedPlan &plan, const OutputRef &output) {
    for (std::size_t i = 0; i < plan.nodes().size(); ++i)
        if (plan.nodes()[i].id == output.node) {
            const auto &exports = plan.outputs();
            require(std::any_of(
                        exports.begin(), exports.end(),
                        [&](const auto &entry) { return entry.node == i && entry.output == output.output; }),
                    "runtime_publication", "Artifact mapping must name an exported plan output");
            return i;
        }
    throw Error("runtime_publication", "Unknown artifact output node");
}
void materialize(Job::State &job) {
    if (!job.publication)
        return;
    Stage staged;
    for (const auto &binding : job.publication->artifacts) {
        if (job.stop.stop_requested())
            throw Error("runtime_cancelled", "Cancelled before publication");
        const auto index = locate(*job.plan, binding.output);
        const auto &value = job.values.at(index)->at(binding.output.output);
        require(value.representation() == kernel::Representation::Bytes, "runtime_publication",
                "Artifact publication requires a byte representation");
        const auto &bytes = value.as_bytes();
        const auto digest = "sha256:" + sha256(std::span<const std::uint8_t>(bytes));
        auto document = binding.record_document;
        if (!document.contains("artifacts"))
            document["artifacts"] = Json::array();
        require(document["artifacts"].is_array(), "runtime_publication", "Record artifacts must be an array");
        document["artifacts"].push_back(
            {{"sha256", digest}, {"byte_length", bytes.size()}, {"media_type", binding.media_type}});
        staged.records.push_back(core::construct(std::move(document)));
        if (job.staged_blobs.insert(digest).second)
            staged.blobs.push_back({digest, bytes});
        staged.required_blob_digests.push_back(digest);
    }
    job.transaction->stage(staged);
}
void execute_work(const std::shared_ptr<Control> &control, Work &work) {
    auto &job = *work.job;
    Json failure = nullptr;
    std::shared_ptr<const kernel::Invocation> value;
    std::size_t bytes = 0;
    std::optional<TransactionResult> publication;
    try {
        if (work.finalize) {
            // Build exports before crossing the publication barrier. Allocation
            // failure here cannot turn a committed write into a failed job.
            for (const auto &output : job.plan->outputs())
                job.outputs.push_back(job.values.at(output.node)->at(output.output));
            if (job.publication) {
                materialize(job);
                publication = job.transaction->commit({job.stop.get_token(), job.deadline});
            } else if (job.stop.stop_requested()) {
                throw Error("runtime_cancelled", "Job stopped before completion");
            }
        } else {
            auto output =
                job.plan->execute_node(work.node, work.inputs, job.stop.get_token(), work.output_limit);
            for (const auto &item : output) {
                const auto size = item.byte_size();
                require(size <= work.output_limit - bytes, "runtime_payload_limit",
                        "Node output budget exceeded");
                bytes += size;
            }
            value = std::make_shared<const kernel::Invocation>(std::move(output));
        }
    } catch (const Error &e) {
        failure = error_json(e);
    } catch (const std::exception &e) {
        failure = {{"code", "runtime_execution"}, {"message", e.what()}};
    } catch (...) {
        failure = {{"code", "runtime_execution"}, {"message", "Unknown native exception"}};
    }

    bool request_stop = false;
    {
        std::lock_guard lock(control->mutex);
        control->slots_used -= work.slots;
        if (!work.finalize)
            --job.running;
        work.released = true;
        if (work.finalize) {
            if (job.transaction)
                job.receipt = job.transaction->result().to_json();
            const bool committed = job.transaction && job.transaction->state() == TransactionState::committed;
            const bool uncertain =
                job.transaction && (job.transaction->state() == TransactionState::outcome_unknown ||
                                    job.transaction->state() == TransactionState::not_observed);
            if (uncertain) {
                job.error = failure.is_null() ? Json{{"code", "outcome_unknown"},
                                                     {"message", "Publication outcome needs recovery"}}
                                              : failure;
                finish_locked(*control, job, JobStatus::outcome_unknown);
            } else if (!committed && (!failure.is_null() || job.cancelled || job.expired)) {
                if (!job.cancelled && !job.expired)
                    job.error = failure;
                job.finalizing = false;
                abort_locked(*control, job);
            } else {
                trace(job, committed ? "publication_committed" : "graph_complete");
                finish_locked(*control, job, JobStatus::succeeded);
            }
        } else {
            if (!failure.is_null()) {
                if (job.error.is_null() && !job.cancelled && !job.expired)
                    job.error = failure;
                request_stop = true;
                trace(job, "node_failed", job.plan->nodes()[work.node].id, failure);
            } else if (!job.cancelled && !job.expired && job.error.is_null()) {
                if (bytes > job.options.max_retained_bytes - job.retained) {
                    job.error = {{"code", "runtime_payload_limit"},
                                 {"message", "Aggregate retained graph output budget exceeded"}};
                    request_stop = true;
                } else {
                    job.retained += bytes;
                    job.peak_retained = std::max(job.peak_retained, job.retained);
                    job.values[work.node] = std::move(value);
                    --job.remaining;
                    ++control->instructions;
                    for (const auto child : job.children[work.node])
                        if (--job.pending[child] == 0)
                            job.ready.insert(std::lower_bound(job.ready.begin(), job.ready.end(), child),
                                             child);
                    trace(job, "node_complete", job.plan->nodes()[work.node].id, {{"output_bytes", bytes}});
                }
            }
        }
    }
    if (request_stop)
        job.stop.request_stop(); // Never call foreign stop callbacks under scheduler lock.
    control->changed.notify_all();
}
void worker(const std::shared_ptr<Control> &control) {
    executing_runtime = control.get();
    for (;;) {
        std::optional<Work> work;
        {
            std::unique_lock lock(control->mutex);
            for (;;) {
                work = take_locked(*control);
                if (work)
                    break;
                if (control->stopping && control->active.empty()) {
                    executing_runtime = nullptr;
                    return;
                }
                // Periodic wake also repairs error paths without a lost notification.
                control->changed.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
        try {
            execute_work(control, *work);
        } catch (...) {
            // Error reporting and completion bookkeeping may themselves throw.
            // Repair slot ownership without allocating another diagnostic. This
            // is containment, not a hard process-memory or OOM guarantee.
            auto &job = *work->job;
            {
                std::lock_guard lock(control->mutex);
                if (!work->released) {
                    control->slots_used -= work->slots;
                    if (!work->finalize)
                        --job.running;
                }
                job.error.swap(job.emergency_error);
                if (work->finalize || job.running == 0) {
                    const auto tx = job.transaction ? job.transaction->state() : TransactionState::active;
                    const auto status =
                        tx == TransactionState::committed ? JobStatus::succeeded
                        : tx == TransactionState::outcome_unknown || tx == TransactionState::not_observed
                            ? JobStatus::outcome_unknown
                            : JobStatus::failed;
                    if (!terminal(job.status))
                        ++control->completed;
                    job.status = status;
                    job.finalizing = false;
                    job.ready.clear();
                    if (status == JobStatus::failed) {
                        job.outputs.clear();
                        if (job.transaction &&
                            (tx == TransactionState::active || tx == TransactionState::failed)) {
                            try {
                                job.transaction->rollback();
                            } catch (...) {
                            }
                        }
                    }
                }
            }
            job.stop.request_stop();
            control->changed.notify_all();
        }
    }
}
void check_handle(const std::shared_ptr<Job::State> &state) {
    require(static_cast<bool>(state), "runtime_job_closed", "Job lease has been released");
}
} // namespace
struct Runtime::Impl {
    std::shared_ptr<Control> control = std::make_shared<Control>();
    std::vector<std::jthread> workers;
    std::jthread monitor;
    std::mutex shutdown_mutex;
};
std::string to_string(JobStatus status) {
    switch (status) {
    case JobStatus::queued:
        return "queued";
    case JobStatus::running:
        return "running";
    case JobStatus::committing:
        return "committing";
    case JobStatus::succeeded:
        return "succeeded";
    case JobStatus::failed:
        return "failed";
    case JobStatus::cancelled:
        return "cancelled";
    case JobStatus::deadline_exceeded:
        return "deadline_exceeded";
    case JobStatus::outcome_unknown:
        return "outcome_unknown";
    }
    throw Error("runtime_status", "Invalid job status");
}
std::string Job::id() const {
    check_handle(state_);
    return state_->id;
}
Json Job::status() const {
    check_handle(state_);
    std::lock_guard lock(state_->control->mutex);
    return status_locked(*state_);
}
bool Job::wait_for(std::chrono::milliseconds timeout) const {
    check_handle(state_);
    require(timeout.count() >= 0 && timeout <= std::chrono::hours(24), "runtime_wait",
            "Wait timeout must be between zero and one day");
    require(executing_runtime != state_->control.get(), "runtime_reentrant_wait",
            "A worker cannot synchronously wait on its own runtime");
    std::unique_lock lock(state_->control->mutex);
    return state_->control->changed.wait_for(lock, timeout, [&] { return terminal(state_->status); });
}
JobResult Job::wait() const {
    check_handle(state_);
    require(executing_runtime != state_->control.get(), "runtime_reentrant_wait",
            "A worker cannot synchronously wait on its own runtime");
    std::unique_lock lock(state_->control->mutex);
    state_->control->changed.wait(lock, [&] { return terminal(state_->status); });
    auto result = result_locked(*state_);
    if (state_->status == JobStatus::outcome_unknown)
        result.recovery = std::make_shared<Job>(*this);
    return result;
}
void Job::cancel() const {
    check_handle(state_);
    {
        std::lock_guard lock(state_->control->mutex);
        if (terminal(state_->status))
            return;
        state_->cancelled = true;
        trace(*state_, "cancel_requested");
        if (!state_->running && !state_->finalizing)
            abort_locked(*state_->control, *state_);
    }
    state_->stop.request_stop();
    state_->control->changed.notify_all();
}
Json Job::recover_publication(bool retry) const {
    check_handle(state_);
    {
        std::lock_guard lock(state_->control->mutex);
        require(state_->status == JobStatus::outcome_unknown && state_->transaction && !state_->recovering,
                "runtime_recovery", "Recovery requires an idle job with uncertain publication");
        state_->recovering = true;
    }
    TransactionResult outcome;
    try {
        // Recovery has no computational retry and no inherited expired execution deadline.
        outcome = retry ? state_->transaction->retry() : state_->transaction->recover();
    } catch (...) {
        std::lock_guard lock(state_->control->mutex);
        state_->recovering = false;
        throw;
    }
    std::lock_guard lock(state_->control->mutex);
    state_->recovering = false;
    state_->receipt = outcome.to_json();
    if (outcome.state == TransactionState::committed) {
        state_->status = JobStatus::succeeded;
        state_->error = nullptr;
        // Exports were fully prepared before publication and retained through
        // uncertainty. Recovery changes knowledge, not computational results.
        trace(*state_, "publication_recovered");
    }
    return state_->receipt;
}
Runtime::Runtime(RuntimeConfig config) : impl_(std::make_unique<Impl>()) {
    if (config.workers == 0)
        config.workers = std::min(4u, std::max(1u, std::thread::hardware_concurrency()));
    require(config.workers <= 256 && config.max_live_jobs > 0 && config.max_live_jobs <= 4096 &&
                config.max_reserved_payload_bytes > 0,
            "runtime_config", "Invalid runtime capacity");
    impl_->control->config = std::move(config);
    try {
        for (std::size_t i = 0; i < impl_->control->config.workers; ++i)
            impl_->workers.emplace_back([control = impl_->control] { worker(control); });
        impl_->monitor = std::jthread([control = impl_->control](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::vector<std::stop_source> expired;
                {
                    std::unique_lock lock(control->mutex);
                    control->changed.wait_for(lock, std::chrono::milliseconds(5));
                    for (auto &job : control->active)
                        if (job->deadline && Clock::now() >= *job->deadline && !job->expired &&
                            !terminal(job->status)) {
                            job->expired = true;
                            trace(*job, "deadline_expired");
                            expired.push_back(job->stop);
                        }
                    for (auto &job : control->active)
                        if (!terminal(job->status) && !job->running && !job->finalizing &&
                            (job->cancelled || job->expired || !job->error.is_null()))
                            abort_locked(*control, *job);
                    // Retire idle cancelled jobs even when every compute worker
                    // is still inside a slow cooperative provider.
                    std::erase_if(control->active, [](const auto &job) { return terminal(job->status); });
                }
                for (auto &source : expired)
                    source.request_stop();
                if (!expired.empty())
                    control->changed.notify_all();
            }
        });
    } catch (...) {
        {
            std::lock_guard lock(impl_->control->mutex);
            impl_->control->stopping = true;
        }
        impl_->control->changed.notify_all();
        for (auto &thread : impl_->workers)
            if (thread.joinable())
                thread.join();
        throw;
    }
}
Runtime::~Runtime() {
    shutdown();
}
Job Runtime::submit(std::shared_ptr<const PreparedPlan> plan, JobOptions options,
                    std::optional<Publication> publication) {
    const auto control = impl_->control;
    require(executing_runtime != control.get(), "runtime_reentrant_submit",
            "Nested submission on the same worker pool is unsupported");
    require(static_cast<bool>(plan), "runtime_plan", "Prepared plan is required");
    require(options.priority >= -16 && options.priority <= 16 && options.max_trace_events <= 65536,
            "runtime_options", "Priority or trace bound is invalid");
    require(!options.timeout || (options.timeout->count() >= 0 && *options.timeout <= std::chrono::hours(24)),
            "runtime_options", "Job timeout must be between zero and one day");
    require(plan->literal_bytes() <= options.max_retained_bytes, "runtime_payload_limit",
            "Plan literals exceed the job retained-payload budget");
    for (const auto &node : plan->nodes()) {
        require(node.cpu_slots > 0 && node.cpu_slots <= control->config.workers, "runtime_capacity",
                "A node requires unavailable compute capacity");
        require(control->config.policy.allow_all_pure ||
                    control->config.policy.allowed_operations.contains(node.operation),
                "runtime_policy", "Operation is not permitted by this runtime policy");
    }
    std::size_t reserve = options.max_retained_bytes;
    if (publication) {
        require(static_cast<bool>(publication->coordinator), "runtime_publication", "Coordinator required");
        require(!publication->options.read_only, "runtime_publication",
                "Job publication requires a writable transaction");
        require(control->config.policy.writable_workspaces.contains(publication->options.workspace),
                "runtime_policy", "Workspace publication is not permitted by this runtime policy");
        require(publication->options.max_staged_bytes <= std::numeric_limits<std::size_t>::max() - reserve,
                "runtime_payload_limit", "Payload reservation overflow");
        reserve += publication->options.max_staged_bytes;
        for (const auto &binding : publication->artifacts) {
            (void)locate(*plan, binding.output);
            require(binding.record_document.is_object() && !binding.media_type.empty(), "runtime_publication",
                    "Artifact binding requires a record document and media type");
        }
    }
    auto job = std::make_shared<Job::State>();
    job->control = control;
    job->lease = std::make_shared<Lease>();
    job->lease->admission = control->admission;
    job->lease->bytes = reserve;
    job->plan = std::move(plan);
    job->options = options;
    job->retained = job->peak_retained = job->plan->literal_bytes();
    if (options.timeout)
        job->deadline = job->admitted + *options.timeout;
    {
        std::lock_guard lock(control->mutex);
        require(!control->stopping, "runtime_closed", "Runtime is shutting down");
        require(control->admission->jobs.load() < control->config.max_live_jobs, "runtime_backpressure",
                "Live job limit includes retained completed results");
        const auto used = control->admission->bytes.load();
        require(reserve <= control->config.max_reserved_payload_bytes - used, "runtime_backpressure",
                "Runtime payload reservation exhausted");
        control->admission->jobs.fetch_add(1);
        control->admission->bytes.fetch_add(reserve);
        job->lease->armed = true;
        job->id = "job:" + std::to_string(++control->next_id);
    }
    // Expensive snapshot/staging runs without scheduler locks and without a SQL write transaction.
    if (publication) {
        job->transaction = publication->coordinator->begin(
            publication->options, {control->admission_stop.get_token(), job->deadline});
        job->transaction->stage(publication->stage);
        for (const auto &blob : publication->stage.blobs)
            job->staged_blobs.insert(blob.digest);
        if (publication->history)
            job->transaction->stage_history(*publication->history);
        publication->stage = {}; // The private transaction owns staging.
        publication->history.reset();
        job->publication = std::move(publication);
    }
    const auto count = job->plan->nodes().size();
    job->remaining = count;
    job->pending.resize(count);
    job->children.resize(count);
    job->values.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto &dependencies = job->plan->nodes()[i].dependencies;
        job->pending[i] = dependencies.size();
        for (auto parent : dependencies)
            job->children.at(parent).push_back(i);
        if (dependencies.empty())
            job->ready.push_back(i);
    }
    {
        std::lock_guard lock(control->mutex);
        require(!control->stopping, "runtime_closed", "Runtime closed during admission");
        job->last_dispatch = control->dispatch_clock;
        if (job->deadline && Clock::now() >= *job->deadline)
            job->expired = true;
        trace(*job, "admitted", "", {{"reserved_bytes", reserve}});
        control->active.push_back(job);
        ++control->submitted;
    }
    control->changed.notify_all();
    return Job(std::move(job));
}
JobResult Runtime::run(std::shared_ptr<const PreparedPlan> plan, JobOptions options,
                       std::optional<Publication> publication) {
    return submit(std::move(plan), std::move(options), std::move(publication)).wait();
}
Json Runtime::diagnostics() const {
    const auto control = impl_->control;
    std::lock_guard lock(control->mutex);
    return {{"workers", control->config.workers},
            {"control_threads", 1},
            {"slots_in_use", control->slots_used},
            {"peak_slots", control->peak_slots},
            {"live_jobs", control->admission->jobs.load()},
            {"reserved_payload_bytes", control->admission->bytes.load()},
            {"submitted_jobs", control->submitted},
            {"terminal_jobs", control->completed},
            {"completed_instructions", control->instructions},
            {"closed", control->stopping},
            {"scheduling", "priority with dispatch-count aging"},
            {"scope", "One runtime instance; callbacks and other runtimes may own additional resources"}};
}
void Runtime::shutdown() {
    if (!impl_)
        return;
    const auto control = impl_->control;
    require(executing_runtime != control.get(), "runtime_reentrant_shutdown",
            "A worker cannot join its own runtime");
    std::lock_guard shutdown_lock(impl_->shutdown_mutex);
    std::vector<std::stop_source> stop;
    {
        std::lock_guard lock(control->mutex);
        control->stopping = true;
        for (auto &job : control->active) {
            if (terminal(job->status))
                continue;
            job->cancelled = true;
            stop.push_back(job->stop);
        }
    }
    control->admission_stop.request_stop();
    for (auto &source : stop)
        source.request_stop();
    control->changed.notify_all();
    for (auto &thread : impl_->workers)
        if (thread.joinable())
            thread.join();
    if (impl_->monitor.joinable()) {
        impl_->monitor.request_stop();
        control->changed.notify_all();
        impl_->monitor.join();
    }
}
} // namespace gr::runtime
