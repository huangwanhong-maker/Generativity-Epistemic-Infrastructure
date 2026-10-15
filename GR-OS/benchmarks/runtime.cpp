#include "gr/runtime/scheduler.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace gr;
using namespace gr::kernel;
using namespace gr::runtime;
namespace {
constexpr std::size_t repetitions = 7;
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
Value bytes(std::string_view text) {
    return Value::bytes("gr.bytes@1", Bytes(text.begin(), text.end()));
}
template <class Function> Json measure(const std::string &name, Json workload, Function function) {
    function(); // Untimed warmup, including the same correctness assertions.
    std::vector<double> samples;
    Json diagnostics;
    for (std::size_t i = 0; i < repetitions; ++i) {
        const auto start = std::chrono::steady_clock::now();
        diagnostics = function();
        const auto finish = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
    }
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    const auto median = sorted[sorted.size() / 2];
    // Nearest-rank p95 for seven samples is the maximum, not a tail-latency SLA.
    const auto p95 = sorted.back();
    std::cout << name << ": median " << median << " ms, p95 " << p95 << " ms\n";
    return {{"name", name},        {"workload", workload},       {"samples_ms", samples},
            {"median_ms", median}, {"p95_nearest_rank_ms", p95}, {"diagnostics", diagnostics}};
}
Graph hash_graph(std::size_t byte_count, std::size_t branches) {
    Bytes payload(byte_count);
    for (std::size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<std::uint8_t>((i * 37 + 11) % 256);
    Graph graph;
    Node seed{"seed", "gr.bytes.identity@1", {Value::bytes("gr.bytes@1", std::move(payload))}};
    seed.max_input_bytes = seed.max_output_bytes = byte_count;
    graph.nodes.push_back(std::move(seed));
    Node join{"join", "gr.bytes.identity@1", {bytes("joined")}};
    join.max_input_bytes = join.max_output_bytes = 6;
    for (std::size_t i = 0; i < branches; ++i) {
        const auto name = "hash-" + std::to_string(i);
        Node node{name, "gr.bytes.sha256@1", {OutputRef{"seed", 0}}};
        node.selection.implementation = "gr.cpu.sha256.incremental@1";
        node.selection.backend = "cpu.sequential";
        node.max_input_bytes = byte_count;
        node.max_output_bytes = 64;
        graph.nodes.push_back(std::move(node));
        join.after.push_back(name);
        graph.outputs.push_back({name, 0});
    }
    graph.nodes.push_back(std::move(join));
    graph.outputs.push_back({"join", 0});
    return graph;
}
Json runtime_observation(const JobResult &result, Runtime &runtime) {
    check(result.status == JobStatus::succeeded, "Runtime benchmark job failed");
    const auto system = runtime.diagnostics();
    return {{"peak_compute_slots", system.at("peak_slots")},
            {"peak_retained_payload_bytes", result.diagnostics.at("peak_retained_payload_bytes")},
            {"reservation_bytes", result.diagnostics.at("reservation_bytes")},
            {"scope", "Logical retained payload and declared slots; not process RSS or callback allocation"}};
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 2, "Usage: benchmark_runtime <output.json>");
        auto catalog = std::make_shared<Catalog>();
        catalog->seal();
        Planner planner(catalog);
        Graph tiny{{{"identity", "gr.bytes.identity@1", {bytes("x")}}}, {{"identity", 0}}};
        tiny.nodes[0].max_input_bytes = tiny.nodes[0].max_output_bytes = 1;
        const auto prepared = planner.compile(tiny);
        const auto input = prepared->assemble_inputs(0, {});
        Json cases = Json::array();
        cases.push_back(
            measure("prepared_identity_4096", {{"invocations", 4096}, {"bytes_per_invocation", 1}}, [&] {
                for (int i = 0; i < 4096; ++i)
                    check(prepared->execute_node(0, input, {}, 1) == input,
                          "Prepared identity changed bytes");
                return Json{{"cpu_slots", 1}, {"execution", "same caller thread; no job submission"}};
            }));
        {
            Runtime runtime({.workers = 1, .max_live_jobs = 32, .max_reserved_payload_bytes = 1024 * 1024});
            JobOptions options;
            options.max_retained_bytes = 1024;
            options.max_trace_events = 0;
            cases.push_back(measure("runtime_identity_256",
                                    {{"jobs", 256},
                                     {"nodes_per_job", 1},
                                     {"input_bytes", 1},
                                     {"workers", 1},
                                     {"trace_events", 0}},
                                    [&] {
                                        Json observed;
                                        for (int i = 0; i < 256; ++i) {
                                            const auto result = runtime.run(prepared, options);
                                            check(result.outputs == input, "Runtime identity changed bytes");
                                            observed = runtime_observation(result, runtime);
                                        }
                                        return observed;
                                    }));
        }
        constexpr std::size_t payload_bytes = 512 * 1024, branches = 8;
        const auto graph = hash_graph(payload_bytes, branches);
        const auto plan = planner.compile(graph);
        const auto &payload = std::get<Value>(graph.nodes[0].inputs[0]).as_bytes();
        const auto expected = sha256(std::span<const std::uint8_t>(payload));
        for (const auto &[workers, serial] :
             std::vector<std::pair<std::size_t, bool>>{{4, true}, {1, false}, {2, false}, {4, false}}) {
            Runtime runtime(
                {.workers = workers, .max_live_jobs = 16, .max_reserved_payload_bytes = 16 * 1024 * 1024});
            JobOptions options;
            options.serial = serial;
            options.max_retained_bytes = 2 * 1024 * 1024;
            options.max_trace_events = 128;
            const auto name = "fork_join_hash_8x512KiB_" +
                              std::string(serial ? "serial" : "workers" + std::to_string(workers));
            cases.push_back(
                measure(name,
                        {{"workers", workers},
                         {"serial_job", serial},
                         {"hash_branches", branches},
                         {"bytes_per_hash", payload_bytes},
                         {"total_hash_bytes", payload_bytes * branches},
                         {"nodes", plan->nodes().size()},
                         {"trace_events", options.max_trace_events},
                         {"plan", plan->id()}},
                        [&] {
                            const auto result = runtime.run(plan, options);
                            check(result.outputs.size() == branches + 1, "Fork/join omitted an output");
                            for (std::size_t i = 0; i < branches; ++i)
                                check(std::string(result.outputs[i].as_bytes().begin(),
                                                  result.outputs[i].as_bytes().end()) == expected,
                                      "Fork/join hash differs from expected bytes");
                            check(result.outputs.back() == bytes("joined"), "Join result is incorrect");
                            return runtime_observation(result, runtime);
                        }));
        }
        Json environment{{"logical_threads_reported", std::thread::hardware_concurrency()},
                         {"cplusplus", __cplusplus},
                         {"build", "unspecified"}};
#ifdef _WIN32
        environment["os"] = "Windows";
#else
        environment["os"] = "non-Windows";
#endif
#ifdef _MSC_VER
        environment["compiler"] = "MSVC";
        environment["compiler_version"] = _MSC_FULL_VER;
#elif defined(__GNUC__)
        environment["compiler"] = "GCC-compatible";
        environment["compiler_version"] = __VERSION__;
#endif
#ifdef NDEBUG
        environment["build"] = "NDEBUG";
#else
        environment["build"] = "assertions enabled";
#endif
        std::ifstream executable(argv[0], std::ios::binary);
        if (executable) {
            const std::string image((std::istreambuf_iterator<char>(executable)), {});
            environment["executable_sha256"] = sha256(image);
        }
        const Json report{
            {"format", "gr.runtime-benchmark/1"},
            {"suite", "native-runtime-v0.3/1"},
            {"environment", environment},
            {"method",
             {{"clock", "steady_clock"},
              {"warmups", 1},
              {"repetitions", repetitions},
              {"includes",
               "native invocation/job completion, output correctness checks and diagnostic collection"},
              {"excludes",
               "catalog registration, planning, runtime thread-pool construction, Python/C transport"},
              {"p95", "nearest rank; with seven samples this is the maximum, not an SLA"}}},
            {"cases", cases}};
        const std::filesystem::path path(argv[1]);
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output << report.dump(2) << '\n';
        check(bool(output), "Failed to write runtime benchmark artifact");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "runtime benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
