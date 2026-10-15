#include "gr/data/standard.hpp"
#include <bit>
#include <iostream>
#include <limits>
using namespace gr;
using namespace gr::data;
namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F f) {
    try {
        f();
    } catch (const Error &e) {
        check(e.code == code, (std::string("Expected ") + code + ", got " + e.code).c_str());
        return;
    }
    throw std::runtime_error(std::string("Expected ") + code);
}
std::vector<DataValue> evaluate(const std::vector<Expr> &outputs, bool serial = false,
                                const Bindings &bindings = {}) {
    auto program = compile(outputs, bindings);
    runtime::Runtime runtime({.workers = 2});
    runtime::JobOptions options;
    options.serial = serial;
    auto result = runtime.run(program.plan(), options);
    if (result.status != runtime::JobStatus::succeeded)
        throw Error(result.error.value("code", std::string("failed")),
                    result.error.value("message", std::string("Failed data program")));
    return program.materialize(result);
}
Expr lit(const std::shared_ptr<Library> &l, DataValue v) {
    return literal(l, std::move(v));
}
void pipeline() {
    auto library = std::make_shared<Library>();
    install_standard(*library);
    library->seal();
    const std::vector<std::int64_t> numbers{1, INT64_MAX, 3, 5};
    const std::vector<std::uint8_t> mask{1, 0, 1, 1}, flags{1, 0, 0, 1};
    const std::vector<double> real{0.5, -0.0, 2.5, 4.5};
    const auto column = column_i64(numbers, std::span<const std::uint8_t>(mask));
    const auto table =
        make_table({{"number", column}, {"flag", column_bool(flags)}, {"real", column_f64(real)}});
    library->validate(table);
    const auto table_expr = lit(library, table);
    const auto field = apply(library, "gr.data.table-column@1", {table_expr}, {{"name", "number"}});
    const auto transformed =
        apply(library, "gr.data.affine@1", {field, lit(library, scalar_i64(2)), lit(library, scalar_i64(1))});
    const auto predicate =
        apply(library, "gr.data.compare@1", {transformed, lit(library, scalar_i64(5))}, {{"relation", "gt"}});
    const auto selected =
        apply(library, "gr.data.table-filter@1", {table_expr, predicate}, {{"nulls", "drop"}});
    const auto selected_numbers =
        apply(library, "gr.data.filter@1", {transformed, predicate}, {{"nulls", "drop"}});
    const auto total =
        apply(library, "gr.data.sum@1", {selected_numbers}, {{"nulls", "skip"}, {"min_count", 1}});
    const auto result = evaluate({transformed, selected, total});
    check(to_i64(result[0]) == std::vector<std::optional<std::int64_t>>{3, {}, 7, 11},
          "Affine skips missing overflowing slots");
    check(to_i64(table_column(result[1], "number")) == std::vector<std::optional<std::int64_t>>{3, 5},
          "Stable table filter preserves row association");
    check(to_bool(table_column(result[1], "flag")) == std::vector<std::optional<bool>>{false, true},
          "Heterogeneous Boolean rows preserved");
    check(to_f64(table_column(result[1], "real")) == std::vector<std::optional<double>>{2.5, 4.5},
          "Finite float storage participates without float arithmetic");
    check(get_optional_i64(result[2]) == 18, "Ordered reduction of selected transformed rows");
    const auto serial = evaluate({transformed, selected, total}, true);
    check(to_i64(serial[0]) == to_i64(result[0]) &&
              get_optional_i64(serial[2]) == get_optional_i64(result[2]),
          "Serial and parallel logical results agree");
    fails("data_missing", [&] {
        evaluate({apply(library, "gr.data.filter@1", {transformed, predicate}, {{"nulls", "error"}})});
    });
    const auto projected =
        evaluate({apply(library, "gr.data.table-project@1", {selected}, {{"names", {"flag", "number"}}})})[0];
    check(projected.spec.type.parameters.at("names") == Json({"flag", "number"}),
          "Projection preserves explicit field order");
    const auto empty_projection =
        evaluate({apply(library, "gr.data.table-project@1", {table_expr}, {{"names", Json::array()}})})[0];
    check(empty_projection.payload.as_buffers().metadata().at("rows") == 4 &&
              empty_projection.payload.as_buffers().buffers().empty(),
          "Zero-column table retains row count");
    fails("data_field", [&] {
        apply(library, "gr.data.table-project@1", {table_expr}, {{"names", {"number", "number"}}});
    });
    const auto reverse =
        evaluate({apply(library, "gr.data.slice@1", {field}, {{"start", 3}, {"count", 4}, {"step", -1}})})[0];
    check(to_i64(reverse) == std::vector<std::optional<std::int64_t>>{5, 3, {}, 1},
          "Negative slice preserves mask and order");
    check(reverse.payload.as_buffers().at("values").buffer().owner_id() ==
              column.payload.as_buffers().at("values").buffer().owner_id(),
          "Slice retains original owner without payload copy");
    const auto reversed_affine = evaluate(
        {apply(library, "gr.data.affine@1",
               {lit(library, reverse), lit(library, scalar_i64(2)), lit(library, scalar_i64(1))})})[0];
    check(to_i64(reversed_affine) == std::vector<std::optional<std::int64_t>>{11, 7, {}, 3},
          "Batch arithmetic preserves reversed value and validity strides");
    const std::vector<std::int64_t> singleton{2};
    auto broadcast = column_i64(singleton);
    const auto &original_view = broadcast.payload.as_buffers().at("values");
    broadcast.payload = kernel::Value::buffers(
        "fixture.broadcast@1",
        memory::BufferSet(
            {{"values", memory::BufferView(original_view.buffer(), original_view.element(), {4}, {0})}}));
    check(get_optional_i64(evaluate(
              {apply(library, "gr.data.sum@1", {lit(library, broadcast)}, {{"nulls", "skip"}})})[0]) == 8,
          "Batch reduction preserves zero-stride broadcast layout");
    check(std::bit_cast<std::uint64_t>(*to_f64(column_f64(real))[1]) == std::bit_cast<std::uint64_t>(-0.0),
          "Signed zero storage survives exactly");
    fails("data_type", [&] {
        apply(library, "gr.data.affine@1",
              {lit(library, column_f64(real)), lit(library, scalar_i64(1)), lit(library, scalar_i64(0))});
    });
    const auto template_input = input(library, "chunk", column.spec);
    const auto template_sum =
        apply(library, "gr.data.sum@1", {template_input}, {{"nulls", "skip"}, {"min_count", 1}});
    check(get_optional_i64(evaluate({template_sum}, false, {{"chunk", column}})[0]) == 9,
          "Reusable logical input binds owned chunk values");
}
void numerical_and_missing() {
    auto library = std::make_shared<Library>();
    install_standard(*library);
    library->seal();
    auto sum = [&](DataValue v, std::string policy, std::size_t minimum = 1) {
        return evaluate({apply(library, "gr.data.sum@1", {lit(library, std::move(v))},
                               {{"nulls", policy}, {"min_count", minimum}})})[0];
    };
    const std::vector<std::int64_t> adversary{INT64_MAX, 1, -1};
    fails("numeric_overflow", [&] { sum(column_i64(adversary), "skip"); });
    const std::vector<std::int64_t> safe{INT64_MAX, -1, 1};
    check(get_optional_i64(sum(column_i64(safe), "skip")) == INT64_MAX,
          "Reduction order is semantically observable");
    const std::vector<std::uint8_t> missing{1, 1, 0};
    check(!get_optional_i64(sum(column_i64(adversary, std::span<const std::uint8_t>(missing)), "propagate")),
          "Declared missing-first propagation avoids arithmetic before returning missing");
    fails("numeric_overflow",
          [&] { sum(column_i64(adversary, std::span<const std::uint8_t>(missing)), "skip"); });
    const std::vector<std::int64_t> empty;
    check(!get_optional_i64(sum(column_i64(empty), "skip")), "Empty reduction missing with min_count one");
    check(get_optional_i64(sum(column_i64(empty), "skip", 0)) == 0,
          "Explicit min_count zero yields additive identity");
    check(get_scalar_i64(scalar_i64(INT64_MIN)) == INT64_MIN &&
              get_scalar_i64(scalar_i64(INT64_MAX)) == INT64_MAX,
          "Full int64 scalar operands avoid JSON numeric clipping");
    const std::vector<std::int64_t> minimum{INT64_MIN};
    fails("numeric_overflow", [&] {
        evaluate({apply(
            library, "gr.data.affine@1",
            {lit(library, column_i64(minimum)), lit(library, scalar_i64(-1)), lit(library, scalar_i64(0))})});
    });
    const std::vector<std::uint8_t> bad{2};
    fails("data_boolean", [&] { column_bool(bad); });
    const std::vector<std::int64_t> one{1};
    fails("data_validity", [&] { column_i64(one, std::span<const std::uint8_t>(bad)); });
    const std::vector<double> nan{std::numeric_limits<double>::quiet_NaN()},
        inf{std::numeric_limits<double>::infinity()};
    fails("data_nonfinite", [&] { column_f64(nan); });
    fails("data_nonfinite", [&] { column_f64(inf); });
}
void bounds_and_ownership() {
    StandardOptions o;
    o.max_elements = 3;
    o.allocation_budget = std::make_shared<memory::AllocationBudget>(24);
    const std::vector<std::int64_t> values{1, 2, 3}, too_many{1, 2, 3, 4};
    fails("data_limit", [&] { column_i64(too_many, {}, o); });
    {
        auto value = column_i64(values, {}, o);
        check(o.allocation_budget->used() == 24, "Constructor charges requested owner allocation");
        auto table = make_table({{"x", value}}, o);
        value = {};
        check(o.allocation_budget->used() == 24, "Table shares and retains its column allocation");
        auto extracted = table_column(table, "x", o);
        table = {};
        check(to_i64(extracted, o).size() == 3 && o.allocation_budget->used() == 24,
              "Extracted column outlives table handle");
        fails("buffer_budget", [&] { scalar_i64(7, o); });
    }
    check(o.allocation_budget->used() == 0, "Final owner releases allocation budget");
    const std::vector<std::uint8_t> validity{1, 1, 1};
    fails("buffer_budget", [&] { column_i64(values, std::span<const std::uint8_t>(validity), o); });
    check(o.allocation_budget->used() == 0, "Partially allocated constructor rolls back budget charge");
    fails("data_shape", [&] {
        make_table(
            {{"a", column_i64(values)}, {"b", column_i64(std::span<const std::int64_t>(values).first(1))}});
    });
    // Public helpers accept DataValue directly, so malformed logical schemas
    // must fail even when the caller never registered/validated a Library.
    auto malformed = make_table({});
    malformed.spec.type.parameters["names"] = Json::array({"ghost"});
    fails("data_fields", [&] { table_column(malformed, "ghost"); });
    auto extra = column_i64(values);
    extra.spec.type.parameters["unregistered_semantics"] = true;
    fails("data_attributes", [&] { to_i64(extra); });
    fails("data_attributes", [&] { make_table({{"x", extra}}); });
    auto duplicate = make_table({{"a", column_i64(values)}, {"b", column_i64(values)}});
    duplicate.spec.type.parameters["names"] = Json::array({"same", "same"});
    fails("data_fields", [&] { table_column(duplicate, "same"); });
}
} // namespace
int main() {
    try {
        pipeline();
        numerical_and_missing();
        bounds_and_ownership();
        std::cout << "data standard: " << checks << " checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << "data standard failure: " << e.what() << '\n';
        return 1;
    }
}
