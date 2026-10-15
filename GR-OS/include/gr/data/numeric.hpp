#pragma once
#include "gr/data/standard.hpp"

namespace gr::data::numeric {
struct Complex128 {
    double real = 0, imag = 0;
    bool operator==(const Complex128 &) const = default;
};
Type uint64_type();
Type complex128_type();
void install(Library &, StandardOptions = {});
Json floating_capabilities();
DataValue scalar_u64(std::uint64_t, StandardOptions = {});
DataValue scalar_f64(double, StandardOptions = {});
DataValue scalar_c128(Complex128, StandardOptions = {});
DataValue column_u64(std::span<const std::uint64_t>, Validity = {}, StandardOptions = {});
DataValue column_f64(std::span<const double>, Validity = {}, StandardOptions = {});
DataValue column_c128(std::span<const Complex128>, Validity = {}, StandardOptions = {});
std::vector<std::optional<std::uint64_t>> to_u64(const DataValue &, StandardOptions = {});
std::vector<std::optional<double>> to_f64(const DataValue &, StandardOptions = {});
std::vector<std::optional<Complex128>> to_c128(const DataValue &, StandardOptions = {});
std::optional<std::uint64_t> get_optional_u64(const DataValue &, StandardOptions = {});
std::optional<double> get_optional_f64(const DataValue &, StandardOptions = {});
std::optional<Complex128> get_optional_c128(const DataValue &, StandardOptions = {});
// Same-element columns; no promotion or broadcast. Nullable status propagates.
// gr.numeric.add@1, multiply@1, equal@1; gr.numeric.sum@1 requires explicit
// nulls=skip|propagate and optional min_count (default 1). Binary64 arithmetic
// is bit-specified rounded machine arithmetic on qualified strict host builds,
// not exact arithmetic over mathematical real or complex numbers.
} // namespace gr::data::numeric
