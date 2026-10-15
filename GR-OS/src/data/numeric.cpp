#include "gr/data/numeric.hpp"
#include "gr/memory/access.hpp"
#include <array>
#include <bit>
#include <cfenv>
#include <cfloat>
#include <limits>
#include <set>
#include <type_traits>
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#define GR_NUMERIC_X64 1
#endif

namespace gr::data::numeric {
namespace {
constexpr const char *tag = "gr.numeric.payload@1";
constexpr const char *column_rep = "gr.numeric.column-le@1";
constexpr const char *scalar_rep = "gr.numeric.scalar-le@1";
constexpr const char *standard_column = "gr.data.column-buffers@1";
void need(bool ok, const char *code, const char *message) {
    if (!ok)
        throw Error(code, message);
}
void active(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Numeric operation cancelled.");
}
void options_valid(const StandardOptions &o) {
    need(o.max_elements > 0 && o.max_elements <= 16 * 1024 * 1024 && bool(o.allocation_budget),
         "data_options", "Numeric values need bounded elements and an allocation budget.");
}
void keys(const Json &j, std::initializer_list<const char *> allowed) {
    need(j.is_object(), "data_attributes", "Attributes must be an object.");
    const std::set<std::string> names(allowed.begin(), allowed.end());
    for (auto it = j.begin(); it != j.end(); ++it)
        need(names.contains(it.key()), "data_attributes", "Unknown numeric attribute.");
}
constexpr bool qualified_float() {
#if defined(_M_X64) && defined(_MSC_VER) && !defined(__clang__) && defined(GR_NUMERIC_STRICT_FP) &&          \
    GR_NUMERIC_STRICT_FP && !defined(__FAST_MATH__)
    return sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 && FLT_EVAL_METHOD == 0;
#else
    return false;
#endif
}
class FloatScope {
  public:
    explicit FloatScope(bool enabled) {
        if (!enabled)
            return;
        need(qualified_float(), "unsupported_numeric_environment",
             "Floating execution requires the qualified MSVC strict binary64 x64/SSE2 build profile.");
        need(std::fegetenv(&saved_) == 0, "unsupported_numeric_environment",
             "Cannot save floating environment.");
#ifdef GR_NUMERIC_X64
        mxcsr_ = _mm_getcsr();
#endif
        saved_ok_ = true;
        try {
            need(std::fesetenv(FE_DFL_ENV) == 0 && std::fesetround(FE_TONEAREST) == 0,
                 "unsupported_numeric_environment", "Cannot select round-nearest environment.");
#ifdef GR_NUMERIC_X64
            // Mask exceptions and preserve gradual underflow: neither DAZ nor FTZ.
            _mm_setcsr((_mm_getcsr() & ~unsigned((1u << 15) | (1u << 6))) | 0x1f80u);
            const auto csr = _mm_getcsr();
            need((csr & ((1u << 15) | (1u << 6) | 0x6000u)) == 0 && (csr & 0x1f80u) == 0x1f80u,
                 "unsupported_numeric_environment", "Host did not accept strict binary64 controls.");
#endif
            need(std::fegetround() == FE_TONEAREST, "unsupported_numeric_environment",
                 "Wrong rounding mode.");
        } catch (...) {
            restore();
            throw;
        }
    }
    ~FloatScope() { restore(); }
    FloatScope(const FloatScope &) = delete;

  private:
    void restore() noexcept {
        if (!saved_ok_)
            return;
        (void)std::fesetenv(&saved_);
#ifdef GR_NUMERIC_X64
        _mm_setcsr(mxcsr_);
#endif
        saved_ok_ = false;
    }
    std::fenv_t saved_{};
    unsigned mxcsr_ = 0;
    bool saved_ok_ = false;
};
bool finite(double x) {
    return (std::bit_cast<std::uint64_t>(x) & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
}
template <class T> Type type();
template <> Type type<std::int64_t>() {
    return int64_type();
}
template <> Type type<std::uint64_t>() {
    return uint64_type();
}
template <> Type type<double>() {
    return float64_type();
}
template <> Type type<Complex128>() {
    return complex128_type();
}
bool supported(const Type &t) {
    return t == int64_type() || t == uint64_type() || t == float64_type() || t == complex128_type();
}
memory::ElementDescriptor encoding(const Type &t) {
    if (t == int64_type())
        return {"gr.int64@1", 8, 8, memory::ByteOrder::Little};
    if (t == uint64_type())
        return {"gr.uint64@1", 8, 8, memory::ByteOrder::Little};
    if (t == float64_type())
        return {"gr.float64-ieee754@1", 8, 8, memory::ByteOrder::Little};
    if (t == complex128_type())
        return {"gr.complex128-pair@1", 16, 8, memory::ByteOrder::Little};
    throw Error("data_type", "Unsupported numeric element.");
}
memory::ElementDescriptor mask_encoding() {
    return {"gr.boolean-u8@1", 1, 1, memory::ByteOrder::NotApplicable};
}
template <class T> T decode(std::span<const std::uint8_t> bytes) {
    if constexpr (std::is_same_v<T, Complex128>)
        return {decode<double>(bytes.first(8)), decode<double>(bytes.subspan(8, 8))};
    else if constexpr (std::is_same_v<T, std::uint64_t>)
        return memory::load_little<std::uint64_t>(bytes);
    else
        return std::bit_cast<T>(memory::load_little<std::uint64_t>(bytes));
}
template <class T> void encode(std::span<std::uint8_t> out, T value) {
    if constexpr (std::is_same_v<T, Complex128>) {
        encode(out.first(8), value.real);
        encode(out.subspan(8, 8), value.imag);
    } else if constexpr (std::is_same_v<T, std::uint64_t>)
        memory::store_little(out, value);
    else
        memory::store_little(out, std::bit_cast<std::uint64_t>(value));
}
template <class T> void finite_input(T value) {
    if constexpr (std::is_same_v<T, double>)
        need(finite(value), "numeric_nonfinite",
             "Finite binary64 operands required, including missing slots.");
    else if constexpr (std::is_same_v<T, Complex128>)
        need(finite(value.real) && finite(value.imag), "numeric_nonfinite",
             "Finite complex components required.");
}
bool nullable(const Type &t) {
    return t.parameters.at("nullable").get<bool>();
}
bool column_spec(const ValueSpec &s) {
    return s.type.family == "gr.data.column@1" && s.type.arguments.size() == 1 &&
           supported(s.type.arguments[0]) &&
           (s.representation == column_rep ||
            (s.representation == standard_column &&
             (s.type.arguments[0] == int64_type() || s.type.arguments[0] == float64_type())));
}
ValueSpec column_output(const Type &element, bool missing) {
    return {column_type(element, missing),
            (element == int64_type() || element == float64_type()) ? standard_column : column_rep};
}
void validate_layout(const Type &t, const kernel::Value &payload, const StandardOptions &o) {
    const bool column = t.family == "gr.data.column@1";
    const bool optional = t.family == "gr.data.optional@1";
    need((!column && !optional) || t.arguments.size() == 1, "data_type", "Numeric arity differs.");
    const auto &element = (column || optional) ? t.arguments[0] : t;
    need(supported(element), "data_type", "Unsupported numeric logical element.");
    bool missing = optional;
    if (column) {
        keys(t.parameters, {"nullable"});
        need(t.parameters.contains("nullable") && t.parameters.at("nullable").is_boolean(), "data_type",
             "Column nullable status must be explicit.");
        missing = nullable(t);
    } else if (optional)
        need(t.parameters == Json::object(), "data_type", "Optional parameters must be empty.");
    const auto &set = payload.as_buffers();
    need(set.metadata() == Json::object() && set.buffers().size() == (missing ? 2u : 1u), "data_layout",
         "Numeric buffer set differs from its representation.");
    const auto &values = set.at("values");
    need(values.element() == encoding(element) && values.shape().size() == (column ? 1u : 0u) &&
             values.elements() <= o.max_elements,
         "data_layout", "Numeric shape or encoding differs.");
    if (missing) {
        const auto &mask = set.at("validity");
        need(mask.shape() == values.shape() && mask.element() == mask_encoding(), "data_layout",
             "Invalid validity layout.");
        if (column)
            memory::ElementRange(mask).for_each([](std::size_t, auto bytes) {
                need(bytes[0] <= 1, "data_validity", "Validity must be 0 or 1.");
            });
        else
            need(mask.element_bytes({})[0] <= 1, "data_validity", "Validity must be 0 or 1.");
    }
    const auto check = [&](std::span<const std::uint8_t> bytes) {
        if (element == float64_type())
            finite_input(decode<double>(bytes));
        else if (element == complex128_type())
            finite_input(decode<Complex128>(bytes));
    };
    // Integer layouts admit every bit pattern; avoid constructing logical types per element.
    if (element == float64_type()) {
        if (column)
            memory::ElementRange(values).for_each(
                [](std::size_t, auto b) { finite_input(decode<double>(b)); });
        else
            check(values.element_bytes({}));
    } else if (element == complex128_type()) {
        if (column)
            memory::ElementRange(values).for_each(
                [](std::size_t, auto b) { finite_input(decode<Complex128>(b)); });
        else
            check(values.element_bytes({}));
    }
}
template <class T>
DataValue construct(std::span<const T> input, Validity validity, bool scalar, bool optional,
                    const StandardOptions &o) {
    options_valid(o);
    need(input.size() <= o.max_elements && (!scalar || input.size() == 1), "data_limit",
         "Numeric value size differs.");
    need(!validity || validity->size() == input.size(), "data_shape", "Validity length differs.");
    const auto e = encoding(type<T>());
    auto owner = memory::MutableBuffer::allocate(input.size() * e.item_size, 64, o.allocation_budget);
    {
        auto lease = owner.borrow();
        for (std::size_t i = 0; i < input.size(); ++i) {
            finite_input(input[i]);
            encode(lease.bytes().subspan(i * e.item_size, e.item_size), input[i]);
        }
    }
    const auto shape = scalar ? std::vector<std::size_t>{} : std::vector<std::size_t>{input.size()};
    std::vector<memory::NamedBuffer> buffers{
        {"values", memory::BufferView(std::move(owner).freeze(), e, shape)}};
    if (validity) {
        for (auto byte : *validity)
            need(byte <= 1, "data_validity", "Validity must be 0 or 1.");
        buffers.push_back(
            {"validity", memory::BufferView(memory::Buffer::copy(*validity, 64, o.allocation_budget),
                                            mask_encoding(), shape)});
    }
    const auto spec = scalar ? ValueSpec{optional ? optional_type(type<T>()) : type<T>(), scalar_rep}
                             : column_output(type<T>(), validity.has_value());
    return {spec, kernel::Value::buffers(tag, memory::BufferSet(std::move(buffers)))};
}
template <class T> DataValue scalar_value(T value, const StandardOptions &o) {
    const std::array<T, 1> items{value};
    return construct<T>(items, {}, true, false, o);
}
template <class T> kernel::Value optional_value(std::optional<T> value, const StandardOptions &o) {
    const std::array<T, 1> items{value.value_or(T{})};
    const std::array<std::uint8_t, 1> mask{static_cast<std::uint8_t>(value.has_value())};
    return construct<T>(items, std::span<const std::uint8_t>(mask), true, true, o).payload;
}
template <class T> std::vector<std::optional<T>> unpack(const DataValue &v, const StandardOptions &o) {
    options_valid(o);
    need(column_spec(v.spec) && v.spec.type.arguments[0] == type<T>(), "data_type",
         "Unexpected numeric column.");
    validate_layout(v.spec.type, v.payload, o);
    const memory::ElementRange values(v.payload.as_buffers().at("values"));
    std::optional<memory::ElementRange> mask;
    if (nullable(v.spec.type))
        mask.emplace(v.payload.as_buffers().at("validity"));
    std::vector<std::optional<T>> out;
    out.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
        out.push_back(!mask || mask->at(i)[0] ? std::optional<T>(decode<T>(values.at(i))) : std::nullopt);
    return out;
}
template <class T> std::optional<T> unpack_scalar(const DataValue &v, const StandardOptions &o) {
    options_valid(o);
    need(v.spec.representation == scalar_rep &&
             (v.spec.type == type<T>() || v.spec.type == optional_type(type<T>())),
         "data_type", "Unexpected numeric scalar.");
    validate_layout(v.spec.type, v.payload, o);
    if (v.spec.type.family == "gr.data.optional@1" &&
        !v.payload.as_buffers().at("validity").element_bytes({})[0])
        return {};
    return decode<T>(v.payload.as_buffers().at("values").element_bytes({}));
}
double rounded_add(double a, double b) {
    const double value = a + b;
    need(finite(value), "numeric_overflow", "Finite binary64 addition overflow.");
    return value;
}
double rounded_subtract(double a, double b) {
    const double value = a - b;
    need(finite(value), "numeric_overflow", "Finite binary64 subtraction overflow.");
    return value;
}
double rounded_multiply(double a, double b) {
    const double value = a * b;
    need(finite(value), "numeric_overflow", "Finite binary64 multiplication overflow.");
    return value;
}
template <class T> T add(T a, T b) {
    if constexpr (std::is_same_v<T, std::uint64_t>) {
        need(a <= UINT64_MAX - b, "numeric_overflow", "Unsigned uint64 addition overflow.");
        return a + b;
    } else if constexpr (std::is_same_v<T, std::int64_t>)
        return kernel::checked_add(a, b);
    else if constexpr (std::is_same_v<T, double>)
        return rounded_add(a, b);
    else
        return {rounded_add(a.real, b.real), rounded_add(a.imag, b.imag)};
}
template <class T> T multiply(T a, T b) {
    if constexpr (std::is_same_v<T, std::uint64_t>) {
        need(!a || b <= UINT64_MAX / a, "numeric_overflow", "Unsigned uint64 multiplication overflow.");
        return a * b;
    } else if constexpr (std::is_same_v<T, std::int64_t>)
        return kernel::checked_multiply(a, b);
    else if constexpr (std::is_same_v<T, double>)
        return rounded_multiply(a, b);
    else {
        const double ac = rounded_multiply(a.real, b.real), bd = rounded_multiply(a.imag, b.imag);
        const double real = rounded_subtract(ac, bd);
        const double ad = rounded_multiply(a.real, b.imag), bc = rounded_multiply(a.imag, b.real);
        return {real, rounded_add(ad, bc)};
    }
}
struct Reader {
    const memory::BufferView &view;
    memory::ElementRange range;
    bool scalar;
    Reader(const kernel::Value &v, bool slow)
        : view(v.as_buffers().at("values")), range(view), scalar(slow) {}
    std::span<const std::uint8_t> at(std::size_t i) const {
        if (!scalar)
            return range.at(i);
        const std::array<std::size_t, 1> index{i};
        return view.element_bytes(index);
    }
};
std::optional<memory::ElementRange> mask(const kernel::Value &v, bool nullable) {
    if (nullable)
        return memory::ElementRange(v.as_buffers().at("validity"));
    return {};
}
template <class T>
kernel::Invocation binary(const kernel::Invocation &in, std::stop_token stop, const std::string &op,
                          bool left_null, bool right_null, bool scalar, const StandardOptions &o) {
    active(stop);
    FloatScope environment(std::is_same_v<T, double> || std::is_same_v<T, Complex128>);
    const Reader left(in[0], scalar), right(in[1], scalar);
    const auto lm = mask(in[0], left_null), rm = mask(in[1], right_null);
    const auto n = left.range.size();
    need(n == right.range.size(), "data_shape", "Numeric column lengths differ.");
    const bool equality = op == "equal";
    const auto e = equality ? mask_encoding() : encoding(type<T>());
    auto output = memory::MutableBuffer::allocate(n * e.item_size, 64, o.allocation_budget);
    std::optional<memory::MutableBuffer> validity;
    if (left_null || right_null)
        validity.emplace(memory::MutableBuffer::allocate(n, 64, o.allocation_budget));
    {
        auto out = output.borrow();
        std::optional<memory::MutableView> valid;
        if (validity)
            valid.emplace(validity->borrow());
        for (std::size_t i = 0; i < n; ++i) {
            if (!(i % 1024))
                active(stop);
            const bool present = (!lm || lm->at(i)[0]) && (!rm || rm->at(i)[0]);
            if (valid)
                valid->bytes()[i] = static_cast<std::uint8_t>(present);
            if (!present)
                continue;
            const auto a = decode<T>(left.at(i)), b = decode<T>(right.at(i));
            if (equality)
                out.bytes()[i] = static_cast<std::uint8_t>(a == b);
            else
                encode(out.bytes().subspan(i * e.item_size, e.item_size),
                       op == "add" ? add(a, b) : multiply(a, b));
        }
    }
    active(stop);
    std::vector<memory::NamedBuffer> buffers{
        {"values", memory::BufferView(std::move(output).freeze(), e, {n})}};
    if (validity)
        buffers.push_back(
            {"validity", memory::BufferView(std::move(*validity).freeze(), mask_encoding(), {n})});
    return {kernel::Value::buffers(tag, memory::BufferSet(std::move(buffers)))};
}
template <class T>
kernel::Invocation sum(const kernel::Invocation &in, std::stop_token stop, bool nullable, bool scalar,
                       bool propagate, std::size_t minimum, const StandardOptions &o) {
    active(stop);
    FloatScope environment(std::is_same_v<T, double> || std::is_same_v<T, Complex128>);
    const Reader input(in[0], scalar);
    const auto valid = mask(in[0], nullable);
    std::size_t count = 0;
    for (std::size_t i = 0; i < input.range.size(); ++i) {
        if (!(i % 1024))
            active(stop);
        if (!valid || valid->at(i)[0])
            ++count;
    }
    if (count < minimum || (propagate && count != input.range.size()))
        return {optional_value<T>({}, o)};
    T total{}; // Explicit +0 for floating/complex empty reductions.
    for (std::size_t i = 0; i < input.range.size(); ++i) {
        if (!(i % 1024))
            active(stop);
        if (!valid || valid->at(i)[0])
            total = add(total, decode<T>(input.at(i)));
    }
    active(stop);
    return {optional_value<T>(total, o)};
}
Json floating_profile() {
    return {
        {"id", "gr.binary64-ordered-finite@1"},
        {"domain", "finite IEEE754 binary64 components"},
        {"rounding", "nearest_even"},
        {"underflow", "gradual; rounded signed zero permitted"},
        {"contraction", "forbidden"},
        {"overflow", "reject each nonfinite intermediate"},
        {"complex_multiply_order", "ac,bd,ac-bd,ad,bc,ad+bc; each separately rounded"},
        {"equality", "numeric component equality; positive and negative zero compare equal"},
        {"exact_scope", "defined machine result on qualified strict x64/SSE2 environment; not exact reals"}};
}
template <class T>
Specialization specialization(const std::string &op, const std::vector<ValueSpec> &in, const Json &attrs,
                              const StandardOptions &o) {
    const bool reduction = op == "sum";
    const bool left_null = nullable(in[0].type);
    const bool right_null = !reduction && nullable(in[1].type);
    const auto minimum =
        reduction && attrs.contains("min_count") ? attrs.at("min_count").get<std::size_t>() : 1;
    const bool propagate = reduction && attrs.at("nulls") == "propagate";
    Specialization s;
    s.contract.meaning =
        "Same-element numeric " + op + "; explicit missingness; checked ordered machine arithmetic.";
    s.contract.metadata["missing_slots"] = "zero component bytes with validity 0";
    s.contract.metadata["max_input_elements"] = o.max_elements;
    if (reduction) {
        s.contract.metadata["missingness_order"] =
            "Count present values first; insufficient count or propagate with any missing returns missing "
            "before arithmetic.";
        s.contract.metadata["reduction_initial"] = "zero; positive zero for each floating component";
    }
    kernel::NumericalContract numeric;
    numeric.storage_format = numeric.accumulation_format = encoding(type<T>()).format_id;
    numeric.reduction_order =
        reduction ? kernel::ReductionOrder::InputOrder : kernel::ReductionOrder::NotApplicable;
    if constexpr (std::is_integral_v<T>)
        numeric.integer_overflow = kernel::IntegerOverflow::Error;
    else {
        numeric.rounding = kernel::Rounding::NearestEven;
        numeric.nonfinite = kernel::NonFinite::Reject;
        s.contract.metadata["floating_profile"] = floating_profile();
    }
    s.contract.numerical = numeric;
    s.validate_inputs = [reduction, maximum = o.max_elements](const kernel::Invocation &values) {
        for (const auto &value : values)
            need(value.as_buffers().at("values").elements() <= maximum, "data_limit",
                 "Numeric input exceeds this module's element limit.");
        if (!reduction)
            need(values[0].as_buffers().at("values").elements() ==
                     values[1].as_buffers().at("values").elements(),
                 "data_shape", "Numeric column lengths differ.");
    };
    for (bool scalar : {false, true}) {
        execution::ImplementationDescriptor p;
        p.id = "gr.numeric.cpu." + op + (scalar ? "-scalar@1" : "-batch@1");
        p.backend_ids = {"cpu.sequential", "cpu.threaded"};
        p.device_kinds = {"cpu"};
        p.memory_domains = {"host"};
        p.thread_safe = true;
        s.providers.push_back({std::move(p), [=](const kernel::Invocation &values, std::stop_token stop) {
                                   return reduction
                                              ? sum<T>(values, stop, left_null, scalar, propagate, minimum, o)
                                              : binary<T>(values, stop, op, left_null, right_null, scalar, o);
                               }});
    }
    return s;
}
} // namespace
Type uint64_type() {
    return {"gr.numeric.uint64@1", {}, Json::object()};
}
Type complex128_type() {
    return {"gr.numeric.complex@1", {float64_type()}, Json::object()};
}
Json floating_capabilities() {
    return {{"supported", qualified_float()},
            {"profile", floating_profile()},
            {"platform", "MSVC x64/SSE2, IEEE binary64, FLT_EVAL_METHOD=0, explicitly strict configuration"},
            {"other_compilers", "Floating execution disabled pending explicit platform conformance; storage "
                                "and checked integer operations remain available"},
            {"caller_environment", "saved and restored including exception flags, rounding, FTZ/DAZ"}};
}
DataValue scalar_u64(std::uint64_t v, StandardOptions o) {
    return scalar_value(v, o);
}
DataValue scalar_f64(double v, StandardOptions o) {
    return scalar_value(v, o);
}
DataValue scalar_c128(Complex128 v, StandardOptions o) {
    return scalar_value(v, o);
}
DataValue column_u64(std::span<const std::uint64_t> v, Validity mask, StandardOptions o) {
    return construct(v, mask, false, false, o);
}
DataValue column_f64(std::span<const double> v, Validity mask, StandardOptions o) {
    return construct(v, mask, false, false, o);
}
DataValue column_c128(std::span<const Complex128> v, Validity mask, StandardOptions o) {
    return construct(v, mask, false, false, o);
}
std::vector<std::optional<std::uint64_t>> to_u64(const DataValue &v, StandardOptions o) {
    return unpack<std::uint64_t>(v, o);
}
std::vector<std::optional<double>> to_f64(const DataValue &v, StandardOptions o) {
    return unpack<double>(v, o);
}
std::vector<std::optional<Complex128>> to_c128(const DataValue &v, StandardOptions o) {
    return unpack<Complex128>(v, o);
}
std::optional<std::uint64_t> get_optional_u64(const DataValue &v, StandardOptions o) {
    return unpack_scalar<std::uint64_t>(v, o);
}
std::optional<double> get_optional_f64(const DataValue &v, StandardOptions o) {
    return unpack_scalar<double>(v, o);
}
std::optional<Complex128> get_optional_c128(const DataValue &v, StandardOptions o) {
    return unpack_scalar<Complex128>(v, o);
}
void install(Library &library, StandardOptions o) {
    options_valid(o);
    // Install after the standard module, before sealing. Its families and
    // representations are resolved by the common library at validation time.
    TypeFamily u;
    u.id = uint64_type().family;
    u.meaning = "Exact unsigned integer in 0..2^64-1; overflow is operation-specific.";
    u.validate = [](const Type &t) {
        need(t.parameters == Json::object(), "data_type", "Uint64 has no parameters.");
    };
    library.register_type(std::move(u));
    TypeFamily c;
    c.id = complex128_type().family;
    c.meaning = "Ordered real/imaginary pair over the declared scalar domain.";
    c.min_arguments = c.max_arguments = 1;
    c.validate = [](const Type &t) {
        need(t.parameters == Json::object(), "data_type", "Complex parameters must be empty.");
    };
    library.register_type(std::move(c));
    for (const bool column : {false, true}) {
        Representation r;
        r.id = column ? column_rep : scalar_rep;
        r.carrier = kernel::Representation::Buffers;
        r.meaning = "Little-endian numeric components with exact encoding and explicit byte validity; finite "
                    "floating components.";
        r.metadata = {{"max_elements", o.max_elements},
                      {"complex_layout", "interleaved real then imaginary binary64"},
                      {"validity", "one byte 0/1 per logical element"}};
        r.supports = [column](const Type &t) {
            if (column)
                return t.family == "gr.data.column@1" && t.arguments.size() == 1 && supported(t.arguments[0]);
            return supported(t) ||
                   (t.family == "gr.data.optional@1" && t.arguments.size() == 1 && supported(t.arguments[0]));
        };
        r.validate = [o](const Type &t, const kernel::Value &v) { validate_layout(t, v, o); };
        library.register_representation(std::move(r));
    }
    for (const auto &name :
         {std::string("add"), std::string("multiply"), std::string("equal"), std::string("sum")}) {
        Operator op;
        op.id = "gr.numeric." + name + "@1";
        op.meaning = "Explicit same-element numeric " + name + "; no implicit promotion or reassociation.";
        op.metadata = {{"module", "gr.data.numeric@1"}, {"floating_profile", floating_profile()}};
        op.infer = [name](const std::vector<ValueSpec> &in, const Json &attrs) {
            const bool reduction = name == "sum";
            need(in.size() == (reduction ? 1u : 2u), "data_arity",
                 "Numeric operation argument count differs.");
            for (const auto &v : in)
                need(column_spec(v), "data_type", "Numeric operation needs supported columns.");
            if (reduction) {
                keys(attrs, {"nulls", "min_count"});
                need(attrs.contains("nulls") &&
                         (attrs.at("nulls") == "skip" || attrs.at("nulls") == "propagate"),
                     "data_attributes", "Numeric sum needs nulls=skip or propagate.");
                if (attrs.contains("min_count")) {
                    const auto &m = attrs.at("min_count");
                    need(m.is_number_integer() && m >= 0 && m.get<std::uint64_t>() <= 16 * 1024 * 1024,
                         "data_attributes", "min_count exceeds the bounded numeric domain.");
                }
                const auto &element = in[0].type.arguments[0];
                return std::vector<ValueSpec>{
                    {optional_type(element), element == int64_type() ? "gr.data.scalar-le@1" : scalar_rep}};
            }
            keys(attrs, {});
            need(in[0].type.arguments[0] == in[1].type.arguments[0], "data_type",
                 "Mixed numeric domains require explicit conversion.");
            const bool missing = nullable(in[0].type) || nullable(in[1].type);
            return std::vector<ValueSpec>{
                name == "equal" ? ValueSpec{column_type(boolean_type(), missing), standard_column}
                                : column_output(in[0].type.arguments[0], missing)};
        };
        op.specialize = [name, o](const auto &in, const auto &, const Json &attrs) {
            const auto &e = in[0].type.arguments[0];
            if (e == int64_type())
                return specialization<std::int64_t>(name, in, attrs, o);
            if (e == uint64_type())
                return specialization<std::uint64_t>(name, in, attrs, o);
            if (e == float64_type())
                return specialization<double>(name, in, attrs, o);
            return specialization<Complex128>(name, in, attrs, o);
        };
        library.register_operator(std::move(op));
    }
}
} // namespace gr::data::numeric
