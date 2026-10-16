#include "../tests/fixture.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#endif

namespace a = gr_relational_analysis;
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
std::uint64_t peak_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return counters.PeakWorkingSetSize;
#endif
    return 0;
}
void fixture_files(const std::filesystem::path &directory, const std::string &name,
                   const gr::data::DataValue &value) {
    std::filesystem::create_directories(directory);
    const auto &bytes = value.payload.as_bytes();
    std::ofstream data(directory / (name + ".bin"), std::ios::binary);
    data.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    data.close();
    if (!data)
        throw std::runtime_error("Cannot write benchmark payload fixture");
    std::ofstream metadata(directory / (name + ".json"), std::ios::binary);
    metadata << gr::canonical(gr::Json{{"spec", value.spec.to_json()}, {"provenance", value.provenance}});
    metadata.close();
    if (!metadata)
        throw std::runtime_error("Cannot write benchmark metadata fixture");
}
int main(int argc, char **argv) {
    try {
        if (argc > 3)
            throw std::runtime_error("Usage: benchmark_analysis [repeats] [fixture_directory]");
        const int repeats = argc > 1 ? std::stoi(argv[1]) : 3;
        if (repeats < 1 || repeats > 7)
            throw std::runtime_error("repeats must be 1..7");
        gr::Json report = {
            {"suite", "gr.relational-analysis/1"},
            {"repeats", repeats},
            {"method",
             "Actual in-memory tracked source; one-time source/Layer3 setup reported separately. Same "
             "immutable incidence owner; three sizes in fixed order. Every result checked; no timing gate."},
            {"memory_scope", "Cumulative process peak includes source setup and earlier sizes. Byte counts "
                             "describe payload/transport, not decoded JSON or total live allocations."},
            {"cases", gr::Json::array()}};
        for (std::size_t relations : {64u, 256u, 768u}) {
            auto setup_start = Clock::now();
            analysis_fixture::Fixture fixture;
            fixture.synthetic(relations);
            auto source_start = Clock::now();
            auto source = fixture.selected();
            const auto source_ms = elapsed(source_start);
            auto projection_start = Clock::now();
            auto incidence = fixture.incidence(source);
            const auto incidence_ms = elapsed(projection_start);
            auto changed = analysis_fixture::document("entity:0");
            changed["payload"]["label"] = "new recording account";
            fixture.publish(gr::Json::array({changed}));
            auto next_incidence = fixture.incidence(fixture.selected());
            auto next_network = a::analyze(next_incidence, fixture.session);
            auto expr = gr::data::apply(fixture.library, a::summary_operation,
                                        {gr::data::literal(fixture.library, incidence)});
            auto program = gr::data::compile({expr});
            const auto setup_ms = elapsed(setup_start);
            // Optional independent-consumer fixtures are outside measured phases.
            if (argc == 3) {
                fixture_files(argv[2], "incidence-" + std::to_string(relations), incidence);
                fixture_files(argv[2], "incidence-next-" + std::to_string(relations), next_incidence);
            }
            gr::Json samples = gr::Json::array();
            std::map<std::string, std::vector<double>> times;
            std::string expected_summary, expected_snapshot;
            std::size_t encoded_bytes = 0;
            for (int repeat = 0; repeat < repeats; ++repeat) {
                gr::Json sample;
                auto record = [&](const char *phase, Clock::time_point begin) {
                    const auto ms = elapsed(begin);
                    sample[phase] = ms;
                    times[phase].push_back(ms);
                };
                auto begin = Clock::now();
                auto network = a::analyze(incidence, fixture.session);
                record("index_ms", begin);
                begin = Clock::now();
                for (int i = 0; i < 1000; ++i) {
                    auto snapshot = network.snapshot();
                    if (&snapshot.vertices() != &network.snapshot().vertices())
                        throw std::runtime_error("Snapshot did not share topology");
                }
                record("share_1000_snapshots_ms", begin);
                begin = Clock::now();
                auto summary = a::summarize(network);
                record("summary_ms", begin);
                begin = Clock::now();
                auto comparison = a::compare(network, next_network);
                record("compare_ms", begin);
                begin = Clock::now();
                auto bytes = network.snapshot().encode();
                record("snapshot_encode_ms", begin);
                begin = Clock::now();
                auto imported = a::NetworkSnapshot::decode(
                    std::span(reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
                record("snapshot_import_ms", begin);
                begin = Clock::now();
                auto prepared_result = fixture.runtime.run(program.plan());
                auto outputs = program.materialize(prepared_result);
                record("prepared_run_materialize_ms", begin);
                auto prepared = a::inspect_summary(outputs.at(0));
                if (summary.at("owners") != relations || summary.at("targets") != 16 ||
                    summary.at("occurrences") != relations * 3 ||
                    comparison.at("records").at("changed").size() != 1 ||
                    imported.id() != network.snapshot().id() ||
                    prepared.at("degrees") != summary.at("degrees") ||
                    imported.validation_basis() != "structural_only")
                    throw std::runtime_error("Analysis benchmark oracle failure");
                const auto summary_digest = "sha256:" + gr::sha256(summary.dump());
                if (repeat && (summary_digest != expected_summary || imported.id() != expected_snapshot))
                    throw std::runtime_error("Nondeterministic benchmark output");
                expected_summary = summary_digest;
                expected_snapshot = imported.id();
                encoded_bytes = bytes.size();
                samples.push_back(std::move(sample));
            }
            gr::Json medians;
            for (auto &[name, values] : times) {
                std::sort(values.begin(), values.end());
                const auto middle = values.size() / 2;
                medians[name] =
                    values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
            }
            report["cases"].push_back({{"relations", relations},
                                       {"entity_records", 16},
                                       {"occurrences", relations * 3},
                                       {"setup_ms", setup_ms},
                                       {"historical_selection_ms", source_ms},
                                       {"initial_layer3_projection_ms", incidence_ms},
                                       {"source_bytes", source.payload.byte_size()},
                                       {"incidence_bytes", incidence.payload.byte_size()},
                                       {"snapshot_encoded_bytes", encoded_bytes},
                                       {"snapshot_shared_payload_copy_bytes", 0},
                                       {"summary_digest", expected_summary},
                                       {"snapshot_id", expected_snapshot},
                                       {"source_pins", source.provenance},
                                       {"samples", std::move(samples)},
                                       {"medians_ms", std::move(medians)},
                                       {"session_statistics", fixture.session->statistics()},
                                       {"cumulative_peak_rss_bytes", peak_bytes()}});
        }
        std::cout << report.dump(2) << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
