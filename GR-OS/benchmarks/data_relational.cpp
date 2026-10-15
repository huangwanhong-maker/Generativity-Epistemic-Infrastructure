#include "gr/data/relational.hpp"
#include "gr/storage/memory_store.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#else
#include <sys/resource.h>
#endif

using gr::Json;
namespace data = gr::data;
namespace rel = gr::data::relational;
namespace {
using Clock = std::chrono::steady_clock;
double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
Json peak_rss() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
        return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
        return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
    }
#endif
    return nullptr;
}
Json document(std::string id, std::string type) {
    return {{"format", "gr.record/1"},
            {"id", std::move(id)},
            {"type", std::move(type)},
            {"payload", Json::object()},
            {"attribution", {{"agent", "benchmark:observer"}, {"method", "synthetic"}}}};
}
Json participant(std::size_t id, const char *role) {
    return {{"role", role}, {"reference", {{"scope", "record"}, {"id", "entity:" + std::to_string(id)}}}};
}
void publish(gr::runtime::TransactionCoordinator &coordinator, const Json &documents, const char *id) {
    gr::runtime::Stage stage;
    for (const auto &value : documents)
        stage.records.push_back(gr::core::construct(value));
    gr::runtime::TransactionOptions options;
    options.workspace = "benchmark";
    options.transaction_id = id;
    options.actor = "benchmark:recorder";
    options.reason = "Synthetic qualified relational data benchmark";
    options.track_history = true;
    coordinator.commit(options, stage);
}
Json run_case(std::size_t relation_count, std::size_t repeats) {
    const auto setup_start = Clock::now();
    auto store = std::make_shared<gr::storage::MemoryStore>();
    store->create_workspace("benchmark");
    gr::runtime::TransactionCoordinator coordinator(store);
    Json initial = Json::array(), updates = Json::array();
    for (std::size_t i = 0; i < 16; ++i) {
        auto value = document("entity:" + std::to_string(i), "gr.entity@1");
        value["payload"] = {{"label", "original " + std::to_string(i)}};
        initial.push_back(value);
        value["payload"]["label"] = "revised " + std::to_string(i);
        updates.push_back(std::move(value));
    }
    for (std::size_t i = 0; i < relation_count; ++i) {
        auto value = document("relation:" + std::to_string(i), "gr.relation@1");
        value["participants"] = Json::array({participant(i % 16, "actor"), participant(i % 16, "actor"),
                                             participant((i + 1) % 16, "witness")});
        value["qualifiers"] = {{"status", i % 2 ? "reported" : "contested"},
                               {"uncertainty", "not quantified"}};
        initial.push_back(std::move(value));
    }
    publish(coordinator, initial, "initial");
    const auto first = coordinator.history_snapshot("benchmark");
    const auto semantic = first.at("provenance").at("semantic_revision").get<std::string>();
    publish(coordinator, updates, "revised");
    const auto physical = store->head("benchmark");
    auto library = std::make_shared<data::Library>();
    rel::install(*library);
    library->seal();
    gr::runtime::Runtime runtime({.workers = 2});
    data::CallOptions options;
    options.selection.backend = "cpu.sequential";
    options.max_input_bytes = options.max_output_bytes = 4 * 1024 * 1024;
    const auto setup_ms = milliseconds(setup_start);
    Json samples = Json::array();
    std::map<std::string, std::vector<double>> timings;
    std::string expected_checksum;
    std::size_t source_bytes = 0, result_bytes = 0;
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        Json sample = {{"repeat", repeat}};
        auto measure = [&](const char *phase, Clock::time_point start) {
            const auto ms = milliseconds(start);
            sample[phase] = ms;
            timings[phase].push_back(ms);
        };
        auto start = Clock::now();
        auto source = rel::select_history(coordinator, {"benchmark", semantic, physical});
        measure("eager_history_select_ms", start);
        source_bytes = source.payload.byte_size();
        start = Clock::now();
        auto expression = data::literal(library, source);
        measure("literal_validation_ms", start);
        start = Clock::now();
        expression = data::apply(library, rel::project_operation, {expression}, Json::object(), options);
        expression =
            data::apply(library, rel::roles_operation, {expression}, {{"roles", {"actor"}}}, options);
        expression = data::apply(library, rel::count_operation, {expression}, Json::object(), options);
        auto program = data::compile({expression});
        measure("expression_and_compile_ms", start);
        start = Clock::now();
        auto job = runtime.run(program.plan());
        measure("prepared_run_ms", start);
        if (job.status != gr::runtime::JobStatus::succeeded)
            throw std::runtime_error("Relational benchmark failed: " + job.error.dump());
        start = Clock::now();
        auto values = program.materialize(job);
        auto qualified = rel::inspect(values.at(0));
        measure("materialize_and_inspect_ms", start);
        result_bytes = values.at(0).payload.byte_size();
        const auto checksum = "sha256:" + gr::sha256(gr::canonical(qualified.at("counts")));
        std::size_t total = 0;
        for (const auto &group : qualified.at("counts"))
            total += group.at("count").get<std::size_t>();
        if (total != relation_count * 2 || qualified.at("incidences").size() != total ||
            qualified.at("excluded_incidences").size() != relation_count ||
            qualified.at("counts").size() != 16 || store->head("benchmark") != physical ||
            qualified.at("source").at("revision") != semantic ||
            qualified.at("source").at("provenance").at("physical_revision") != physical)
            throw std::runtime_error("Relational benchmark invariant failure");
        if (!expected_checksum.empty() && checksum != expected_checksum)
            throw std::runtime_error("Relational benchmark checksum changed across repeats");
        expected_checksum = checksum;
        sample["selected_occurrences"] = total;
        sample["counts_checksum"] = checksum;
        samples.push_back(std::move(sample));
    }
    Json medians = Json::object();
    for (auto &[phase, values] : timings) {
        std::sort(values.begin(), values.end());
        medians[phase] = values.size() % 2 ? values[values.size() / 2]
                                           : (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2;
    }
    return {{"relation_records", relation_count},
            {"entity_records", 16},
            {"source_record_count", relation_count + 16},
            {"recording_revisions", 2},
            {"selected_semantic_revision", semantic},
            {"pinned_physical_revision", physical},
            {"source_bytes", source_bytes},
            {"result_bytes", result_bytes},
            {"repeats", repeats},
            {"setup_ms_excluded", setup_ms},
            {"median_ms", std::move(medians)},
            {"samples", std::move(samples)},
            {"process_peak_rss_bytes_cumulative", peak_rss()}};
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::size_t repeats = 3;
        if (argc > 2)
            throw std::runtime_error("Usage: gr_data_relational_benchmark [repeats:1..10]");
        if (argc == 2) {
            const std::string argument = argv[1];
            if (argument.empty() || argument.find_first_not_of("0123456789") != std::string::npos)
                throw std::runtime_error("Repeats must be an integer");
            repeats = std::stoull(argument);
            if (repeats < 1 || repeats > 10)
                throw std::runtime_error("Repeats must be in 1..10");
        }
        Json result = {
            {"suite", "gr.data.relational-benchmark/1"},
            {"backend", "cpu.sequential"},
            {"runtime_workers", 2},
            {"method",
             "Each repeat pins the same old semantic snapshot through the same newer physical manifest, "
             "validates a literal, compiles incidence/role/count expressions, executes the prepared plan "
             "once, then materializes and fully inspects the qualified result. Setup excluded; first repeat "
             "included; phase times include their actual native validation/copying costs."},
            {"memory_scope", "Cumulative process peak RSS including setup and earlier cases; not a per-phase "
                             "allocation or incremental case peak."},
            {"limitations",
             {"Small synthetic in-memory workloads, no SDK transport or SQLite I/O",
              "Prepared execution includes validation; it is not raw incidence-kernel time",
              "No temporal query in this benchmark; temporal costs are workload dependent",
              "No cross-version performance baseline for this new pilot"}},
            {"cases", Json::array()}};
        for (const std::size_t count : {64, 256})
            result["cases"].push_back(run_case(count, repeats));
        std::cout << result.dump(2) << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
