#include "gr/data/sparse.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <tuple>

using namespace gr;
using namespace gr::data;
namespace {
constexpr std::size_t repetitions = 5;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Run, class Verify> Json measure(std::string name, Json workload, Run run, Verify verify) {
    // Verification, output hashing, and destruction are outside the measured interval.
    {
        auto warm = run();
        verify(warm);
    }
    std::vector<double> samples;
    Json diagnostics;
    for (std::size_t i = 0; i < repetitions; ++i) {
        const auto start = std::chrono::steady_clock::now();
        auto result = run();
        const auto finish = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
        diagnostics = verify(result);
    }
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    std::cout << name << ": " << sorted[sorted.size() / 2] << " ms\n";
    return {{"name", name},
            {"workload", workload},
            {"samples_ms", samples},
            {"median_ms", sorted[sorted.size() / 2]},
            {"min_ms", sorted.front()},
            {"max_ms", sorted.back()},
            {"p95_nearest_rank_ms", sorted.back()},
            {"diagnostics", diagnostics}};
}
Json payloads(const kernel::Invocation &values) {
    Json digests = Json::array();
    std::size_t logical = 0, retained_per_value = 0;
    for (const auto &v : values) {
        digests.push_back(v.as_buffers().content_digest());
        logical += v.as_buffers().logical_bytes();
        retained_per_value += v.as_buffers().retained_bytes();
    }
    return {{"physical_output_digests", digests},
            {"output_logical_bytes", logical},
            {"output_retained_bytes_sum_per_value", retained_per_value},
            {"retention_scope", "Per-value owner sums can count shared owners more than once; not RSS"}};
}
struct PreparedResult {
    kernel::Invocation outputs;
    std::size_t peak_observed_owner_bytes = 0;
};
PreparedResult execute_prepared(const Program &program, const StandardOptions &options) {
    const auto &plan = *program.plan();
    std::vector<std::shared_ptr<const kernel::Invocation>> results(plan.nodes().size());
    PreparedResult answer;
    answer.peak_observed_owner_bytes = options.allocation_budget->used();
    for (std::size_t i = 0; i < plan.nodes().size(); ++i) {
        auto inputs = plan.assemble_inputs(i, results);
        results[i] = std::make_shared<const kernel::Invocation>(
            plan.execute_node(i, inputs, {}, plan.nodes()[i].max_output_bytes));
        answer.peak_observed_owner_bytes =
            std::max(answer.peak_observed_owner_bytes, options.allocation_budget->used());
    }
    for (const auto &output : plan.outputs())
        answer.outputs.push_back(results.at(output.node)->at(output.output));
    return answer;
}
std::vector<DataValue> as_data(const Program &program, const kernel::Invocation &values) {
    check(program.outputs().size() == values.size(), "Wrong output count");
    std::vector<DataValue> result;
    for (std::size_t i = 0; i < values.size(); ++i)
        result.push_back({program.outputs()[i], values[i]});
    return result;
}
struct TableFixture {
    std::vector<std::int64_t> numbers, selected_numbers;
    std::vector<std::uint8_t> validity, flags, selected_flags;
    std::int64_t expected_sum = 0;
};
TableFixture table_fixture(std::size_t n) {
    TableFixture f;
    f.numbers.resize(n);
    f.validity.resize(n);
    f.flags.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        f.numbers[i] = static_cast<std::int64_t>(i % 101) - 50;
        f.validity[i] = i % 7 != 0;
        f.flags[i] = i % 3 == 0;
        if (f.validity[i] && 2 * f.numbers[i] + 1 > 10) {
            f.selected_numbers.push_back(f.numbers[i]);
            f.selected_flags.push_back(f.flags[i]);
            f.expected_sum += 2 * f.numbers[i] + 1;
        }
    }
    return f;
}
DataValue construct_table(const TableFixture &f, const StandardOptions &options) {
    return make_table({{"number", column_i64(f.numbers, std::span<const std::uint8_t>(f.validity), options)},
                       {"flag", column_bool(f.flags, {}, options)}},
                      options);
}
std::vector<Expr> table_expressions(const std::shared_ptr<Library> &library, const Expr &table,
                                    const Expr &two, const Expr &one, const Expr &threshold) {
    auto field = apply(library, "gr.data.table-column@1", {table}, {{"name", "number"}});
    auto transformed = apply(library, "gr.data.affine@1", {field, two, one});
    auto predicate = apply(library, "gr.data.compare@1", {transformed, threshold}, {{"relation", "gt"}});
    auto selected = apply(library, "gr.data.table-filter@1", {table, predicate}, {{"nulls", "drop"}});
    auto selected_numbers = apply(library, "gr.data.filter@1", {transformed, predicate}, {{"nulls", "drop"}});
    auto sum = apply(library, "gr.data.sum@1", {selected_numbers}, {{"nulls", "skip"}, {"min_count", 1}});
    return {selected, sum};
}
Json verify_table(const std::vector<DataValue> &values, const TableFixture &f,
                  const StandardOptions &options) {
    check(values.size() == 2, "Wrong table pipeline result count");
    auto numbers = to_i64(table_column(values[0], "number", options), options);
    auto flags = to_bool(table_column(values[0], "flag", options), options);
    check(numbers.size() == f.selected_numbers.size() && flags.size() == numbers.size(),
          "Wrong filtered table row count");
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        check(numbers[i] == f.selected_numbers[i], "Filtered values differ from independent oracle");
        check(flags[i] == static_cast<bool>(f.selected_flags[i]), "Filtered row association changed");
    }
    check(get_optional_i64(values[1], options) == f.expected_sum, "Wrong checked ordered sum");
    auto report = payloads({values[0].payload, values[1].payload});
    report["selected_rows"] = numbers.size();
    report["expected_sum"] = f.expected_sum;
    return report;
}
void table_benchmarks(Json &cases, std::size_t rows) {
    StandardOptions options;
    auto library = std::make_shared<Library>();
    install_standard(*library, options);
    library->seal();
    const auto fixture = table_fixture(rows); // Untimed host input and independent expected output.
    const auto prefix = "nullable_table_" + std::to_string(rows) + "_";
    Json workload{{"rows", rows},
                  {"input_columns", 2},
                  {"owned_input_payload_bytes", rows * 10},
                  {"input_distribution", "int64 i%101-50; validity i%7!=0; bool i%3==0"},
                  {"pipeline", "column -> affine(2,1) -> compare(>10) -> table/filter -> ordered sum"}};
    cases.push_back(measure(
        prefix + "construct", workload, [&] { return construct_table(fixture, options); },
        [&](const DataValue &v) {
            library->validate(v);
            check(v.payload.as_buffers().metadata().at("rows") == rows, "Construction row count");
            return payloads({v.payload});
        }));
    const auto table = construct_table(fixture, options);
    const auto table_expr = literal(library, table);
    const auto two = literal(library, scalar_i64(2, options));
    const auto one = literal(library, scalar_i64(1, options));
    const auto threshold = literal(library, scalar_i64(10, options));
    cases.push_back(measure(
        prefix + "infer_existing_literals", workload,
        [&] { return table_expressions(library, table_expr, two, one, threshold); },
        [&](const std::vector<Expr> &expressions) {
            check(expressions.size() == 2 && expressions[0].spec() == table.spec,
                  "Inference changed table schema");
            return Json{{"calls", 6}, {"literal_creation_included", false}};
        }));
    const auto expressions = table_expressions(library, table_expr, two, one, threshold);
    cases.push_back(measure(
        prefix + "compile_warm_input_digests", workload, [&] { return compile(expressions); },
        [](const Program &p) {
            check(p.plan()->nodes().size() == 6, "Compiler changed reachable node count");
            return Json{{"nodes", p.plan()->nodes().size()},
                        {"plan_id", p.plan()->id()},
                        {"literal_accounted_bytes", p.plan()->literal_bytes()},
                        {"fresh_catalog_and_plan", true},
                        {"immutable_owner_hashes", "warm"}};
        }));
    const auto program = compile(expressions);
    cases.push_back(measure(
        prefix + "prepared_serial", workload, [&] { return execute_prepared(program, options); },
        [&](const PreparedResult &r) {
            auto report = verify_table(as_data(program, r.outputs), fixture, options);
            report["observed_owner_bytes_at_node_boundaries"] = r.peak_observed_owner_bytes;
            report["lifetime"] = "Harness retains every intermediate until the DAG finishes";
            return report;
        }));
    for (std::size_t workers : {std::size_t{1}, std::size_t{4}}) {
        runtime::Runtime runtime({.workers = workers});
        runtime::JobOptions job;
        job.max_trace_events = 0;
        auto work = workload;
        work["workers"] = workers;
        work["materialization_timed"] = false;
        cases.push_back(measure(
            prefix + "runtime_workers" + std::to_string(workers), work,
            [&] { return runtime.run(program.plan(), job); },
            [&](const runtime::JobResult &r) {
                check(r.status == runtime::JobStatus::succeeded, "Runtime pipeline failed");
                auto report = verify_table(program.materialize(r), fixture, options);
                report["runtime"] = r.diagnostics;
                report["peak_slots"] = runtime.diagnostics().at("peak_slots");
                return report;
            }));
    }
    // This baseline exploits known bounded values and fuses the whole pipeline.
    // It does not implement generic validation, buffers, inference, or lineage.
    cases.push_back(measure(
        prefix + "specialized_fused_host_loop", workload,
        [&] {
            std::vector<std::int64_t> selected;
            std::vector<std::uint8_t> flags;
            std::int64_t total = 0;
            for (std::size_t i = 0; i < rows; ++i)
                if (fixture.validity[i] && fixture.numbers[i] * 2 + 1 > 10) {
                    selected.push_back(fixture.numbers[i]);
                    flags.push_back(fixture.flags[i]);
                    total += fixture.numbers[i] * 2 + 1;
                }
            return std::tuple{std::move(selected), std::move(flags), total};
        },
        [&](const auto &r) {
            check(std::get<0>(r) == fixture.selected_numbers && std::get<1>(r) == fixture.selected_flags &&
                      std::get<2>(r) == fixture.expected_sum,
                  "Specialized loop changed result");
            return Json{{"selected_rows", std::get<0>(r).size()},
                        {"sum", std::get<2>(r)},
                        {"scope", "Known-range fused std::vector baseline; not an equivalent generic "
                                  "contract implementation"}};
        }));
}
void sparse_benchmarks(Json &cases, std::size_t rows) {
    StandardOptions options;
    auto library = std::make_shared<Library>();
    install_standard(*library, options);
    install_sparse(*library, options);
    library->seal();
    std::vector<std::int64_t> indptr{0}, indices, values, vector(rows), expected(rows);
    for (std::size_t i = 0; i < rows; ++i)
        vector[i] = static_cast<std::int64_t>(i % 31) + 1;
    for (std::size_t i = 0; i < rows; ++i) {
        if (i) {
            indices.push_back(static_cast<std::int64_t>(i - 1));
            values.push_back(-1);
        }
        indices.push_back(static_cast<std::int64_t>(i));
        values.push_back(2);
        if (i + 1 < rows) {
            indices.push_back(static_cast<std::int64_t>(i + 1));
            values.push_back(3);
        }
        indptr.push_back(static_cast<std::int64_t>(values.size()));
        // Independent stencil oracle, not iteration over the tested CSR representation.
        expected[i] = 2 * vector[i] - (i ? vector[i - 1] : 0) + (i + 1 < rows ? 3 * vector[i + 1] : 0);
    }
    const auto prefix = "csr_" + std::to_string(rows) + "_";
    Json workload{{"rows", rows},
                  {"columns", rows},
                  {"nnz", values.size()},
                  {"input_int64_slots", indptr.size() + indices.size() + values.size() + vector.size()},
                  {"matrix", "Tridiagonal: left -1, diagonal 2, right 3; explicit sorted CSR"}};
    cases.push_back(measure(
        prefix + "construct", workload, [&] { return csr_i64(rows, rows, indptr, indices, values, options); },
        [&](const DataValue &v) {
            library->validate(v);
            return payloads({v.payload});
        }));
    const auto matrix = literal(library, csr_i64(rows, rows, indptr, indices, values, options));
    const auto vector_expr = literal(library, column_i64(vector, {}, options));
    std::string first_provider_digest;
    for (const auto &provider : {std::string("direct"), std::string("decoded")}) {
        CallOptions call_options;
        call_options.selection.implementation = "gr.sparse.cpu.matvec-" + provider + "@1";
        call_options.selection.backend = "cpu.sequential";
        const auto expr =
            apply(library, "gr.sparse.matvec@1", {matrix, vector_expr}, Json::object(), call_options);
        auto work = workload;
        work["provider"] = call_options.selection.implementation;
        cases.push_back(measure(
            prefix + provider + "_compile_warm_input_digests", work, [&] { return compile({expr}); },
            [](const Program &p) {
                check(p.plan()->nodes().size() == 1, "CSR compile node count");
                return Json{{"plan_id", p.plan()->id()},
                            {"fresh_catalog_and_plan", true},
                            {"immutable_owner_hashes", "warm"}};
            }));
        const auto program = compile({expr});
        cases.push_back(measure(
            prefix + provider + "_prepared", work, [&] { return execute_prepared(program, options); },
            [&](const PreparedResult &r) {
                check(r.outputs.size() == 1, "CSR result count");
                const auto decoded = to_i64(as_data(program, r.outputs)[0], options);
                check(decoded.size() == expected.size(), "CSR result shape");
                for (std::size_t i = 0; i < expected.size(); ++i)
                    check(decoded[i] == expected[i], "CSR result differs from stencil oracle");
                const auto digest = r.outputs[0].as_buffers().content_digest();
                if (first_provider_digest.empty())
                    first_provider_digest = digest;
                check(first_provider_digest == digest, "Independent providers changed output physical bytes");
                auto report = payloads(r.outputs);
                report["observed_owner_bytes_at_node_boundaries"] = r.peak_observed_owner_bytes;
                report["decoded_provider_temporary_vectors"] =
                    "Outside native owner budget, included in elapsed time";
                return report;
            }));
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 2, "Usage: benchmark_data <output.json>");
        Json cases = Json::array();
        for (std::size_t rows : {std::size_t{65536}, std::size_t{262144}})
            table_benchmarks(cases, rows);
        for (std::size_t rows : {std::size_t{4096}, std::size_t{16384}})
            sparse_benchmarks(cases, rows);
        Json environment{{"logical_threads_reported", std::thread::hardware_concurrency()},
                         {"cplusplus", __cplusplus}};
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
            const std::string bytes((std::istreambuf_iterator<char>(executable)), {});
            environment["executable_sha256"] = sha256(bytes);
        }
        const Json report{
            {"format", "gr.data-benchmark/1"},
            {"suite", "native-layer3-v0.5/1"},
            {"environment", environment},
            {"method",
             {{"clock", "steady_clock"},
              {"warmups", 1},
              {"repetitions", repetitions},
              {"correctness_and_hashing_timed", false},
              {"result_destruction_timed", false},
              {"runtime_pools", "Created once per case; warmed before measurements"},
              {"prepared_path", "Serial DAG assembly/execution; all intermediates retained until completion; "
                                "no scheduler/materialization"},
              {"compile_path",
               "Fresh catalog and plan; existing expressions and warmed immutable owner digests"},
              {"memory_scope", "Requested native buffer owner bytes only; excludes temporary vectors, "
                               "metadata, allocator overhead, process RSS"},
              {"statistics", "Five samples; nearest-rank p95 is maximum, not a tail-latency guarantee"}}},
            {"cases", cases}};
        std::ofstream output(argv[1], std::ios::binary);
        check(static_cast<bool>(output), "Cannot open benchmark output");
        output << report.dump(2) << '\n';
        check(static_cast<bool>(output), "Cannot write benchmark output");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Data benchmark failed: " << e.what() << '\n';
        return 1;
    }
}
