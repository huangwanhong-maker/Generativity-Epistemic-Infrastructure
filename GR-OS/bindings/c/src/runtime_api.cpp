#include "gr/runtime/scheduler.hpp"
#include "internal.hpp"
#ifdef GR_EXPERIMENTAL_DATA
#include "gr/experimental/data.hpp"
#endif
#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace gr::binding {
namespace {
void keys(const Json &object, std::initializer_list<const char *> allowed) {
    if (!object.is_object())
        throw Error("invalid_request", "Expected an object");
    std::set<std::string> names(allowed.begin(), allowed.end());
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!names.contains(it.key()))
            throw Error("invalid_request", "Unknown field: " + it.key());
}
bool flag(const Json &object, const char *key, bool fallback) {
    if (!object.contains(key))
        return fallback;
    if (!object.at(key).is_boolean())
        throw Error("invalid_request", std::string(key) + " must be boolean");
    return object.at(key).get<bool>();
}
std::vector<std::string> strings(const Json &object, const char *key) {
    std::vector<std::string> result;
    if (object.contains(key))
        for (const auto &v : array(object, key)) {
            if (!v.is_string() || v.get_ref<const std::string &>().find('\0') != std::string::npos)
                throw Error("invalid_request", std::string(key) + " must contain strings without NUL");
            result.push_back(v.get<std::string>());
        }
    return result;
}
runtime::OutputRef reference(const Json &value) {
    keys(value, {"node", "output"});
    return {text(value, "node"), count(value, "output", 0)};
}
runtime::Graph graph_from_json(const Json &value) {
    keys(value, {"nodes", "outputs", "context"});
    runtime::Graph graph;
    graph.context = value.value("context", Json::object());
    const auto &nodes = array(value, "nodes");
    if (nodes.size() > 4096)
        throw Error("request_limit", "Graph exceeds 4096 nodes");
    std::size_t used = 0;
    for (const auto &entry : nodes) {
        keys(entry,
             {"id", "operation", "inputs", "after", "selection", "max_output_bytes", "max_input_bytes"});
        runtime::Node node;
        node.id = text(entry, "id");
        node.operation = text(entry, "operation");
        node.after = strings(entry, "after");
        node.max_input_bytes = count(entry, "max_input_bytes", node.max_input_bytes);
        node.max_output_bytes = count(entry, "max_output_bytes", node.max_output_bytes);
        if (entry.contains("selection")) {
            const auto &s = entry.at("selection");
            keys(s, {"implementation", "backend", "device", "prefer_implementations"});
            if (s.contains("implementation"))
                node.selection.implementation = text(s, "implementation");
            if (s.contains("backend"))
                node.selection.backend = text(s, "backend");
            if (s.contains("device"))
                node.selection.device = text(s, "device");
            node.selection.prefer_implementations = strings(s, "prefer_implementations");
        }
        const auto &inputs = array(entry, "inputs");
        if (inputs.size() > 1024)
            throw Error("request_limit", "Node arity exceeds 1024");
        for (const auto &input : inputs) {
            keys(input, {"literal", "ref"});
            if (input.contains("literal") == input.contains("ref"))
                throw Error("invalid_request", "Operand needs exactly one of literal or ref");
            if (input.contains("ref"))
                node.inputs.emplace_back(reference(input.at("ref")));
            else {
                keys(input.at("literal"), {"type", "representation", "data", "data_base64"});
                node.inputs.emplace_back(value_from_json(input.at("literal"), used, 64 * 1024 * 1024));
            }
        }
        graph.nodes.push_back(std::move(node));
    }
    for (const auto &entry : array(value, "outputs"))
        graph.outputs.push_back(reference(entry));
    return graph;
}
runtime::RuntimeConfig runtime_config(const Json &value) {
    keys(value, {"workers", "max_live_jobs", "max_reserved_payload_bytes", "policy"});
    runtime::RuntimeConfig config;
    config.workers = count(value, "workers", config.workers);
    config.max_live_jobs = count(value, "max_live_jobs", config.max_live_jobs);
    config.max_reserved_payload_bytes =
        count(value, "max_reserved_payload_bytes", config.max_reserved_payload_bytes);
    if (value.contains("policy")) {
        const auto &p = value.at("policy");
        keys(p, {"allow_all_pure", "allowed_operations", "writable_workspaces"});
        config.policy.allow_all_pure = flag(p, "allow_all_pure", true);
        auto operations = strings(p, "allowed_operations"), workspaces = strings(p, "writable_workspaces");
        config.policy.allowed_operations.insert(operations.begin(), operations.end());
        config.policy.writable_workspaces.insert(workspaces.begin(), workspaces.end());
    }
    return config;
}
runtime::JobOptions job_options(const Json &value) {
    keys(value, {"priority", "serial", "max_retained_bytes", "timeout_ms", "max_trace_events"});
    runtime::JobOptions options;
    if (value.contains("priority")) {
        const auto &p = value.at("priority");
        if (!p.is_number_integer() ||
            (p.is_number_unsigned() ? p.get<std::uint64_t>() > 16
                                    : (p.get<std::int64_t>() < -16 || p.get<std::int64_t>() > 16)))
            throw Error("invalid_request", "priority must be an integer within -16..16");
        options.priority = p.get<int>();
    }
    options.serial = flag(value, "serial", false);
    options.max_retained_bytes = count(value, "max_retained_bytes", options.max_retained_bytes);
    options.max_trace_events = count(value, "max_trace_events", options.max_trace_events);
    if (value.contains("timeout_ms")) {
        const auto ms = count(value, "timeout_ms", 0);
        if (ms > 86400000)
            throw Error("invalid_request", "timeout_ms exceeds one day");
        options.timeout = std::chrono::milliseconds(ms);
    }
    return options;
}
std::optional<runtime::Publication> publication(const Json &request) {
    if (!request.contains("publication"))
        return {};
    const auto &p = request.at("publication");
    keys(p, {"store_handle", "transaction", "stage", "history", "artifacts"});
    runtime::Publication result;
    result.coordinator =
        std::make_shared<runtime::TransactionCoordinator>(store_for({{"handle", text(p, "store_handle")}}));
    keys(p.at("transaction"),
         {"workspace", "expected_revision", "transaction_id", "actor", "reason", "read_only", "track_history",
          "isolation", "requirements", "max_staged_bytes", "admission", "upgrade_history"});
    result.options = transaction_options(p.at("transaction"));
    if (p.contains("stage")) {
        keys(p.at("stage"), {"records", "blobs", "required_blob_digests"});
        result.stage = transaction_stage(p.at("stage"));
    }
    if (p.contains("history")) {
        keys(p.at("history"), {"bundle", "head"});
        result.history =
            runtime::HistoryPublication{p.at("history").at("bundle"), text(p.at("history"), "head")};
    }
    if (p.contains("artifacts"))
        for (const auto &a : array(p, "artifacts")) {
            keys(a, {"output", "record_document", "media_type"});
            result.artifacts.push_back(
                {reference(a.at("output")), a.at("record_document"),
                 a.contains("media_type") ? text(a, "media_type") : "application/octet-stream"});
        }
    return result;
}
struct Context {
    std::shared_ptr<runtime::Catalog> catalog;
    runtime::Planner planner;
    runtime::Runtime scheduler;
    static std::shared_ptr<runtime::Catalog> make_catalog(const std::vector<std::string> &modules) {
        auto catalog = std::make_shared<runtime::Catalog>();
        for (const auto &module : modules) {
#ifdef GR_EXPERIMENTAL_DATA
            if (module == "gr.experimental.data@1") {
                experimental::data::install(*catalog);
                continue;
            }
#endif
            throw Error("unsupported_module", "Unknown optional module", {{"module", module}});
        }
        catalog->seal();
        return catalog;
    }
    explicit Context(runtime::RuntimeConfig config, const std::vector<std::string> &modules = {})
        : catalog(make_catalog(modules)), planner(catalog), scheduler(std::move(config)) {}
};
std::mutex handles_mutex;
std::atomic<std::uint64_t> next_handle{1};
std::map<std::string, std::shared_ptr<Context>> contexts;
std::map<std::string, std::shared_ptr<const runtime::PreparedPlan>> plans;
std::map<std::string, runtime::Job> jobs;
std::size_t pending_contexts = 0, pending_plans = 0, pending_jobs = 0;
struct Reservation {
    std::size_t &pending;
    bool active = true;
    ~Reservation() {
        if (active) {
            std::lock_guard lock(handles_mutex);
            --pending;
        }
    }
    void publish() {
        --pending;
        active = false;
    }
};
template <class T> auto acquire(const std::map<std::string, T> &map, const std::string &handle) {
    std::lock_guard lock(handles_mutex);
    auto found = map.find(handle);
    if (found == map.end())
        throw Error("invalid_handle", "Runtime resource is closed or unknown");
    return found->second;
}
template <class T>
void reserve(const std::map<std::string, T> &map, std::size_t &pending, std::size_t maximum) {
    std::lock_guard lock(handles_mutex);
    if (map.size() + pending >= maximum)
        throw Error("resource_limit", "Native runtime handle capacity reached");
    ++pending;
}
template <class T> void close(std::map<std::string, T> &map, const std::string &handle) {
    T released;
    {
        std::lock_guard lock(handles_mutex);
        auto found = map.find(handle);
        if (found == map.end())
            throw Error("invalid_handle", "Runtime resource is closed or unknown");
        released = std::move(found->second);
        map.erase(found);
    }
    // Potential worker joins and ownership destruction happen after unlocking.
}
Json result_json(const runtime::JobResult &result) {
    Json outputs = Json::array();
    for (const auto &v : result.outputs)
        outputs.push_back(value_json(v));
    return {{"id", result.id},       {"status", runtime::to_string(result.status)},
            {"outputs", outputs},    {"publication", result.publication},
            {"error", result.error}, {"diagnostics", result.diagnostics}};
}
Json synchronous(Context &context, std::shared_ptr<const runtime::PreparedPlan> plan,
                 runtime::JobOptions options, std::optional<runtime::Publication> publish) {
    // Reserve recovery capacity before any effect. Successful synchronous calls
    // return values only; uncertain publication retains exact-request control.
    reserve(jobs, pending_jobs, 4096);
    Reservation reserved{pending_jobs};
    auto result = context.scheduler.run(std::move(plan), options, std::move(publish));
    auto encoded = result_json(result);
    if (result.recovery) {
        const auto handle = "job:" + std::to_string(next_handle.fetch_add(1));
        encoded["recovery_handle"] = handle;
        std::lock_guard lock(handles_mutex);
        jobs.emplace(handle, *result.recovery);
        reserved.publish();
    }
    return encoded;
}
} // namespace
Json publish_plan(std::shared_ptr<const runtime::PreparedPlan> plan) {
    reserve(plans, pending_plans, 1024);
    Reservation reserved{pending_plans};
    const auto handle = "plan:" + std::to_string(next_handle.fetch_add(1));
    Json result{{"handle", handle}, {"plan", plan->describe()}};
    std::lock_guard lock(handles_mutex);
    plans.emplace(handle, std::move(plan));
    reserved.publish();
    return result;
}
void discard_plan(const std::string &handle) noexcept {
    try {
        close(plans, handle);
    } catch (...) {
    }
}
runtime::Job job_for(const std::string &handle) {
    return acquire(jobs, handle);
}
std::optional<Json> runtime_call(const Json &request, const std::string &op) {
    if (op == "runtime.evaluate") {
        Context context(runtime_config(request.value("config", Json::object())), strings(request, "modules"));
        auto plan = context.planner.compile(graph_from_json(request.at("graph")));
        return synchronous(context, std::move(plan), job_options(request.value("options", Json::object())),
                           publication(request));
    }
    if (op == "runtime.open") {
        reserve(contexts, pending_contexts, 32);
        Reservation reserved{pending_contexts};
        auto context = std::make_shared<Context>(runtime_config(request.value("config", Json::object())),
                                                 strings(request, "modules"));
        const auto handle = "runtime:" + std::to_string(next_handle.fetch_add(1));
        auto capabilities = context->catalog->capabilities();
        {
            std::lock_guard lock(handles_mutex);
            contexts.emplace(handle, context);
            reserved.publish();
        }
        return Json{{"handle", handle}, {"catalog", capabilities}};
    }
    if (op == "runtime.close") {
        // Explicit shutdown also joins if another in-flight call retains Context.
        auto context = acquire(contexts, text(request, "handle"));
        close(contexts, text(request, "handle"));
        context->scheduler.shutdown();
        return Json{{"closed", true}};
    }
    if (op == "plan.close") {
        close(plans, text(request, "handle"));
        return Json{{"closed", true}};
    }
    if (op == "plan.describe")
        return acquire(plans, text(request, "handle"))->describe();
    if (op == "job.close") {
        close(jobs, text(request, "handle"));
        return Json{{"closed", true}};
    }
    if (op.rfind("job.", 0) == 0) {
        auto job = acquire(jobs, text(request, "handle"));
        if (op == "job.status")
            return job.status();
        if (op == "job.cancel") {
            job.cancel();
            return job.status();
        }
        if (op == "job.recover")
            return job.recover_publication(flag(request, "retry", false));
        if (op == "job.wait" || op == "job.result") {
            const auto ms = count(request, "timeout_ms", 0);
            if (ms > 60000)
                throw Error("invalid_request", "wait timeout_ms exceeds 60000");
            const auto ready = job.wait_for(std::chrono::milliseconds(ms));
            Json result{{"ready", ready}, {"status", job.status()}};
            if (ready)
                result["result"] = result_json(job.wait());
            return result;
        }
        throw Error("unsupported_operation", "Unknown job operation", {{"op", op}});
    }
    if (op.rfind("runtime.", 0) != 0)
        return std::nullopt;
    auto context = acquire(contexts, text(request, "handle"));
    if (op == "runtime.capabilities")
        return Json{{"catalog", context->catalog->capabilities()},
                    {"scheduler", context->scheduler.diagnostics()},
                    {"planner", context->planner.cache_info()}};
    if (op == "runtime.plan") {
        reserve(plans, pending_plans, 1024);
        Reservation reserved{pending_plans};
        auto plan = context->planner.compile(graph_from_json(request.at("graph")));
        const auto handle = "plan:" + std::to_string(next_handle.fetch_add(1));
        auto description = plan->describe();
        {
            std::lock_guard lock(handles_mutex);
            plans.emplace(handle, plan);
            reserved.publish();
        }
        return Json{{"handle", handle}, {"plan", description}};
    }
    if (op == "runtime.submit" || op == "runtime.run") {
        auto plan = acquire(plans, text(request, "plan"));
        auto options = job_options(request.value("options", Json::object()));
        auto publish = publication(request);
        if (op == "runtime.run")
            return synchronous(*context, std::move(plan), options, std::move(publish));
        reserve(jobs, pending_jobs, 4096);
        Reservation reserved{pending_jobs};
        auto job = context->scheduler.submit(plan, options, std::move(publish));
        const auto handle = "job:" + std::to_string(next_handle.fetch_add(1));
        auto status = job.status();
        {
            std::lock_guard lock(handles_mutex);
            jobs.emplace(handle, std::move(job));
            reserved.publish();
        }
        return Json{{"handle", handle}, {"status", status}};
    }
    throw Error("unsupported_operation", "Unknown runtime operation", {{"op", op}});
}
} // namespace gr::binding
