#include "gr/runtime.hpp"
#include "gr/storage/memory_store.hpp"
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

using namespace gr;
using namespace gr::runtime;
using namespace std::chrono_literals;
namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void fails(F &&f, const char *message) {
    bool failed = false;
    try {
        f();
    } catch (const Error &) {
        failed = true;
    }
    check(failed, message);
}
kernel::Value bytes(std::string s) {
    return kernel::Value::bytes("gr.bytes@1", {s.begin(), s.end()});
}
std::string text(const kernel::Value &value) {
    return {value.as_bytes().begin(), value.as_bytes().end()};
}
std::shared_ptr<const PreparedPlan> builtin(Graph graph) {
    auto catalog = std::make_shared<Catalog>();
    catalog->seal();
    return Planner(catalog).compile(graph);
}
std::shared_ptr<Catalog> instrumented(execution::ImplementationFunction function) {
    auto catalog = std::make_shared<Catalog>();
    kernel::OperationDescriptor operation;
    operation.id = "test.runtime.work@1";
    operation.inputs = {"gr.bytes@1"};
    operation.outputs = {"gr.bytes@1"};
    operation.meaning = "Return exact input bytes; test instrumentation is external observation";
    catalog->pin_types(operation);
    catalog->register_operation(operation);
    catalog->register_implementation({"test.runtime.provider@1",
                                      operation.id,
                                      operation.fingerprint(),
                                      {"cpu.sequential"},
                                      {"cpu"},
                                      {"host"},
                                      kernel::Determinism::Exact,
                                      true,
                                      Json::object()},
                                     std::move(function));
    catalog->seal();
    return catalog;
}
Graph tasks(std::size_t count, const std::string &operation = "gr.bytes.identity@1",
            const std::string &tag = "x") {
    Graph graph;
    for (std::size_t i = 0; i < count; ++i) {
        graph.nodes.push_back({"node:" + std::to_string(i), operation, {bytes(tag)}});
        graph.outputs.push_back({graph.nodes.back().id, 0});
    }
    return graph;
}
JobOptions small() {
    JobOptions options;
    options.max_retained_bytes = 4096;
    options.timeout = 5s;
    return options;
}
Json doc(std::string id) {
    return {{"format", "gr.record/1"},
            {"id", id},
            {"type", "gr.assertion@1"},
            {"payload", {{"claim", "synthetic runtime fixture"}}},
            {"attribution", {{"agent", "fixture:observer"}, {"method", "synthetic"}}}};
}
void dag_and_leases() {
    Graph graph;
    graph.nodes = {{"left", "gr.bytes.identity@1", {bytes("a")}},
                   {"right", "gr.bytes.identity@1", {bytes("b")}},
                   {"joined", "gr.bytes.concat@1", {OutputRef{"left"}, OutputRef{"right"}}}};
    graph.outputs = {{"joined"}};
    auto plan = builtin(graph);
    RuntimeConfig config;
    config.workers = 2;
    config.max_live_jobs = 1;
    config.max_reserved_payload_bytes = 4096;
    Runtime runtime(config);
    auto options = small();
    options.max_trace_events = 4;
    auto job = runtime.submit(plan, options);
    check(job.wait_for(2s), "diamond graph completes");
    auto result = job.wait();
    check(result.status == JobStatus::succeeded && text(result.outputs[0]) == "ab",
          "typed dependent graph result");
    check(result.diagnostics["trace"].size() == 4 && result.diagnostics["dropped_trace_events"] > 0,
          "trace storage is bounded");
    fails([&] { runtime.submit(plan, options); }, "completed job retention enforces backpressure");
    job.release();
    fails([&] { runtime.submit(plan, options); }, "exported C++ result retains budget after job release");
    result = {};
    for (int i = 0; i < 100 && runtime.diagnostics()["live_jobs"] != 0; ++i)
        std::this_thread::sleep_for(1ms);
    {
        auto next = runtime.run(plan, options);
        check(next.status == JobStatus::succeeded, "released result permits next admission");
    }
    for (int i = 0; i < 100 && runtime.diagnostics()["live_jobs"] != 0; ++i)
        std::this_thread::sleep_for(1ms);
    check(runtime.diagnostics()["reserved_payload_bytes"] == 0,
          "terminal result release reclaims reservation");
    auto too_small = small();
    too_small.max_retained_bytes = 1;
    fails([&] { runtime.submit(plan, too_small); }, "literal admission bound");
    auto output_limited = small();
    output_limited.max_retained_bytes = 3;
    auto limited = runtime.run(plan, output_limited);
    check(limited.status == JobStatus::failed && limited.outputs.empty(),
          "aggregate output exhaustion returns no partial results");
}
void concurrency_and_failures() {
    std::atomic<int> active{0}, peak{0};
    auto catalog = instrumented([&](const kernel::Invocation &input, std::stop_token stop) {
        const auto current = ++active;
        auto seen = peak.load();
        while (seen < current && !peak.compare_exchange_weak(seen, current)) {
        }
        for (int i = 0; i < 6 && !stop.stop_requested(); ++i)
            std::this_thread::sleep_for(1ms);
        --active;
        if (stop.stop_requested())
            throw Error("execution_cancelled", "fixture stopped");
        return input;
    });
    auto plan = Planner(catalog).compile(tasks(12, "test.runtime.work@1"));
    RuntimeConfig config;
    config.workers = 3;
    Runtime runtime(config);
    auto result = runtime.run(plan, small());
    check(result.status == JobStatus::succeeded && result.outputs.size() == 12, "parallel graph completes");
    check(peak > 1 && peak <= 3 && active == 0, "global worker budget with real concurrent callbacks");
    check(runtime.diagnostics()["peak_slots"] <= 3, "diagnostic slots reflect budget");
    peak = 0;
    auto serial = small();
    serial.serial = true;
    auto serial_result = runtime.run(plan, serial);
    check(serial_result.status == JobStatus::succeeded && peak == 1, "serial mode runs one node at a time");
    std::atomic<bool> entered{false};
    std::atomic<int> outstanding{0};
    auto failing = instrumented([&](const kernel::Invocation &input, std::stop_token stop) {
        if (text(input[0]) == "wait") {
            ++outstanding;
            entered = true;
            while (!stop.stop_requested())
                std::this_thread::sleep_for(1ms);
            --outstanding;
            throw Error("execution_cancelled", "sibling cancelled");
        }
        while (!entered && !stop.stop_requested())
            std::this_thread::sleep_for(1ms);
        throw Error("fixture_failure", "deliberate provider failure");
        return input;
    });
    Graph graph = tasks(2, "test.runtime.work@1");
    graph.nodes[0].inputs = {bytes("wait")};
    graph.nodes[1].inputs = {bytes("fail")};
    auto failed = runtime.run(Planner(failing).compile(graph), small());
    check(failed.status == JobStatus::failed && failed.outputs.empty() && outstanding == 0,
          "failure cancels and joins siblings before terminal report");
}
void cancel_deadline_shutdown() {
    std::atomic<bool> entered{false}, release{false};
    std::atomic<int> calls{0};
    auto catalog = instrumented([&](const kernel::Invocation &input, std::stop_token stop) {
        ++calls;
        entered = true;
        while (!release && !stop.stop_requested())
            std::this_thread::sleep_for(1ms);
        if (stop.stop_requested())
            throw Error("execution_cancelled", "fixture stopped");
        return input;
    });
    auto plan = Planner(catalog).compile(tasks(1, "test.runtime.work@1"));
    RuntimeConfig config;
    config.workers = 1;
    Runtime runtime(config);
    auto first = runtime.submit(plan, small());
    while (!entered)
        std::this_thread::sleep_for(1ms);
    auto queued = runtime.submit(plan, small());
    check(!queued.wait_for(0ms), "zero poll is not cancellation");
    queued.cancel();
    check(queued.wait_for(100ms) && queued.wait().status == JobStatus::cancelled && calls == 1,
          "queued cancellation finishes while worker is occupied");
    first.cancel();
    check(first.wait().status == JobStatus::cancelled, "running cancellation propagates");
    auto timed = small();
    timed.timeout = 15ms;
    auto deadline = runtime.run(plan, timed);
    check(deadline.status == JobStatus::deadline_exceeded, "one job deadline reaches cooperative callback");
    auto survivor = runtime.submit(plan, small());
    runtime.shutdown();
    check(survivor.wait().status == JobStatus::cancelled,
          "shutdown joins active work; handle remains readable");
    fails([&] { runtime.submit(plan, small()); }, "closed runtime rejects new jobs");
}
void priority_and_stress() {
    std::atomic<bool> entered{false}, release{false};
    std::atomic<int> high_seen{0}, low_at{-1};
    auto catalog = instrumented([&](const kernel::Invocation &input, std::stop_token stop) {
        const auto tag = text(input[0]);
        if (tag == "hold") {
            entered = true;
            while (!release && !stop.stop_requested())
                std::this_thread::sleep_for(1ms);
        } else if (tag == "high")
            ++high_seen;
        else if (tag == "low")
            low_at = high_seen.load();
        return input;
    });
    Planner planner(catalog);
    RuntimeConfig config;
    config.workers = 1;
    config.max_live_jobs = 128;
    Runtime runtime(config);
    auto held = runtime.submit(planner.compile(tasks(1, "test.runtime.work@1", "hold")), small());
    while (!entered)
        std::this_thread::sleep_for(1ms);
    auto low_options = small(), high_options = small();
    low_options.priority = -16;
    high_options.priority = 16;
    auto low = runtime.submit(planner.compile(tasks(1, "test.runtime.work@1", "low")), low_options);
    auto high = runtime.submit(planner.compile(tasks(80, "test.runtime.work@1", "high")), high_options);
    release = true;
    check(held.wait().status == JobStatus::succeeded && low.wait().status == JobStatus::succeeded &&
              high.wait().status == JobStatus::succeeded,
          "priority jobs complete");
    check(low_at >= 0 && low_at < 80, "aging serves low-priority work before high-priority backlog drains");
    std::mutex mutex;
    std::vector<Job> jobs;
    auto plan = planner.compile(tasks(1, "test.runtime.work@1"));
    std::vector<std::jthread> submitters;
    for (int t = 0; t < 4; ++t)
        submitters.emplace_back([&] {
            for (int i = 0; i < 20; ++i) {
                auto job = runtime.submit(plan, small());
                std::lock_guard lock(mutex);
                jobs.push_back(std::move(job));
            }
        });
    for (auto &thread : submitters)
        thread.join();
    for (auto &job : jobs)
        check(job.wait().status == JobStatus::succeeded, "concurrent submitter stress");
}
void publication_and_policy() {
    auto store = std::make_shared<storage::MemoryStore>();
    store->create_workspace("workspace");
    auto coordinator = std::make_shared<TransactionCoordinator>(store);
    auto plan = builtin(tasks(1, "gr.bytes.identity@1", "artifact contents"));
    Publication publication;
    publication.coordinator = coordinator;
    publication.options.workspace = "workspace";
    publication.options.expected_revision = "0";
    publication.options.transaction_id = "runtime:1";
    publication.options.actor = "fixture:recorder";
    publication.options.reason = "atomic computed artifact";
    publication.options.track_history = true;
    publication.options.max_staged_bytes = 1024 * 1024;
    publication.artifacts = {{{"node:0", 0}, doc("result"), "application/octet-stream"},
                             {{"node:0", 0}, doc("same-artifact"), "application/octet-stream"}};
    const kernel::Bytes artifact_bytes{'a', 'r', 't', 'i', 'f', 'a', 'c', 't', ' ',
                                       'c', 'o', 'n', 't', 'e', 'n', 't', 's'};
    publication.stage.blobs.push_back({"sha256:" + sha256(artifact_bytes), artifact_bytes});
    Runtime denied;
    fails([&] { denied.submit(plan, small(), publication); },
          "module availability does not grant publication authority");
    RuntimeConfig config;
    config.workers = 2;
    config.policy.writable_workspaces.insert("workspace");
    Runtime runtime(config);
    auto committed = runtime.run(plan, small(), publication);
    check(committed.status == JobStatus::succeeded && !committed.publication.is_null(),
          "computed result publication");
    auto snapshot = coordinator->snapshot("workspace");
    check(snapshot["records"].size() == 2,
          "duplicate computed artifacts share custody; technical manifest excluded from user view");
    const auto digest = snapshot["records"][0]["document"]["artifacts"][0]["sha256"].get<std::string>();
    check(store->blob("workspace", digest) == kernel::Bytes({'a', 'r', 't', 'i', 'f', 'a', 'c', 't', ' ', 'c',
                                                             'o', 'n', 't', 'e', 'n', 't', 's'}),
          "computed artifact custody matches content");
    check(store->receipt("workspace", "runtime:1").has_value(), "receipt published with artifact");
    publication.options.transaction_id = "runtime:stale";
    publication.artifacts[0].record_document = doc("stale");
    auto conflict = runtime.run(plan, small(), publication);
    check(conflict.status == JobStatus::failed && !store->receipt("workspace", "runtime:stale"),
          "stale-base publication does not silently retry/rebase");
    RuntimeConfig restrictive;
    restrictive.policy.allow_all_pure = false;
    Runtime restricted(restrictive);
    fails([&] { restricted.submit(plan, small()); }, "operation policy is enforced independently of catalog");
}
} // namespace
int main() {
    try {
        dag_and_leases();
        concurrency_and_failures();
        cancel_deadline_shutdown();
        priority_and_stress();
        publication_and_policy();
        std::cout << "runtime: " << checks << " checks passed, including 80 concurrent-submitter jobs\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "runtime failure: " << e.what() << '\n';
        return 1;
    }
}
