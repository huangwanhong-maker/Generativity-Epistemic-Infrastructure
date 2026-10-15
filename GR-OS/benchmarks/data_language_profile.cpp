#include "gr/data/standard.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>

using namespace gr;
using namespace gr::data;
namespace {
using Clock = std::chrono::steady_clock;
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Run, class Verify> Json measure(Run run, Verify verify) {
    auto warm = run();
    verify(warm);
    std::vector<double> samples;
    for (int i = 0; i < 5; ++i) {
        auto start = Clock::now();
        auto result = run();
        samples.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
        verify(result);
    }
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    return {{"samples_ms", samples}, {"median_ms", sorted[2]}};
}
Json delta(const Json &before, const Json &after) {
    Json out;
    for (const char *key : {"hits", "misses", "failures", "evictions", "expired", "successful_validation_ns"})
        out[key] = after.at(key).get<std::uint64_t>() - before.at(key).get<std::uint64_t>();
    out["entries_after"] = after.at("entries");
    out["charged_bytes_after"] = after.at("charged_bytes");
    return out;
}
void run_case(Json &cases, std::size_t rows, bool nullable, bool memo) {
    auto library = std::make_shared<Library>(ValidationCacheLimits{memo ? 64u : 0u, 16 * 1024 * 1024, true});
    install_standard(*library);
    library->seal();
    std::vector<std::int64_t> values(rows);
    std::vector<std::uint8_t> mask(nullable ? rows : 0);
    std::int64_t expected = 0;
    for (std::size_t i = 0; i < rows; ++i) {
        values[i] = static_cast<std::int64_t>(i % 101) - 50;
        if (nullable)
            mask[i] = i % 7 != 0;
        if (!nullable || mask[i])
            expected += 2 * values[i] + 1;
    }
    auto value = column_i64(values, nullable ? Validity{mask} : Validity{});
    auto x = input(library, "x", value.spec);
    auto two = literal(library, scalar_i64(2));
    auto one = literal(library, scalar_i64(1));
    auto affine = apply(library, "gr.data.affine@1", {x, two, one});
    Function function({apply(library, "gr.data.sum@1", {affine}, {{"nulls", "skip"}, {"min_count", 1}})});
    Bindings bindings{{"x", value}};
    auto program = function.bind(bindings);
    runtime::Runtime runtime({.workers = 1});
    runtime::JobOptions job;
    job.max_trace_events = 0;
    Json item{{"rows", rows}, {"nullable", nullable}, {"memo", memo}, {"expected_sum", expected}};
    auto profile = [&](const char *phase, auto run, auto verify) {
        const auto before = library->validation_statistics();
        item[phase] = measure(run, verify);
        item[phase]["validation_profile_including_warmup"] = delta(before, library->validation_statistics());
    };
    profile(
        "validate_repeated_owner",
        [&] {
            library->validate(value);
            return 0;
        },
        [](int) {});
    profile(
        "validate_cold_owner_certificate",
        [&] {
            library->clear_validation_cache();
            library->validate(value);
            return 0;
        },
        [](int) {});
    profile(
        "bind_existing_function", [&] { return function.bind(bindings); },
        [](const Program &p) { check(p.plan()->nodes().size() == 2, "Unexpected reachable node count"); });
    auto verify = [&](const std::vector<DataValue> &out) {
        check(out.size() == 1 && get_optional_i64(out.front()) == expected,
              "Independent checked sum differs");
    };
    profile(
        "prepared_runtime_and_materialization",
        [&] { return program.materialize(runtime.run(program.plan(), job)); }, verify);
    profile(
        "bind_runtime_and_materialization",
        [&] {
            auto rebound = function.bind(bindings);
            return rebound.materialize(runtime.run(rebound.plan(), job));
        },
        verify);
    item["final_validation_statistics"] = library->validation_statistics();
    cases.push_back(std::move(item));
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 2, "Usage: benchmark_data_language_profile OUTPUT.json");
        Json out{{"suite", "gr.data.language-profile/1"},
                 {"method",
                  "One warmup plus five samples; result verification and destruction outside timing. "
                  "Instrumented validation counters include warmup; timings cover successful representation "
                  "validation, size checks and cache-charge calculation. Not sampled CPU profiling."},
                 {"limits", "Weak payload-owner reuse, 64 entries/16 MiB conservative source+spec charge; "
                            "counters are not RSS. Clear can repopulate during concurrent validation."},
                 {"cases", Json::array()}};
        for (std::size_t rows : {8u, 4096u, 262144u})
            for (bool nullable : {false, true})
                for (bool memo : {false, true})
                    run_case(out["cases"], rows, nullable, memo);
        std::ofstream file(argv[1], std::ios::binary);
        file << out.dump(2) << '\n';
        check(bool(file), "Cannot write profile capture");
        std::cout << "12 language profiles passed independent result checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
