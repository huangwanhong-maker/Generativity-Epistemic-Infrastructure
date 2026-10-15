#include "gr/data/numeric.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <limits>
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif
using namespace gr;
using namespace gr::data;
namespace number = gr::data::numeric;
namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F fn) {
    try {
        fn();
    } catch (const Error &e) {
        check(e.code == code, (std::string("Expected ") + code + ", got " + e.code).c_str());
        return;
    }
    throw std::runtime_error(std::string("Expected ") + code);
}
std::shared_ptr<Library> library() {
    auto l = std::make_shared<Library>();
    install_standard(*l);
    number::install(*l);
    l->seal();
    return l;
}
DataValue evaluate(const std::shared_ptr<Library> &l, const std::string &op,
                   const std::vector<DataValue> &inputs, const Json &attrs = Json::object(),
                   bool scalar = false, std::stop_token stop = {}) {
    std::vector<Expr> literals;
    for (const auto &v : inputs)
        literals.push_back(literal(l, v));
    CallOptions options;
    options.selection.implementation = "gr.numeric.cpu." + op + (scalar ? "-scalar@1" : "-batch@1");
    auto p = compile({apply(l, "gr.numeric." + op + "@1", literals, attrs, options)});
    check(p.plan()->nodes().size() == 1, "Single operation lowered once");
    auto arguments = p.plan()->assemble_inputs(0, {});
    auto result = p.plan()->execute_node(0, arguments, stop, 64 * 1024 * 1024);
    return {p.outputs()[0], result.at(0)};
}
double floating(std::uint64_t bits) {
    return std::bit_cast<double>(bits);
}
std::uint64_t bits(double x) {
    return std::bit_cast<std::uint64_t>(x);
}
void unsigned_tests(const std::shared_ptr<Library> &l) {
    const std::vector<std::uint64_t> a{0, 1, UINT64_MAX, UINT64_MAX - 2}, b{0, 2, 0, 2};
    for (bool scalar : {false, true}) {
        auto out = number::to_u64(
            evaluate(l, "add", {number::column_u64(a), number::column_u64(b)}, Json::object(), scalar));
        check(out == std::vector<std::optional<std::uint64_t>>{0, 3, UINT64_MAX, UINT64_MAX},
              "Full unsigned range");
        check(number::get_optional_u64(number::scalar_u64(UINT64_MAX)) == UINT64_MAX, "Full unsigned scalar");
    }
    const std::vector<std::uint64_t> maximum{UINT64_MAX}, one{1}, two{2};
    fails("numeric_overflow",
          [&] { evaluate(l, "add", {number::column_u64(maximum), number::column_u64(one)}); });
    fails("numeric_overflow",
          [&] { evaluate(l, "multiply", {number::column_u64(maximum), number::column_u64(two)}); });
    const std::vector<std::uint64_t> over{UINT64_MAX, 1, 0};
    const std::vector<std::uint8_t> mask{1, 1, 0};
    const auto nullable = number::column_u64(over, std::span<const std::uint8_t>(mask));
    check(!number::get_optional_u64(evaluate(l, "sum", {nullable}, {{"nulls", "propagate"}})),
          "Missing-first reduction");
    fails("numeric_overflow", [&] { evaluate(l, "sum", {nullable}, {{"nulls", "skip"}}); });
    const std::vector<std::uint64_t> empty;
    check(number::get_optional_u64(
              evaluate(l, "sum", {number::column_u64(empty)}, {{"nulls", "skip"}, {"min_count", 0}})) == 0,
          "Explicit empty identity");
    const std::vector<std::int64_t> signed_one{1};
    fails("data_type", [&] { evaluate(l, "add", {column_i64(signed_one), number::column_u64(one)}); });
    fails("data_shape", [&] { evaluate(l, "add", {number::column_u64(a), number::column_u64(one)}); });
    std::stop_source cancelled;
    cancelled.request_stop();
    fails("execution_cancelled", [&] {
        evaluate(l, "sum", {number::column_u64(a)}, {{"nulls", "skip"}}, false, cancelled.get_token());
    });
    // Strided numeric input is imported explicitly, preserving original ownership.
    const auto original = number::column_u64(a);
    auto reversed = original;
    reversed.payload = kernel::Value::buffers(
        "fixture.numeric@1",
        memory::BufferSet({{"values", original.payload.as_buffers().at("values").slice(0, 3, 4, -1)}}));
    const std::vector<std::uint64_t> zero(4);
    check(number::to_u64(evaluate(l, "add", {reversed, number::column_u64(zero)})) ==
              std::vector<std::optional<std::uint64_t>>{UINT64_MAX - 2, UINT64_MAX, 1, 0},
          "Negative numeric strides");
}
void cross_module_limits() {
    auto l = std::make_shared<Library>();
    install_standard(*l);
    StandardOptions bounded;
    bounded.max_elements = 2;
    bounded.allocation_budget = std::make_shared<memory::AllocationBudget>(1024);
    number::install(*l, bounded);
    l->seal();
    const std::vector<std::int64_t> values{1, 2, 3};
    const auto input = column_i64(values);
    for (bool scalar : {false, true}) {
        fails("data_limit", [&] { evaluate(l, "add", {input, input}, Json::object(), scalar); });
        fails("data_limit", [&] { evaluate(l, "sum", {input}, {{"nulls", "skip"}}, scalar); });
        check(bounded.allocation_budget->used() == 0, "Cross-module limit fails before allocation");
    }
}
void floating_tests(const std::shared_ptr<Library> &l) {
    const std::vector<double> a{1, 1, -0.0, floating(1), 1e16, -1e16, std::numeric_limits<double>::max()};
    const std::vector<double> b{
        floating(0x3ca0000000000000ULL),    floating(0x3cb8000000000000ULL), -0.0, floating(1), -1e16, 1,
        -std::numeric_limits<double>::max()};
    const std::vector<std::uint64_t> expected{
        0x3ff0000000000000ULL, 0x3ff0000000000002ULL, 0x8000000000000000ULL, 2, 0, bits(-1e16), 0};
    std::string digest;
    for (bool scalar : {false, true}) {
        auto result =
            evaluate(l, "add", {number::column_f64(a), number::column_f64(b)}, Json::object(), scalar);
        const auto decoded = number::to_f64(result);
        for (std::size_t i = 0; i < expected.size(); ++i)
            check(bits(*decoded[i]) == expected[i], "Independent IEEE binary64 bit fixture");
        const auto now = result.payload.as_buffers().content_digest();
        if (digest.empty())
            digest = now;
        check(digest == now, "Scalar/batch provider physical results agree");
    }
    const std::vector<double> tiny{floating(0x0010000000000000ULL), floating(1),
                                   floating(0x8000000000000001ULL)};
    const std::vector<double> halves(3, 0.5);
    const auto products =
        number::to_f64(evaluate(l, "multiply", {number::column_f64(tiny), number::column_f64(halves)}));
    check(bits(*products[0]) == 0x0008000000000000ULL && bits(*products[1]) == 0 &&
              bits(*products[2]) == 0x8000000000000000ULL,
          "Gradual subnormal underflow and signed zero ties");
    const std::vector<double> first{1e16, 1, -1e16}, second{1e16, -1e16, 1};
    check(
        number::get_optional_f64(evaluate(l, "sum", {number::column_f64(first)}, {{"nulls", "skip"}})) == 0 &&
            number::get_optional_f64(evaluate(l, "sum", {number::column_f64(second)}, {{"nulls", "skip"}})) ==
                1,
        "Ordered machine sum is intentionally not exact real summation");
    const std::vector<double> zero{0.0}, negative{-0.0};
    check(to_bool(evaluate(l, "equal", {number::column_f64(zero), number::column_f64(negative)}))[0] == true,
          "Numeric zero equality differs from physical identity");
    for (auto bad : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
                     floating(0x7ff0000000000001ULL)}) {
        const std::vector<double> invalid{bad};
        fails("numeric_nonfinite", [&] { number::column_f64(invalid); });
    }
    const std::vector<double> maximum{std::numeric_limits<double>::max()}, two{2};
    fails("numeric_overflow",
          [&] { evaluate(l, "multiply", {number::column_f64(maximum), number::column_f64(two)}); });
    const std::vector<std::uint8_t> absent{0};
    check(!number::to_f64(evaluate(l, "multiply",
                                   {number::column_f64(maximum, std::span<const std::uint8_t>(absent)),
                                    number::column_f64(two)}))[0],
          "Missing finite binary64 slots do not execute overflowing arithmetic");
    // Deliberately hostile caller environment. Provider restores rounding, FTZ/DAZ and flags.
    std::fenv_t original;
    check(std::fegetenv(&original) == 0, "Save test environment");
#if defined(_M_X64) || defined(__x86_64__)
    const auto original_csr = _mm_getcsr();
#endif
    check(std::fesetround(FE_UPWARD) == 0, "Select hostile rounding");
    std::feraiseexcept(FE_INEXACT);
#if defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr(_mm_getcsr() | (1u << 15) | (1u << 6));
    const auto hostile = _mm_getcsr();
#endif
    auto underflow = evaluate(l, "multiply", {number::column_f64(tiny), number::column_f64(halves)});
    fails("numeric_overflow",
          [&] { evaluate(l, "multiply", {number::column_f64(maximum), number::column_f64(two)}); });
    const auto observed_round = std::fegetround();
#if defined(_M_X64) || defined(__x86_64__)
    const auto observed_csr = _mm_getcsr();
#endif
    std::fesetenv(&original);
#if defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr(original_csr);
    check(observed_csr == hostile, "Restore exact MXCSR including flags and FTZ/DAZ");
#endif
    check(observed_round == FE_UPWARD, "Restore caller rounding");
    check(bits(*number::to_f64(underflow)[0]) == 0x0008000000000000ULL,
          "Hostile FTZ cannot change provider output");
}
void complex_tests(const std::shared_ptr<Library> &l) {
    const std::vector<number::Complex128> a{{1, 2}, {-0.0, 1}, {3, -2}}, b{{3, 4}, {1, 0}, {-4, 5}};
    for (bool scalar : {false, true}) {
        const auto out = number::to_c128(evaluate(
            l, "multiply", {number::column_c128(a), number::column_c128(b)}, Json::object(), scalar));
        check(out[0] == number::Complex128{-5, 10} && out[2] == number::Complex128{-2, 23},
              "Complex specified component multiplication sequence");
        const std::vector<number::Complex128> rounded_a{{floating(0x3ff0000000000001ULL), 1}},
            rounded_b{{floating(0x3feffffffffffffeULL), 1}};
        const auto rounded = number::to_c128(
            evaluate(l, "multiply", {number::column_c128(rounded_a), number::column_c128(rounded_b)},
                     Json::object(), scalar));
        check(bits(rounded[0]->real) == 0 && bits(rounded[0]->imag) == 0x4000000000000000ULL,
              "Complex products round separately before subtraction; fused real component would differ");
    }
    const std::vector<std::uint8_t> mask{1, 0, 1};
    auto sum = evaluate(l, "sum", {number::column_c128(a, std::span<const std::uint8_t>(mask))},
                        {{"nulls", "skip"}});
    check(number::get_optional_c128(sum) == number::Complex128{4, 0}, "Nullable complex component sum");
    const double maximum = std::numeric_limits<double>::max();
    const std::vector<number::Complex128> large{{maximum, maximum}}, cancel{{2, 2}};
    fails("numeric_overflow",
          [&] { evaluate(l, "multiply", {number::column_c128(large), number::column_c128(cancel)}); });
    const std::vector<std::uint8_t> absent{0};
    check(!number::to_c128(evaluate(l, "multiply",
                                    {number::column_c128(large, std::span<const std::uint8_t>(absent)),
                                     number::column_c128(cancel)}))[0],
          "Missing finite complex slots do not execute overflowing intermediate products");
    StandardOptions small;
    small.allocation_budget = std::make_shared<memory::AllocationBudget>(16);
    const std::vector<number::Complex128> one{{1, 2}};
    const std::vector<std::uint8_t> valid{1};
    fails("buffer_budget", [&] { number::column_c128(one, std::span<const std::uint8_t>(valid), small); });
    check(small.allocation_budget->used() == 0, "Partial complex allocation rolls back");
}
} // namespace
int main() {
    try {
        auto l = library();
        unsigned_tests(l);
        cross_module_limits();
        if (number::floating_capabilities().at("supported").get<bool>()) {
            floating_tests(l);
            complex_tests(l);
        } else {
            const std::vector<double> one{1};
            fails("unsupported_numeric_environment",
                  [&] { evaluate(l, "add", {number::column_f64(one), number::column_f64(one)}); });
            std::cout << "Floating provider fixtures skipped: build is explicitly unsupported\n";
        }
        std::cout << "numeric data: " << checks << " checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
