#include "gr/kernel/numerical.hpp"
#include "gr/kernel/types.hpp"
#include <charconv>
#include <cmath>
#include <limits>

namespace gr::kernel {
namespace {
template <class Enum> const char *name(Enum value);
template <> const char *name(IntegerOverflow value) {
    switch (value) {
    case IntegerOverflow::NotApplicable:
        return "not_applicable";
    case IntegerOverflow::Error:
        return "error";
    case IntegerOverflow::Modulo:
        return "modulo";
    case IntegerOverflow::Saturate:
        return "saturate";
    }
    throw Error("numerical_contract", "Unknown integer overflow policy.");
}
template <> const char *name(Rounding value) {
    switch (value) {
    case Rounding::NotApplicable:
        return "not_applicable";
    case Rounding::NearestEven:
        return "nearest_even";
    case Rounding::TowardZero:
        return "toward_zero";
    case Rounding::TowardPositive:
        return "toward_positive";
    case Rounding::TowardNegative:
        return "toward_negative";
    }
    throw Error("numerical_contract", "Unknown rounding policy.");
}
template <> const char *name(NonFinite value) {
    switch (value) {
    case NonFinite::NotApplicable:
        return "not_applicable";
    case NonFinite::Reject:
        return "reject";
    case NonFinite::IEEE:
        return "ieee";
    }
    throw Error("numerical_contract", "Unknown nonfinite policy.");
}
template <> const char *name(ReductionOrder value) {
    switch (value) {
    case ReductionOrder::NotApplicable:
        return "not_applicable";
    case ReductionOrder::InputOrder:
        return "input_order";
    case ReductionOrder::FixedTree:
        return "fixed_tree";
    case ReductionOrder::Unspecified:
        return "unspecified";
    }
    throw Error("numerical_contract", "Unknown reduction ordering.");
}
template <> const char *name(Repeatability value) {
    switch (value) {
    case Repeatability::AcrossProviders:
        return "across_providers";
    case Repeatability::SameImplementation:
        return "same_implementation";
    case Repeatability::Unspecified:
        return "unspecified";
    }
    throw Error("numerical_contract", "Unknown repeatability scope.");
}
void decimal(const std::string &text) {
    if (text.empty() || text.size() > 128)
        throw Error("numerical_contract", "Tolerance needs a bounded decimal string.");
    double value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size() || !std::isfinite(value) || value < 0 ||
        std::signbit(value))
        throw Error("numerical_contract", "Tolerance needs a finite nonnegative decimal string.");
}
} // namespace
Json ErrorTolerance::to_json() const {
    return {{"absolute", absolute},
            {"relative", relative},
            {"reference_id", reference_id},
            {"comparison", "abs(actual-reference) <= absolute + relative*abs(reference)"}};
}
void ErrorTolerance::validate() const {
    decimal(absolute);
    decimal(relative);
    validate_qualified_id(reference_id);
}
void NumericalContract::validate() const {
    validate_qualified_id(profile_id);
    validate_qualified_id(storage_format);
    validate_qualified_id(accumulation_format);
    if (reassociation && reduction_order != ReductionOrder::Unspecified)
        throw Error("numerical_contract",
                    "Reassociation requires an explicitly unspecified reduction order.");
    if (tolerance)
        tolerance->validate();
    canonical(to_json()); // Also validates enum values and portable serialization.
}
Json NumericalContract::to_json() const {
    Json result{{"profile_id", profile_id},
                {"storage_format", storage_format},
                {"accumulation_format", accumulation_format},
                {"integer_overflow", name(integer_overflow)},
                {"rounding", name(rounding)},
                {"nonfinite", name(nonfinite)},
                {"reduction_order", name(reduction_order)},
                {"repeatability", name(repeatability)},
                {"reassociation", reassociation}};
    if (tolerance)
        result["tolerance"] = tolerance->to_json();
    return result;
}
std::int64_t checked_add(std::int64_t a, std::int64_t b) {
    constexpr auto low = std::numeric_limits<std::int64_t>::min();
    constexpr auto high = std::numeric_limits<std::int64_t>::max();
    if ((b > 0 && a > high - b) || (b < 0 && a < low - b))
        throw Error("numeric_overflow", "Signed int64 addition overflow.");
    return a + b;
}
std::int64_t checked_multiply(std::int64_t a, std::int64_t b) {
    const auto magnitude = [](std::int64_t x) {
        return x < 0 ? static_cast<std::uint64_t>(-(x + 1)) + 1 : static_cast<std::uint64_t>(x);
    };
    const auto left = magnitude(a), right = magnitude(b);
    const bool negative = (a < 0) != (b < 0);
    const auto limit =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + (negative ? 1 : 0);
    if (left && right > limit / left)
        throw Error("numeric_overflow", "Signed int64 multiplication overflow.");
    const auto product = left * right;
    if (negative && product == limit)
        return std::numeric_limits<std::int64_t>::min();
    return negative ? -static_cast<std::int64_t>(product) : static_cast<std::int64_t>(product);
}
} // namespace gr::kernel
