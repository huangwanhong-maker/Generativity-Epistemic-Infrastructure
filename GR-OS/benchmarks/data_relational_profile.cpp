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
double elapsed(Clock::time_point start) {
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
            {"attribution", {{"agent", "profile:observer"}, {"method", "synthetic"}}}};
}
struct Fixture {
    std::shared_ptr<gr::storage::MemoryStore> store = std::make_shared<gr::storage::MemoryStore>();
    gr::runtime::TransactionCoordinator coordinator{store};
    data::DataValue source;
    double setup_ms = 0, selection_ms = 0;
    explicit Fixture(std::size_t count) {
        auto start = Clock::now();
        store->create_workspace("profile");
        gr::runtime::Stage stage;
        for (std::size_t i = 0; i < 16; ++i)
            stage.records.push_back(
                gr::core::construct(document("entity:" + std::to_string(i), "gr.entity@1")));
        auto participant = [](std::size_t i, const char *role) {
            return Json{{"role", role},
                        {"reference", {{"scope", "record"}, {"id", "entity:" + std::to_string(i)}}}};
        };
        for (std::size_t i = 0; i < count; ++i) {
            auto value = document("relation:" + std::to_string(i), "gr.relation@1");
            value["participants"] = Json::array({participant(i % 16, "actor"), participant(i % 16, "actor"),
                                                 participant((i + 1) % 16, "witness")});
            value["qualifiers"] = {{"status", "contested"}, {"uncertainty", "not quantified"}};
            stage.records.push_back(gr::core::construct(value));
        }
        gr::runtime::TransactionOptions options;
        options.workspace = "profile";
        options.transaction_id = "seed";
        options.actor = "profile:recorder";
        options.reason = "Qualified immutable-owner cache profiling";
        options.track_history = true;
        coordinator.commit(options, stage);
        setup_ms = elapsed(start);
        start = Clock::now();
        source = rel::select_history(coordinator, {"profile", "", ""});
        selection_ms = elapsed(start);
    }
};
Json counters(const Json &before, const Json &after) {
    Json result = Json::object();
    for (auto field :
         {"hits", "misses", "full_checks", "failures", "source_hits", "source_checks", "decoded_bytes",
          "serialized_bytes", "derivations", "operator_calls", "evictions", "expired"})
        result[field] = after.at(field).get<std::uint64_t>() - before.at(field).get<std::uint64_t>();
    result["timing_ns"] = Json::object();
    for (auto field : {"decode", "source_validation", "derive", "encode"})
        result["timing_ns"][field] = after.at("timing_ns").at(field).get<std::uint64_t>() -
                                     before.at("timing_ns").at(field).get<std::uint64_t>();
    return result;
}
Json mode(Fixture &fixture, std::size_t count, std::size_t repeats, const std::string &name) {
    const bool enabled = name != "off";
    auto library = std::make_shared<data::Library>(
        data::ValidationCacheLimits{enabled ? 64u : 0u, enabled ? 16u * 1024u * 1024u : 0u, true});
    auto session = rel::install(*library, {}, {enabled ? 64u : 0u, enabled ? 16u * 1024u * 1024u : 0u, true});
    library->seal();
    gr::runtime::Runtime runtime({.workers = 2});
    data::CallOptions options;
    options.selection.backend = "cpu.sequential";
    options.max_input_bytes = options.max_output_bytes = 8 * 1024 * 1024;
    Json samples = Json::array();
    std::map<std::string, std::vector<double>> timings;
    std::string expected_checksum;
    std::size_t result_bytes = 0;
    const auto execute = [&](bool measured, std::size_t repeat) {
        if (name == "cold") {
            library->clear_validation_cache();
            session->clear_cache();
        }
        Json sample = {{"repeat", repeat}, {"phases", Json::object()}};
        auto before = session->statistics();
        sample["generic_before"] = library->validation_statistics();
        auto start = Clock::now();
        const auto boundary = [&](const char *phase) {
            const auto ms = elapsed(start);
            auto after = session->statistics();
            sample["phases"][phase] = {{"wall_ms", ms}, {"relational_delta", counters(before, after)}};
            if (measured)
                timings[phase].push_back(ms);
            before = std::move(after);
            start = Clock::now();
        };
        auto expression = data::literal(library, fixture.source);
        boundary("literal");
        expression = data::apply(library, rel::project_operation, {expression}, Json::object(), options);
        expression =
            data::apply(library, rel::roles_operation, {expression}, {{"roles", {"actor"}}}, options);
        expression = data::apply(library, rel::count_operation, {expression}, Json::object(), options);
        auto program = data::compile({expression});
        boundary("expression_compile");
        auto job = runtime.run(program.plan());
        boundary("prepared_run");
        if (job.status != gr::runtime::JobStatus::succeeded)
            throw std::runtime_error("Relational profile execution failed: " + job.error.dump());
        auto values = program.materialize(job);
        boundary("materialize");
        auto qualified = session->inspect(values.at(0));
        boundary("scoped_inspect");
        auto independent = rel::inspect(values.at(0));
        boundary("independent_inspect");
        result_bytes = values.at(0).payload.byte_size();
        const auto checksum = "sha256:" + gr::sha256(gr::canonical(qualified.at("counts")));
        std::size_t total = 0;
        for (const auto &group : qualified.at("counts"))
            total += group.at("count").get<std::size_t>();
        if (total != count * 2 || qualified.at("excluded_incidences").size() != count ||
            qualified.at("counts").size() != 16 || independent != qualified ||
            qualified.at("source").at("provenance").at("physical_revision") !=
                fixture.store->head("profile") ||
            qualified.at("source").at("revision") != fixture.source.provenance.at("semantic_revision"))
            throw std::runtime_error("Profile counts, pins or independent validation mismatch");
        if (!expected_checksum.empty() && expected_checksum != checksum)
            throw std::runtime_error("Profile checksum changed across repeats");
        expected_checksum = checksum;
        sample["selected_occurrences"] = total;
        sample["counts_checksum"] = checksum;
        sample["generic_after"] = library->validation_statistics();
        sample["relational_after"] = session->statistics();
        if (measured)
            samples.push_back(std::move(sample));
    };
    if (name == "warm")
        execute(false, 0);
    for (std::size_t i = 0; i < repeats; ++i)
        execute(true, i);
    Json medians = Json::object();
    for (auto &[phase, values] : timings) {
        std::sort(values.begin(), values.end());
        medians[phase] = values.size() % 2 ? values[values.size() / 2]
                                           : (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2;
    }
    return {{"mode", name},
            {"median_ms", std::move(medians)},
            {"samples", std::move(samples)},
            {"result_bytes", result_bytes},
            {"counts_checksum", expected_checksum},
            {"process_peak_rss_bytes_cumulative", peak_rss()}};
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::size_t repeats = 3;
        if (argc > 2)
            throw std::runtime_error("Usage: benchmark_data_relational_profile [repeats:1..10]");
        if (argc == 2) {
            const std::string text = argv[1];
            if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
                throw std::runtime_error("Repeats must be an integer in 1..10");
            repeats = std::stoull(text);
            if (!repeats || repeats > 10)
                throw std::runtime_error("Repeats must be in 1..10");
        }
        Json output = {
            {"suite", "gr.data.relational-profile/1"},
            {"repeats", repeats},
            {"backend", "cpu.sequential"},
            {"timing_instrumentation", true},
            {"method", "One eager pinned source per size; off disables both validation caches, cold clears "
                       "both before each pipeline, warm primes one pipeline before measured repeats. New "
                       "output owners remain fully validated in every mode. Phase wall times exclude "
                       "statistics snapshots. Independent inspect uses an uninstrumented separate context."},
            {"memory_scope", "Cumulative process peak including fixture setup and prior cases; cache "
                             "wire-byte charges exclude decoded JSON allocator overhead and are not RSS."},
            {"limitations",
             {"Synthetic one-revision memory-store fixture; no temporal query or SDK transport",
              "Instrumented current-build diagnostics, not the unchanged v0.5 comparison",
              "Warm input reuse does not imply reuse of newly allocated operator outputs"}},
            {"cases", Json::array()}};
        for (const std::size_t count : {64, 256, 768}) {
            Fixture fixture(count);
            Json modes = Json::array();
            std::string checksum;
            for (const std::string name : {"off", "cold", "warm"}) {
                auto result = mode(fixture, count, repeats, name);
                const auto actual = result.at("counts_checksum").get<std::string>();
                if (!checksum.empty() && checksum != actual)
                    throw std::runtime_error("Cache mode changed exact counts checksum");
                checksum = actual;
                modes.push_back(std::move(result));
            }
            output["cases"].push_back({{"relation_records", count},
                                       {"entity_records", 16},
                                       {"setup_ms_excluded", fixture.setup_ms},
                                       {"eager_select_ms_excluded", fixture.selection_ms},
                                       {"source_bytes", fixture.source.payload.byte_size()},
                                       {"pins", fixture.source.provenance},
                                       {"modes", std::move(modes)}});
        }
        std::cout << output.dump(2) << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
