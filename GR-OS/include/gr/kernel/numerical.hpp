#pragma once
#include "gr/common.hpp"
#include <optional>
#include <string>

namespace gr::kernel {
enum class IntegerOverflow { NotApplicable, Error, Modulo, Saturate };
enum class Rounding { NotApplicable, NearestEven, TowardZero, TowardPositive, TowardNegative };
enum class NonFinite { NotApplicable, Reject, IEEE };
enum class ReductionOrder { NotApplicable, InputOrder, FixedTree, Unspecified };
// Separate from mathematical accuracy: repeatability has a stated scope.
enum class Repeatability { AcrossProviders, SameImplementation, Unspecified };
struct ErrorTolerance {
    // Finite nonnegative decimal strings: no change to canonical JSON's integer domain.
    std::string absolute = "0", relative = "0";
    std::string reference_id;
    Json to_json() const;
    void validate() const;
};
struct NumericalContract {
    std::string profile_id = "gr.numeric-contract@1";
    std::string storage_format, accumulation_format;
    IntegerOverflow integer_overflow = IntegerOverflow::NotApplicable;
    Rounding rounding = Rounding::NotApplicable;
    NonFinite nonfinite = NonFinite::NotApplicable;
    ReductionOrder reduction_order = ReductionOrder::NotApplicable;
    Repeatability repeatability = Repeatability::AcrossProviders;
    bool reassociation = false;
    std::optional<ErrorTolerance> tolerance;
    void validate() const;
    Json to_json() const;
};
// Explicit checked primitives; never invoke signed-overflow undefined behavior.
std::int64_t checked_add(std::int64_t, std::int64_t);
std::int64_t checked_multiply(std::int64_t, std::int64_t);
} // namespace gr::kernel
