#include "gr/memory/access.hpp"
#include "gr/data/standard.hpp"
#include "gr/kernel/numerical.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>

using namespace gr;
using namespace gr::data;
namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::int64_t load(std::span<const std::uint8_t> bytes) {
    return std::bit_cast<std::int64_t>(memory::load_little<std::uint64_t>(bytes));
}
std::int64_t scalar(const memory::BufferSet &set, bool nullable) {
    std::int64_t total = 0;
    const auto n = set.at("values").elements();
    for (std::size_t i = 0; i < n; ++i) {
        const std::array<std::size_t, 1> position{i};
        if (!nullable || set.at("validity").element_bytes(position)[0])
            total = kernel::checked_add(total, load(set.at("values").element_bytes(position)));
    }
    return total;
}
std::int64_t batch(const memory::BufferSet &set, bool nullable) {
    std::int64_t total = 0;
    const memory::ElementRange values(set.at("values"));
    std::optional<memory::ElementRange> mask;
    if (nullable)
        mask.emplace(set.at("validity"));
    values.for_each([&](std::size_t i, auto bytes) {
        if (!mask || mask->at(i)[0])
            total = kernel::checked_add(total, load(bytes));
    });
    return total;
}
template <class F> Json measure(F fn, std::int64_t expected, std::size_t inner) {
    check(fn() == expected, "Access warmup checksum");
    std::vector<double> times;
    for (int repeat = 0; repeat < 5; ++repeat) {
        const auto start = std::chrono::steady_clock::now();
        std::int64_t combined = 0;
        for (std::size_t i = 0; i < inner; ++i)
            combined += fn();
        const auto finish = std::chrono::steady_clock::now();
        check(combined == expected * static_cast<std::int64_t>(inner), "Access measured checksum");
        times.push_back(std::chrono::duration<double, std::milli>(finish - start).count() / inner);
    }
    auto sorted = times;
    std::sort(sorted.begin(), sorted.end());
    return {{"samples_ms_per_scan", times},
            {"median_ms", sorted[2]},
            {"min_ms", sorted.front()},
            {"max_ms", sorted.back()},
            {"inner_scans", inner}};
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 2, "Usage: benchmark_access <output.json>");
        Json cases = Json::array();
        for (std::size_t n : {std::size_t{256}, std::size_t{16384}, std::size_t{262144}}) {
            std::vector<std::int64_t> numbers(n);
            std::vector<std::uint8_t> validity(n);
            for (std::size_t i = 0; i < n; ++i) {
                numbers[i] = static_cast<std::int64_t>(i % 251) - 125;
                validity[i] = i % 7 != 0;
            }
            for (bool nullable : {false, true}) {
                StandardOptions options;
                const auto value = column_i64(
                    numbers, nullable ? Validity(std::span<const std::uint8_t>(validity)) : Validity{},
                    options);
                for (const std::string layout : {"contiguous", "reversed", "broadcast"}) {
                    auto source = value.payload.as_buffers().at("values");
                    std::optional<memory::BufferView> mask;
                    if (nullable)
                        mask = value.payload.as_buffers().at("validity");
                    if (layout == "reversed") {
                        source = source.slice(0, n - 1, n, -1);
                        if (mask)
                            mask = mask->slice(0, n - 1, n, -1);
                    } else if (layout == "broadcast") {
                        source = memory::BufferView(source.buffer(), source.element(), {n}, {0}, 8);
                        if (mask)
                            mask = memory::BufferView(mask->buffer(), mask->element(), {n}, {0}, 1);
                    }
                    std::vector<memory::NamedBuffer> named{{"values", source}};
                    if (mask)
                        named.push_back({"validity", *mask});
                    const memory::BufferSet set(std::move(named));
                    std::int64_t expected = 0;
                    for (std::size_t i = 0; i < n; ++i) {
                        const auto position = layout == "broadcast"  ? 1
                                              : layout == "reversed" ? n - i - 1
                                                                     : i;
                        if (!nullable || validity[position])
                            expected += numbers[position];
                    }
                    const auto inner = n == 256 ? std::size_t{64} : std::size_t{1};
                    Json entry{{"elements", n},
                               {"nullable", nullable},
                               {"layout", layout},
                               {"checksum", expected},
                               {"input_logical_bytes", set.logical_bytes()},
                               {"input_retained_owner_bytes", set.retained_bytes()},
                               {"scalar", measure([&] { return scalar(set, nullable); }, expected, inner)},
                               {"batch", measure([&] { return batch(set, nullable); }, expected, inner)}};
                    entry["batch_payload_copies"] = 0;
                    entry["copy_then_batch"] = measure(
                        [&] {
                            std::vector<memory::NamedBuffer> copied{
                                {"values", source.copy_contiguous(options.allocation_budget)}};
                            if (mask)
                                copied.push_back(
                                    {"validity", mask->copy_contiguous(options.allocation_budget)});
                            return batch(memory::BufferSet(std::move(copied)), nullable);
                        },
                        expected, inner);
                    entry["copy_then_batch_payload_bytes_per_scan"] = set.logical_bytes();
                    entry["owner_budget_peak_through_case"] = options.allocation_budget->peak();
                    cases.push_back(std::move(entry));
                }
            }
        }
        Json report{
            {"format", "gr.access-profile/1"},
            {"cases", cases},
            {"method",
             {{"warmups", 1},
              {"samples", 5},
              {"purpose", "Controlled checked scalar geometry/name lookup versus owning batch access; both "
                          "use same endian load and checked sum"},
              {"scope", "Access and explicit copying only; not full operation validation, compilation, "
                        "transport or scheduler"},
              {"timing", "Per-scan wall clock; checks outside timed interval; copy case includes "
                         "allocation/destruction"},
              {"memory", "Requested owned buffer bytes; not metadata, temporary vectors or process RSS"}}}};
        std::ofstream out(argv[1], std::ios::binary);
        check(bool(out), "Cannot open report");
        out << report.dump(2) << '\n';
        check(bool(out), "Cannot write report");
        std::cout << cases.size() << " access profiles written\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
