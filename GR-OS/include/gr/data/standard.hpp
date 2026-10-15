#pragma once
#include "gr/data/language.hpp"
#include <optional>
#include <span>

namespace gr::data {
struct StandardOptions {
    std::size_t max_elements = 1024 * 1024;
    std::size_t max_columns = 64;
    std::shared_ptr<memory::AllocationBudget> allocation_budget =
        std::make_shared<memory::AllocationBudget>(256 * 1024 * 1024);
};
Type int64_type();
Type boolean_type();
Type float64_type(); // Finite binary64 storage; arithmetic is not installed.
Type optional_type(Type element);
Type column_type(Type element, bool nullable = false);
Type table_type(const std::vector<std::pair<std::string, Type>> &columns);
Type matrix_type(Type element);
void install_standard(Library &, StandardOptions = {});

using Validity = std::optional<std::span<const std::uint8_t>>;
DataValue scalar_i64(std::int64_t, StandardOptions = {});
DataValue optional_i64(std::optional<std::int64_t>, StandardOptions = {});
DataValue column_i64(std::span<const std::int64_t>, Validity = {}, StandardOptions = {});
DataValue column_bool(std::span<const std::uint8_t>, Validity = {}, StandardOptions = {});
DataValue column_f64(std::span<const double>, Validity = {}, StandardOptions = {});
DataValue make_table(const std::vector<std::pair<std::string, DataValue>> &, StandardOptions = {});
DataValue table_column(const DataValue &, std::string_view, StandardOptions = {});
std::vector<std::optional<std::int64_t>> to_i64(const DataValue &, StandardOptions = {});
std::vector<std::optional<bool>> to_bool(const DataValue &, StandardOptions = {});
std::vector<std::optional<double>> to_f64(const DataValue &, StandardOptions = {});
std::int64_t get_scalar_i64(const DataValue &, StandardOptions = {});
std::optional<std::int64_t> get_optional_i64(const DataValue &, StandardOptions = {});

// Versioned representation IDs: gr.data.scalar-le@1, gr.data.column-buffers@1,
// gr.data.table-buffers@1. Nullable columns use an explicit byte-per-slot 0/1
// validity view; this is not an Arrow bitmap or a claim of Arrow conformance.
// Installed operators: gr.data.slice@1, affine@1, compare@1, filter@1, sum@1,
// table-column@1, table-project@1, table-filter@1, each under gr.data namespace.
} // namespace gr::data
