#include "gr/data/standard.hpp"
#include "gr/kernel/numerical.hpp"
#include "gr/memory/access.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <set>

namespace gr::data {
namespace {
constexpr const char *tag = "gr.data.payload@1";
constexpr const char *scalar_rep = "gr.data.scalar-le@1";
constexpr const char *column_rep = "gr.data.column-buffers@1";
constexpr const char *table_rep = "gr.data.table-buffers@1";
void need(bool condition, const char *code, const char *message) {
    if (!condition)
        throw Error(code, message);
}
void active(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Data operation cancelled.");
}
void options_valid(const StandardOptions &o) {
    need(o.max_elements > 0 && o.max_elements <= 16 * 1024 * 1024 && o.max_columns > 0 &&
             o.max_columns <= 64 && bool(o.allocation_budget),
         "data_options", "Data limits require 1..16777216 elements, 1..64 columns and an allocation budget.");
}
void keys(const Json &j, std::initializer_list<const char *> allowed) {
    need(j.is_object(), "data_attributes", "Attributes must be an object.");
    std::set<std::string> names(allowed.begin(), allowed.end());
    for (auto i = j.begin(); i != j.end(); ++i)
        need(names.contains(i.key()), "data_attributes", "Unknown attribute.");
}
std::size_t count(const Json &j, const char *name, std::size_t fallback) {
    if (!j.contains(name))
        return fallback;
    const auto &v = j.at(name);
    need(v.is_number_integer() && v >= 0 &&
             v.get<std::uint64_t>() <= static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()),
         "data_attributes", "Expected an addressable nonnegative integer.");
    return v.get<std::size_t>();
}
std::string text(const Json &j, const char *name) {
    need(j.contains(name) && j.at(name).is_string(), "data_attributes", "Required string attribute absent.");
    return j.at(name).get<std::string>();
}
bool primitive(const Type &t) {
    return (t.family == "gr.data.int64@1" || t.family == "gr.data.boolean@1" ||
            t.family == "gr.data.float64@1") &&
           t.arguments.empty() && t.parameters == Json::object();
}
bool optional(const Type &t) {
    return t.family == "gr.data.optional@1";
}
const Type &scalar_element(const Type &t) {
    return optional(t) ? t.arguments.at(0) : t;
}
bool nullable(const Type &t) {
    return t.parameters.at("nullable").get<bool>();
}
void column_parameters(const Type &t) {
    keys(t.parameters, {"nullable"});
    need(t.parameters.contains("nullable") && t.parameters.at("nullable").is_boolean(), "data_type",
         "Column needs explicit Boolean nullable parameter.");
}
void table_parameters(const Type &t) {
    keys(t.parameters, {"names"});
    need(t.parameters.contains("names") && t.parameters.at("names").is_array() &&
             t.parameters.at("names").size() == t.arguments.size(),
         "data_fields", "Names and table fields differ.");
    std::set<std::string> unique;
    for (const auto &name : t.parameters.at("names"))
        need(name.is_string() && !name.get_ref<const std::string &>().empty() &&
                 name.get_ref<const std::string &>().size() <= 256 &&
                 unique.insert(name.get<std::string>()).second,
             "data_fields", "Table names must be bounded, nonempty and unique.");
    for (const auto &child : t.arguments)
        need(child.family == "gr.data.column@1", "data_type", "Table fields require column types.");
}
bool is_column(const ValueSpec &s) {
    return s.type.family == "gr.data.column@1" && s.representation == column_rep;
}
bool is_table(const ValueSpec &s) {
    return s.type.family == "gr.data.table@1" && s.representation == table_rep;
}
memory::ElementDescriptor element(const Type &t) {
    if (t == int64_type())
        return {"gr.int64@1", 8, 8, memory::ByteOrder::Little, Json::object()};
    if (t == boolean_type())
        return {"gr.boolean-u8@1", 1, 1, memory::ByteOrder::NotApplicable, Json::object()};
    if (t == float64_type())
        return {"gr.float64-ieee754@1", 8, 8, memory::ByteOrder::Little, Json::object()};
    throw Error("data_type", "Physical scalar representation is unsupported.");
}
std::uint64_t bits(std::span<const std::uint8_t> bytes) {
    if (bytes.size() == 8)
        return memory::load_little<std::uint64_t>(bytes);
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        result |= std::uint64_t(bytes[i]) << (8 * i);
    return result;
}
void put(std::span<std::uint8_t> bytes, std::uint64_t value) {
    if (bytes.size() == 8) {
        memory::store_little(bytes, value);
        return;
    }
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
const memory::BufferView &values(const kernel::Value &v) {
    return v.as_buffers().at("values");
}
std::optional<memory::ElementRange> validity_range(const kernel::Value &v, bool has_mask) {
    if (has_mask)
        return memory::ElementRange(v.as_buffers().at("validity"));
    return {};
}
bool valid(const std::optional<memory::ElementRange> &mask, std::size_t index) {
    return !mask || mask->at(index)[0] != 0;
}
kernel::Value payload(memory::BufferView view, std::optional<memory::BufferView> validity = {}) {
    std::vector<memory::NamedBuffer> buffers{{"values", std::move(view)}};
    if (validity)
        buffers.push_back({"validity", std::move(*validity)});
    return kernel::Value::buffers(tag, memory::BufferSet(std::move(buffers)));
}
memory::MutableBuffer allocate(std::size_t n, std::size_t width, const StandardOptions &o) {
    need(n <= o.max_elements && n <= std::numeric_limits<std::size_t>::max() / width, "data_limit",
         "Element count exceeds data limit.");
    return memory::MutableBuffer::allocate(n * width, 64, o.allocation_budget);
}
memory::BufferView finish(memory::MutableBuffer builder, const Type &t, std::vector<std::size_t> shape) {
    return {std::move(builder).freeze(), element(t), std::move(shape)};
}
void mask_valid(const memory::BufferView &v, std::size_t n, bool scalar) {
    need(v.element() == element(boolean_type()) &&
             v.shape() == (scalar ? std::vector<std::size_t>{} : std::vector<std::size_t>{n}),
         "data_layout", "Invalid validity view geometry or encoding.");
    if (scalar)
        need(v.element_bytes({})[0] <= 1, "data_validity", "Validity must be 0 or 1.");
    else
        memory::ElementRange(v).for_each([](std::size_t, auto bytes) {
            need(bytes[0] <= 1, "data_validity", "Validity must be 0 or 1.");
        });
}
void encoded_valid(const memory::BufferView &v, const Type &t, bool scalar) {
    if (t == int64_type())
        return;
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
                  "Finite binary64 storage requires IEEE binary64 double.");
    const bool boolean = t == boolean_type();
    const auto validate = [boolean](std::span<const std::uint8_t> bytes) {
        if (boolean)
            need(bytes[0] <= 1, "data_boolean", "Boolean encoding must be 0 or 1.");
        else
            need(std::isfinite(std::bit_cast<double>(bits(bytes))), "data_nonfinite",
                 "Only finite binary64 values are supported.");
    };
    if (scalar)
        validate(v.element_bytes({}));
    else
        memory::ElementRange(v).for_each([&](std::size_t, auto bytes) { validate(bytes); });
}
void validate_column(const Type &t, const kernel::Value &v, const StandardOptions &o) {
    need(t.family == "gr.data.column@1" && t.arguments.size() == 1 && primitive(t.arguments[0]), "data_type",
         "Expected a supported column type.");
    column_parameters(t);
    const bool mask = nullable(t);
    const auto &set = v.as_buffers();
    need(set.metadata() == Json::object() && set.buffers().size() == (mask ? 2u : 1u), "data_layout",
         "Column buffers or metadata differ from the representation contract.");
    const auto &view = set.at("values");
    need(view.shape().size() == 1 && view.element() == element(t.arguments[0]) &&
             view.elements() <= o.max_elements,
         "data_layout", "Invalid column shape, encoding or element count.");
    if (mask)
        mask_valid(set.at("validity"), view.elements(), false);
    encoded_valid(view, t.arguments[0], false);
}
void validate_scalar(const Type &t, const kernel::Value &v) {
    const auto &e = scalar_element(t);
    need(primitive(e), "data_type", "Unsupported scalar type.");
    const auto &set = v.as_buffers();
    need(set.metadata() == Json::object() && set.buffers().size() == (optional(t) ? 2u : 1u), "data_layout",
         "Invalid scalar buffers or metadata.");
    const auto &view = set.at("values");
    need(view.shape().empty() && view.element() == element(e), "data_layout", "Invalid scalar encoding.");
    if (optional(t))
        mask_valid(set.at("validity"), 1, true);
    encoded_valid(view, e, true);
}
std::vector<std::string> names(const Type &t) {
    return t.parameters.at("names").get<std::vector<std::string>>();
}
kernel::Value extract_column(const kernel::Value &table, const Type &column, std::size_t index) {
    const auto prefix = "c" + std::to_string(index) + ".";
    const auto &set = table.as_buffers();
    return payload(set.at(prefix + "values"),
                   nullable(column) ? std::optional(set.at(prefix + "validity")) : std::nullopt);
}
std::size_t rows(const kernel::Value &table) {
    return count(table.as_buffers().metadata(), "rows", 0);
}
void validate_table(const Type &t, const kernel::Value &v, const StandardOptions &o) {
    need(t.family == "gr.data.table@1" && t.arguments.size() <= o.max_columns, "data_type",
         "Invalid table type or column count.");
    table_parameters(t);
    const auto &set = v.as_buffers();
    keys(set.metadata(), {"rows"});
    need(set.metadata().contains("rows"), "data_layout", "Table row count missing.");
    const auto n = rows(v);
    need(n <= o.max_elements, "data_limit", "Table row count exceeds limit.");
    std::size_t expected = 0;
    for (std::size_t i = 0; i < t.arguments.size(); ++i) {
        const auto col = extract_column(v, t.arguments[i], i);
        validate_column(t.arguments[i], col, o);
        need(values(col).elements() == n, "data_shape", "Table columns have unequal row counts.");
        expected += nullable(t.arguments[i]) ? 2 : 1;
    }
    need(set.buffers().size() == expected, "data_layout", "Unexpected table buffers.");
}
kernel::Value assemble_table(const std::vector<kernel::Value> &columns, const std::vector<Type> &types,
                             std::size_t n) {
    std::vector<memory::NamedBuffer> buffers;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto prefix = "c" + std::to_string(i) + ".";
        buffers.push_back({prefix + "values", values(columns[i])});
        if (nullable(types[i]))
            buffers.push_back({prefix + "validity", columns[i].as_buffers().at("validity")});
    }
    return kernel::Value::buffers(tag, memory::BufferSet(std::move(buffers), {{"rows", n}}));
}
kernel::NumericalContract exact_integer(bool reduction) {
    kernel::NumericalContract c;
    c.storage_format = c.accumulation_format = "gr.int64@1";
    c.integer_overflow = kernel::IntegerOverflow::Error;
    c.reduction_order =
        reduction ? kernel::ReductionOrder::InputOrder : kernel::ReductionOrder::NotApplicable;
    return c;
}
Specialization specialized(std::string name, std::string meaning, execution::ImplementationFunction function,
                           std::optional<kernel::NumericalContract> numerical = {}) {
    Specialization s;
    s.contract.meaning = std::move(meaning);
    s.contract.numerical = std::move(numerical);
    execution::ImplementationDescriptor p;
    p.id = "gr.data.cpu." + name + "@1";
    p.backend_ids = {"cpu.sequential", "cpu.threaded"};
    p.device_kinds = {"cpu"};
    p.memory_domains = {"host"};
    p.thread_safe = true;
    s.providers.push_back({std::move(p), std::move(function)});
    return s;
}
void arity(const std::vector<ValueSpec> &s, std::size_t n) {
    need(s.size() == n, "data_arity", "Incorrect operator argument count.");
}
std::string null_policy(const Json &a, bool filter) {
    const auto p = text(a, "nulls");
    need(filter ? p == "error" || p == "drop" : p == "skip" || p == "propagate", "data_attributes",
         "Unsupported null policy.");
    return p;
}
std::vector<std::size_t> selected(const kernel::Value &mask, const Type &t, std::size_t n,
                                  const std::string &policy, std::stop_token stop) {
    need(values(mask).elements() == n, "data_shape", "Filter mask length differs from input.");
    const memory::ElementRange predicates(values(mask));
    const auto validity = validity_range(mask, nullable(t));
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i % 1024))
            active(stop);
        if (!valid(validity, i)) {
            need(policy == "drop", "data_missing", "Filter mask contains a missing predicate.");
        } else if (predicates.at(i)[0])
            indices.push_back(i);
    }
    return indices;
}
kernel::Value gather(const kernel::Value &v, const Type &t, const std::vector<std::size_t> &indices,
                     const StandardOptions &o, std::stop_token stop) {
    const auto width = element(t.arguments[0]).item_size;
    const memory::ElementRange input(values(v));
    const auto input_validity = validity_range(v, nullable(t));
    auto data = allocate(indices.size(), width, o);
    std::optional<memory::MutableBuffer> mask;
    if (nullable(t))
        mask.emplace(allocate(indices.size(), 1, o));
    {
        auto out = data.borrow();
        std::optional<memory::MutableView> validity;
        if (mask)
            validity.emplace(mask->borrow());
        for (std::size_t i = 0; i < indices.size(); ++i) {
            if (!(i % 1024))
                active(stop);
            const auto bytes = input.at(indices[i]);
            std::copy(bytes.begin(), bytes.end(), out.bytes().begin() + i * width);
            if (validity)
                validity->bytes()[i] = valid(input_validity, indices[i]) ? 1 : 0;
        }
    }
    return payload(finish(std::move(data), t.arguments[0], {indices.size()}),
                   mask ? std::optional(finish(std::move(*mask), boolean_type(), {indices.size()}))
                        : std::nullopt);
}
} // namespace

Type int64_type() {
    return {"gr.data.int64@1", {}, Json::object()};
}
Type boolean_type() {
    return {"gr.data.boolean@1", {}, Json::object()};
}
Type float64_type() {
    return {"gr.data.float64@1", {}, Json::object()};
}
Type optional_type(Type t) {
    return {"gr.data.optional@1", {std::move(t)}, Json::object()};
}
Type column_type(Type t, bool missing) {
    return {"gr.data.column@1", {std::move(t)}, {{"nullable", missing}}};
}
Type matrix_type(Type t) {
    return {"gr.data.matrix@1", {std::move(t)}, Json::object()};
}
Type table_type(const std::vector<std::pair<std::string, Type>> &columns) {
    Type t{"gr.data.table@1", {}, {{"names", Json::array()}}};
    for (const auto &[name, type] : columns) {
        t.arguments.push_back(type);
        t.parameters["names"].push_back(name);
    }
    return t;
}
DataValue scalar_i64(std::int64_t value, StandardOptions o) {
    options_valid(o);
    auto data = allocate(1, 8, o);
    {
        auto lease = data.borrow();
        put(lease.bytes(), std::bit_cast<std::uint64_t>(value));
    }
    return {{int64_type(), scalar_rep}, payload(finish(std::move(data), int64_type(), {})), Json::object()};
}
DataValue optional_i64(std::optional<std::int64_t> value, StandardOptions o) {
    auto result = scalar_i64(value.value_or(0), o);
    auto mask = allocate(1, 1, o);
    {
        auto lease = mask.borrow();
        lease.bytes()[0] = value.has_value();
    }
    result.spec.type = optional_type(int64_type());
    result.payload = payload(values(result.payload), finish(std::move(mask), boolean_type(), {}));
    return result;
}
namespace {
template <class T>
DataValue make_column(std::span<const T> input, Validity validity, const Type &t, StandardOptions o) {
    options_valid(o);
    need(!validity || validity->size() == input.size(), "data_shape", "Validity length differs from column.");
    const auto width = element(t).item_size;
    auto data = allocate(input.size(), width, o);
    std::optional<memory::MutableBuffer> mask;
    if (validity)
        mask.emplace(allocate(input.size(), 1, o));
    {
        auto lease = data.borrow();
        std::optional<memory::MutableView> m;
        if (mask)
            m.emplace(mask->borrow());
        for (std::size_t i = 0; i < input.size(); ++i) {
            std::uint64_t encoded;
            if constexpr (std::is_same_v<T, std::int64_t>)
                encoded = std::bit_cast<std::uint64_t>(input[i]);
            else if constexpr (std::is_same_v<T, double>) {
                need(std::isfinite(input[i]), "data_nonfinite",
                     "Finite binary64 storage rejects NaN and infinity.");
                encoded = std::bit_cast<std::uint64_t>(input[i]);
            } else {
                need(input[i] <= 1, "data_boolean", "Boolean must be 0 or 1.");
                encoded = input[i];
            }
            put(lease.bytes().subspan(i * width, width), encoded);
            if (m) {
                need((*validity)[i] <= 1, "data_validity", "Validity must be 0 or 1.");
                m->bytes()[i] = (*validity)[i];
            }
        }
    }
    return {{column_type(t, validity.has_value()), column_rep},
            payload(finish(std::move(data), t, {input.size()}),
                    mask ? std::optional(finish(std::move(*mask), boolean_type(), {input.size()}))
                         : std::nullopt),
            Json::object()};
}
} // namespace
DataValue column_i64(std::span<const std::int64_t> v, Validity m, StandardOptions o) {
    return make_column(v, m, int64_type(), std::move(o));
}
DataValue column_bool(std::span<const std::uint8_t> v, Validity m, StandardOptions o) {
    return make_column(v, m, boolean_type(), std::move(o));
}
DataValue column_f64(std::span<const double> v, Validity m, StandardOptions o) {
    return make_column(v, m, float64_type(), std::move(o));
}
DataValue make_table(const std::vector<std::pair<std::string, DataValue>> &columns, StandardOptions o) {
    options_valid(o);
    need(columns.size() <= o.max_columns, "data_limit", "Too many table columns.");
    std::set<std::string> unique;
    std::vector<std::pair<std::string, Type>> fields;
    std::vector<kernel::Value> payloads;
    Json provenance = Json::array();
    std::size_t n = 0;
    for (const auto &[name, value] : columns) {
        need(!name.empty() && name.size() <= 256 && unique.insert(name).second, "data_fields",
             "Table names must be bounded, nonempty and unique.");
        need(is_column(value.spec), "data_type", "Tables require column values.");
        validate_column(value.spec.type, value.payload, o);
        const auto size = values(value.payload).elements();
        if (fields.empty())
            n = size;
        need(n == size, "data_shape", "Table columns have unequal row counts.");
        fields.emplace_back(name, value.spec.type);
        payloads.push_back(value.payload);
        provenance.push_back({{"name", name}, {"provenance", value.provenance}});
    }
    auto t = table_type(fields);
    return {{t, table_rep},
            assemble_table(payloads, t.arguments, n),
            {{"constructor", "gr.data.table@1"}, {"columns", provenance}}};
}
DataValue table_column(const DataValue &v, std::string_view name, StandardOptions o) {
    options_valid(o);
    need(is_table(v.spec), "data_type", "Expected table value.");
    validate_table(v.spec.type, v.payload, o);
    const auto fields = names(v.spec.type);
    const auto found = std::find(fields.begin(), fields.end(), name);
    need(found != fields.end(), "data_field", "Table column is absent.");
    const auto index = static_cast<std::size_t>(found - fields.begin());
    return {{v.spec.type.arguments[index], column_rep},
            extract_column(v.payload, v.spec.type.arguments[index], index),
            {{"source", v.provenance}, {"column", name}}};
}
std::int64_t get_scalar_i64(const DataValue &v, StandardOptions o) {
    options_valid(o);
    need(v.spec == ValueSpec{int64_type(), scalar_rep}, "data_type", "Expected int64 scalar.");
    validate_scalar(v.spec.type, v.payload);
    return std::bit_cast<std::int64_t>(bits(values(v.payload).element_bytes({})));
}
std::optional<std::int64_t> get_optional_i64(const DataValue &v, StandardOptions o) {
    options_valid(o);
    need(v.spec == ValueSpec{optional_type(int64_type()), scalar_rep}, "data_type",
         "Expected optional int64 scalar.");
    validate_scalar(v.spec.type, v.payload);
    if (!v.payload.as_buffers().at("validity").element_bytes({})[0])
        return {};
    return std::bit_cast<std::int64_t>(bits(values(v.payload).element_bytes({})));
}
namespace {
template <class T, class F>
std::vector<std::optional<T>> unpack(const DataValue &v, const Type &t, const StandardOptions &o, F decode) {
    options_valid(o);
    need(is_column(v.spec) && v.spec.type.arguments.at(0) == t, "data_type",
         "Unexpected column element type.");
    validate_column(v.spec.type, v.payload, o);
    const memory::ElementRange input(values(v.payload));
    const auto mask = validity_range(v.payload, nullable(v.spec.type));
    std::vector<std::optional<T>> result;
    result.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i)
        result.push_back(valid(mask, i) ? std::optional<T>(decode(input.at(i))) : std::nullopt);
    return result;
}
} // namespace
std::vector<std::optional<std::int64_t>> to_i64(const DataValue &v, StandardOptions o) {
    return unpack<std::int64_t>(v, int64_type(), o,
                                [](auto b) { return std::bit_cast<std::int64_t>(bits(b)); });
}
std::vector<std::optional<bool>> to_bool(const DataValue &v, StandardOptions o) {
    return unpack<bool>(v, boolean_type(), o, [](auto b) { return b[0] != 0; });
}
std::vector<std::optional<double>> to_f64(const DataValue &v, StandardOptions o) {
    return unpack<double>(v, float64_type(), o, [](auto b) { return std::bit_cast<double>(bits(b)); });
}

void install_standard(Library &library, StandardOptions o) {
    options_valid(o);
    for (const auto &[t, meaning] : std::vector<std::pair<Type, std::string>>{
             {int64_type(), "Exact signed 64-bit integer; arithmetic specifies overflow explicitly."},
             {boolean_type(), "Two Boolean values; missingness is separate."},
             {float64_type(),
              "Finite IEEE binary64 value preserving signed zero; no arithmetic supplied."}}) {
        TypeFamily f;
        f.id = t.family;
        f.meaning = meaning;
        f.validate = [](const Type &x) {
            need(x.parameters == Json::object(), "data_type", "Primitive parameters must be empty.");
        };
        library.register_type(std::move(f));
    }
    TypeFamily opt;
    opt.id = "gr.data.optional@1";
    opt.meaning = "An element or an explicit missing value; no uncertainty or probability inference.";
    opt.min_arguments = opt.max_arguments = 1;
    opt.validate = [](const Type &t) {
        need(t.parameters == Json::object(), "data_type", "Optional parameters must be empty.");
    };
    library.register_type(std::move(opt));
    TypeFamily col;
    col.id = "gr.data.column@1";
    col.meaning = "Finite ordered column of one element type, with explicit nullable status; length is a "
                  "value property.";
    col.min_arguments = col.max_arguments = 1;
    col.validate = column_parameters;
    library.register_type(std::move(col));
    TypeFamily tab;
    tab.id = "gr.data.table@1";
    tab.meaning = "Ordered rows in distinctly named typed columns; fields may be heterogeneous and nullable.";
    tab.max_arguments = 64;
    tab.validate = table_parameters;
    library.register_type(std::move(tab));
    Representation scalar;
    scalar.id = scalar_rep;
    scalar.meaning =
        "One rank-zero little-endian encoded scalar; optional scalars carry a separate byte validity flag.";
    scalar.supports = [](const Type &t) {
        return primitive(t) || (optional(t) && t.arguments.size() == 1 && primitive(t.arguments[0]));
    };
    scalar.validate = [](const Type &t, const kernel::Value &v) { validate_scalar(t, v); };
    library.register_representation(std::move(scalar));
    Representation column;
    column.id = column_rep;
    column.meaning = "Rank-one encoded values and, for nullable columns, equally shaped byte-per-slot "
                     "validity; views may be strided.";
    column.metadata = {{"max_elements", o.max_elements},
                       {"validity", "one byte 0/1 per row; not Arrow bitmap"}};
    column.supports = [](const Type &t) {
        return t.family == "gr.data.column@1" && t.arguments.size() == 1 && primitive(t.arguments[0]);
    };
    column.validate = [o](const Type &t, const kernel::Value &v) { validate_column(t, v, o); };
    library.register_representation(std::move(column));
    Representation table;
    table.id = table_rep;
    table.meaning =
        "Named cN.values/cN.validity column views with explicit row count; shares immutable column owners.";
    table.metadata = {{"max_elements", o.max_elements}, {"max_columns", o.max_columns}};
    table.supports = [](const Type &t) {
        return t.family == "gr.data.table@1" &&
               std::all_of(t.arguments.begin(), t.arguments.end(), [](const Type &c) {
                   return c.family == "gr.data.column@1" && c.arguments.size() == 1 &&
                          primitive(c.arguments[0]);
               });
    };
    table.validate = [o](const Type &t, const kernel::Value &v) { validate_table(t, v, o); };
    library.register_representation(std::move(table));

    Operator slice;
    slice.id = "gr.data.slice@1";
    slice.meaning =
        "Select ordered explicit start/count/step column positions, sharing immutable storage and validity.";
    slice.infer = [](const auto &in, const Json &a) {
        arity(in, 1);
        need(is_column(in[0]), "data_type", "Slice needs a column.");
        keys(a, {"start", "count", "step"});
        need(a.contains("start") && a.contains("count"), "data_attributes", "Slice needs start and count.");
        (void)count(a, "start", 0);
        (void)count(a, "count", 0);
        if (a.contains("step")) {
            need(a.at("step").is_number_integer() && a.at("step") != 0 &&
                     !(a.at("step").is_number_unsigned() && a.at("step").get<std::uint64_t>() > INT64_MAX),
                 "data_attributes", "Slice step must be nonzero signed integer.");
            const auto step = a.at("step").get<std::int64_t>();
            need(step >= std::numeric_limits<std::ptrdiff_t>::min() &&
                     step <= std::numeric_limits<std::ptrdiff_t>::max(),
                 "data_attributes", "Slice step exceeds the addressable stride range.");
        }
        return in;
    };
    slice.specialize = [](const auto &in, const auto &, const Json &a) {
        const auto start = count(a, "start", 0), length = count(a, "count", 0);
        const auto step = a.value("step", std::int64_t(1));
        const bool mask = nullable(in[0].type);
        return specialized(
            "slice", "Exact ordered view slice with validity preserved.",
            [=](const kernel::Invocation &v, std::stop_token stop) {
                active(stop);
                return kernel::Invocation{payload(
                    values(v[0]).slice(0, start, length, static_cast<std::ptrdiff_t>(step)),
                    mask ? std::optional(v[0].as_buffers()
                                             .at("validity")
                                             .slice(0, start, length, static_cast<std::ptrdiff_t>(step)))
                         : std::nullopt)};
            });
    };
    library.register_operator(std::move(slice));

    Operator affine;
    affine.id = "gr.data.affine@1";
    affine.meaning = "For each present int64 value, checked multiplication then checked addition; preserve "
                     "missing slots and source order.";
    affine.infer = [](const auto &in, const Json &a) {
        arity(in, 3);
        keys(a, {});
        need(is_column(in[0]) && in[0].type.arguments[0] == int64_type() &&
                 in[1] == ValueSpec{int64_type(), scalar_rep} && in[2] == in[1],
             "data_type", "Affine needs int64 column and two exact int64 scalar operands.");
        return std::vector<ValueSpec>{in[0]};
    };
    affine.specialize = [o](const auto &in, const auto &, const Json &) {
        const bool mask = nullable(in[0].type);
        return specialized(
            "affine", "Checked multiply then checked add for each nonmissing element; no reassociation.",
            [=](const kernel::Invocation &v, std::stop_token stop) {
                active(stop);
                const auto n = values(v[0]).elements();
                const memory::ElementRange input(values(v[0]));
                const auto validity = validity_range(v[0], mask);
                const auto multiplier = std::bit_cast<std::int64_t>(bits(values(v[1]).element_bytes({})));
                const auto addend = std::bit_cast<std::int64_t>(bits(values(v[2]).element_bytes({})));
                auto result = allocate(n, 8, o);
                {
                    auto lease = result.borrow();
                    for (std::size_t i = 0; i < n; ++i) {
                        if (!(i % 1024))
                            active(stop);
                        if (valid(validity, i))
                            put(lease.bytes().subspan(i * 8, 8),
                                std::bit_cast<std::uint64_t>(kernel::checked_add(
                                    kernel::checked_multiply(std::bit_cast<std::int64_t>(bits(input.at(i))),
                                                             multiplier),
                                    addend)));
                    }
                }
                return kernel::Invocation{
                    payload(finish(std::move(result), int64_type(), {n}),
                            mask ? std::optional(v[0].as_buffers().at("validity")) : std::nullopt)};
            },
            exact_integer(false));
    };
    library.register_operator(std::move(affine));

    Operator compare;
    compare.id = "gr.data.compare@1";
    compare.meaning = "Compare each present int64 column value with one exact scalar; missing inputs yield "
                      "missing predicates.";
    compare.infer = [](const auto &in, const Json &a) {
        arity(in, 2);
        keys(a, {"relation"});
        const auto r = text(a, "relation");
        need(std::set<std::string>{"lt", "le", "eq", "ne", "ge", "gt"}.contains(r), "data_attributes",
             "Unknown comparison relation.");
        need(is_column(in[0]) && in[0].type.arguments[0] == int64_type() &&
                 in[1] == ValueSpec{int64_type(), scalar_rep},
             "data_type", "Compare needs int64 column and scalar.");
        return std::vector<ValueSpec>{{column_type(boolean_type(), nullable(in[0].type)), column_rep}};
    };
    compare.specialize = [o](const auto &in, const auto &, const Json &a) {
        const auto relation = text(a, "relation");
        const bool mask = nullable(in[0].type);
        return specialized("compare", "Exact signed integer comparison with explicit missing predicates.",
                           [=](const kernel::Invocation &v, std::stop_token stop) {
                               active(stop);
                               const auto n = values(v[0]).elements();
                               const memory::ElementRange input(values(v[0]));
                               const auto validity = validity_range(v[0], mask);
                               const auto threshold =
                                   std::bit_cast<std::int64_t>(bits(values(v[1]).element_bytes({})));
                               auto result = allocate(n, 1, o);
                               {
                                   auto lease = result.borrow();
                                   for (std::size_t i = 0; i < n; ++i) {
                                       if (!(i % 1024))
                                           active(stop);
                                       if (!valid(validity, i))
                                           continue;
                                       const auto x = std::bit_cast<std::int64_t>(bits(input.at(i)));
                                       lease.bytes()[i] = relation == "lt"   ? x < threshold
                                                          : relation == "le" ? x <= threshold
                                                          : relation == "eq" ? x == threshold
                                                          : relation == "ne" ? x != threshold
                                                          : relation == "ge" ? x >= threshold
                                                                             : x > threshold;
                                   }
                               }
                               return kernel::Invocation{payload(
                                   finish(std::move(result), boolean_type(), {n}),
                                   mask ? std::optional(v[0].as_buffers().at("validity")) : std::nullopt)};
                           });
    };
    library.register_operator(std::move(compare));

    for (const bool tables : {false, true}) {
        Operator filter;
        filter.id = tables ? "gr.data.table-filter@1" : "gr.data.filter@1";
        filter.meaning = "Stable row selection by a Boolean column; missing predicate policy must be "
                         "explicitly error or drop.";
        filter.qualifications =
            Json::array({"Unselected rows are omitted; missing predicates are never silently accepted."});
        filter.infer = [tables](const auto &in, const Json &a) {
            arity(in, 2);
            keys(a, {"nulls"});
            (void)null_policy(a, true);
            need((tables ? is_table(in[0]) : is_column(in[0])) && is_column(in[1]) &&
                     in[1].type.arguments[0] == boolean_type(),
                 "data_type", "Filter needs a matching Boolean column predicate.");
            return std::vector<ValueSpec>{in[0]};
        };
        filter.specialize = [o, tables](const auto &in, const auto &, const Json &a) {
            const auto policy = null_policy(a, true);
            const auto source = in[0].type, mask_type = in[1].type;
            return specialized(
                tables ? "table-filter" : "filter",
                "Stable selection under explicit predicate missingness policy.",
                [=](const kernel::Invocation &v, std::stop_token stop) {
                    active(stop);
                    const auto indices = selected(
                        v[1], mask_type, tables ? rows(v[0]) : values(v[0]).elements(), policy, stop);
                    if (!tables)
                        return kernel::Invocation{gather(v[0], source, indices, o, stop)};
                    std::vector<kernel::Value> columns;
                    for (std::size_t i = 0; i < source.arguments.size(); ++i) {
                        active(stop);
                        columns.push_back(gather(extract_column(v[0], source.arguments[i], i),
                                                 source.arguments[i], indices, o, stop));
                    }
                    return kernel::Invocation{assemble_table(columns, source.arguments, indices.size())};
                });
        };
        library.register_operator(std::move(filter));
    }

    Operator sum;
    sum.id = "gr.data.sum@1";
    sum.meaning = "Checked left-to-right int64 sum, with explicit skip/propagate missingness and minimum "
                  "present count; returns optional int64.";
    sum.infer = [](const auto &in, const Json &a) {
        arity(in, 1);
        keys(a, {"nulls", "min_count"});
        (void)null_policy(a, false);
        (void)count(a, "min_count", 1);
        need(is_column(in[0]) && in[0].type.arguments[0] == int64_type(), "data_type",
             "Sum requires int64 column.");
        return std::vector<ValueSpec>{{optional_type(int64_type()), scalar_rep}};
    };
    sum.specialize = [o](const auto &in, const auto &, const Json &a) {
        const auto policy = null_policy(a, false);
        const auto minimum = count(a, "min_count", 1);
        const bool mask = nullable(in[0].type);
        return specialized(
            "sum",
            "If propagation sees any missing value or present count is below min_count, return missing "
            "before arithmetic. Otherwise add present elements in order with checked int64 overflow.",
            [=](const kernel::Invocation &v, std::stop_token stop) {
                active(stop);
                const auto n = values(v[0]).elements();
                const memory::ElementRange input(values(v[0]));
                const auto validity = validity_range(v[0], mask);
                std::size_t present = 0;
                bool missing = false;
                for (std::size_t i = 0; i < n; ++i) {
                    if (!(i % 1024))
                        active(stop);
                    if (valid(validity, i))
                        ++present;
                    else
                        missing = true;
                }
                if (present < minimum || (missing && policy == "propagate"))
                    return kernel::Invocation{optional_i64({}, o).payload};
                std::int64_t total = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    if (!(i % 1024))
                        active(stop);
                    if (valid(validity, i))
                        total = kernel::checked_add(total, std::bit_cast<std::int64_t>(bits(input.at(i))));
                }
                return kernel::Invocation{optional_i64(total, o).payload};
            },
            exact_integer(true));
    };
    library.register_operator(std::move(sum));

    for (const bool project : {false, true}) {
        Operator op;
        op.id = project ? "gr.data.table-project@1" : "gr.data.table-column@1";
        op.meaning =
            project ? "Select and order distinct named table columns, preserving row count and sharing "
                      "immutable buffers."
                    : "Select one named column from a table, retaining its nullable type and shared owners.";
        op.infer = [project](const auto &in, const Json &a) {
            arity(in, 1);
            need(is_table(in[0]), "data_type", "Expected table.");
            keys(a, project ? std::initializer_list<const char *>{"names"}
                            : std::initializer_list<const char *>{"name"});
            std::vector<std::string> requested;
            if (project) {
                need(a.contains("names") && a.at("names").is_array(), "data_attributes",
                     "Projection needs names.");
                for (const auto &name : a.at("names")) {
                    need(name.is_string(), "data_attributes", "Column names must be strings.");
                    requested.push_back(name.get<std::string>());
                }
            } else
                requested.push_back(text(a, "name"));
            const auto original = names(in[0].type);
            std::set<std::string> unique;
            std::vector<std::pair<std::string, Type>> fields;
            for (const auto &name : requested) {
                const auto found = std::find(original.begin(), original.end(), name);
                need(found != original.end() && unique.insert(name).second, "data_field",
                     "Unknown or repeated projected field.");
                fields.emplace_back(name, in[0].type.arguments[found - original.begin()]);
            }
            return std::vector<ValueSpec>{
                {project ? table_type(fields) : fields.at(0).second, project ? table_rep : column_rep}};
        };
        op.specialize = [project](const auto &in, const auto &out, const Json &a) {
            const auto original = names(in[0].type);
            const auto requested = project ? names(out[0].type) : std::vector<std::string>{text(a, "name")};
            std::vector<std::size_t> indices;
            for (const auto &name : requested)
                indices.push_back(std::find(original.begin(), original.end(), name) - original.begin());
            const auto source = in[0].type, target = out[0].type;
            return specialized(project ? "table-project" : "table-column",
                               "Exact column selection with retained immutable ownership.",
                               [=](const kernel::Invocation &v, std::stop_token stop) {
                                   active(stop);
                                   std::vector<kernel::Value> columns;
                                   for (auto i : indices)
                                       columns.push_back(extract_column(v[0], source.arguments[i], i));
                                   return kernel::Invocation{
                                       project ? assemble_table(columns, target.arguments, rows(v[0]))
                                               : columns.at(0)};
                               });
        };
        library.register_operator(std::move(op));
    }
}
} // namespace gr::data
