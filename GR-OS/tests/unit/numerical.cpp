#include "gr/kernel/operations.hpp"
#include <iostream>
#include <limits>
using namespace gr;
using namespace gr::kernel;
namespace {
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F function) {
    try {
        function();
    } catch (const Error &error) {
        check(error.code == code, "Unexpected numeric error code");
        return;
    }
    throw std::runtime_error("Expected numeric failure");
}
} // namespace
int main() {
    try {
        constexpr auto low = std::numeric_limits<std::int64_t>::min();
        constexpr auto high = std::numeric_limits<std::int64_t>::max();
        check(checked_add(low, high) == -1 && checked_add(high, -1) == high - 1,
              "Checked addition boundaries");
        check(checked_multiply(low, 1) == low && checked_multiply(low, 0) == 0,
              "Minimum int64 multiplication");
        check(checked_multiply(-3, -7) == 21 && checked_multiply(high, 1) == high, "Multiply signs");
        fails("numeric_overflow", [&] { checked_add(high, 1); });
        fails("numeric_overflow", [&] { checked_add(low, -1); });
        fails("numeric_overflow", [&] { checked_multiply(low, -1); });
        fails("numeric_overflow", [&] { checked_multiply(high, 2); });
        fails("numeric_overflow", [&] { checked_add(checked_add(high, 1), -1); });
        check(checked_add(high, checked_add(1, -1)) == high, "Reassociation changes overflow observability");
        for (int a = -128; a <= 127; ++a)
            for (int b = -128; b <= 127; ++b)
                check(checked_multiply(a, b) == static_cast<std::int64_t>(a * b),
                      "Independent narrow-product oracle");
        NumericalContract numeric;
        numeric.storage_format = numeric.accumulation_format = "gr.int64@1";
        numeric.integer_overflow = IntegerOverflow::Error;
        numeric.reduction_order = ReductionOrder::InputOrder;
        numeric.validate();
        auto invalid = numeric;
        invalid.reassociation = true;
        fails("numerical_contract", [&] { invalid.validate(); });
        ErrorTolerance tolerance{"1e-12", "1e-8", "fixture.exact-reference@1"};
        tolerance.validate();
        for (const auto text : {"nan", "inf", "-1", "-0", "1e9999", " 1", "1x"}) {
            auto bad = tolerance;
            bad.absolute = text;
            fails("numerical_contract", [&] { bad.validate(); });
        }
        ValueTypeRegistry types;
        OperationRegistry operations;
        auto operation = operations.describe("gr.bytes.identity@1");
        check(!operation.to_json().contains("numerical"), "Existing operation serialization unchanged");
        const auto original = operation.fingerprint();
        operation.numerical = numeric;
        check(operation.fingerprint() != original, "Numerical guarantee participates in exact fingerprint");
        operation.id = "fixture.numeric@1";
        operations.register_operation(operation);
        check(operations.describe(operation.id).numerical->integer_overflow == IntegerOverflow::Error,
              "Registered numerical descriptor remains inspectable");
        operation.id = "fixture.invalid-numeric@1";
        operation.numerical->repeatability = Repeatability::Unspecified;
        fails("numerical_contract", [&] { operations.register_operation(operation); });
        std::cout << "numerical: " << checks << " checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << "numerical failure: " << error.what() << '\n';
        return 1;
    }
}
