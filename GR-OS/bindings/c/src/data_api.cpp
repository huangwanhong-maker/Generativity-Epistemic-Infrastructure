#include "gr/data/language.hpp"
#include "gr/data/numeric.hpp"
#include "gr/data/relational.hpp"
#include "gr/data/sparse.hpp"
#include "gr/data/standard.hpp"
#include "internal.hpp"
#include <atomic>
#include <bit>
#include <cstring>
#include <map>
#include <mutex>
#include <set>

namespace gr::binding {
namespace {
struct Context {
    explicit Context(data::ValidationCacheLimits limits) : library(std::make_shared<data::Library>(limits)) {}
    std::shared_ptr<data::Library> library;
    data::StandardOptions options;
    std::shared_ptr<data::relational::Session> relational;
};
struct ProgramEntry {
    data::Program program;
    std::string plan_handle;
};
std::mutex mutex;
std::atomic<std::uint64_t> sequence{1};
std::map<std::string, std::shared_ptr<Context>> contexts;
std::map<std::string, std::shared_ptr<const data::DataValue>> values;
std::map<std::string, data::Expr> expressions;
std::map<std::string, std::shared_ptr<ProgramEntry>> programs;
std::map<std::string, std::shared_ptr<const data::Function>> functions;
enum class Kind { context, value, expression, program, function };
thread_local std::vector<std::pair<Kind, std::string>> exported;

void keys(const Json &object, std::initializer_list<const char *> allowed) {
    if (!object.is_object())
        throw Error("invalid_request", "Expected an object");
    const std::set<std::string> names(allowed.begin(), allowed.end());
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!names.contains(it.key()))
            throw Error("invalid_request", "Unknown field: " + it.key());
}
template <class T> T acquire(const std::map<std::string, T> &entries, const std::string &handle) {
    std::lock_guard lock(mutex);
    const auto found = entries.find(handle);
    if (found == entries.end())
        throw Error("invalid_handle", "Data resource is closed or unknown");
    return found->second;
}
template <class T> T remove(std::map<std::string, T> &entries, const std::string &handle) {
    std::lock_guard lock(mutex);
    const auto found = entries.find(handle);
    if (found == entries.end())
        throw Error("invalid_handle", "Data resource is closed or unknown");
    T result = std::move(found->second);
    entries.erase(found);
    return result;
}
void release(Kind kind, const std::string &handle) {
    switch (kind) {
    case Kind::context:
        (void)remove(contexts, handle);
        break;
    case Kind::value:
        (void)remove(values, handle);
        break;
    case Kind::expression:
        (void)remove(expressions, handle);
        break;
    case Kind::function:
        (void)remove(functions, handle);
        break;
    case Kind::program: {
        auto entry = remove(programs, handle);
        if (!entry->plan_handle.empty())
            discard_plan(entry->plan_handle);
        break;
    }
    }
}
template <class T>
std::string publish(std::map<std::string, T> &entries, T value, Kind kind, const char *prefix,
                    std::size_t capacity) {
    auto handle = std::string(prefix) + std::to_string(sequence.fetch_add(1));
    exported.emplace_back(kind, handle);
    std::lock_guard lock(mutex);
    if (entries.size() >= capacity)
        throw Error("resource_limit", "Data handle capacity reached");
    entries.emplace(handle, std::move(value));
    return handle;
}
Json publish_value(data::DataValue value) {
    Json response{{"spec", value.spec.to_json()}, {"provenance", value.provenance}};
    response["handle"] = publish(values, std::make_shared<const data::DataValue>(std::move(value)),
                                 Kind::value, "data.value:", 4096);
    return response;
}
Json publish_expression(data::Expr value) {
    Json response{{"expression", value.describe()}, {"spec", value.spec().to_json()}};
    response["handle"] = publish(expressions, std::move(value), Kind::expression, "data.expr:", 8192);
    return response;
}
Json publish_program(data::Program program) {
    auto entry = std::make_shared<ProgramEntry>();
    entry->program = std::move(program);
    Json response{{"program", entry->program.describe()}};
    response["handle"] = publish(programs, entry, Kind::program, "data.program:", 1024);
    auto plan = publish_plan(entry->program.plan());
    try {
        entry->plan_handle = plan.at("handle").get<std::string>();
        response["plan"] = std::move(plan);
    } catch (...) {
        if (!entry->plan_handle.empty())
            discard_plan(entry->plan_handle);
        else
            discard_plan(plan.at("handle").get_ref<const std::string &>());
        throw;
    }
    return response;
}
Json publish_function(std::shared_ptr<const data::Function> function) {
    Json description = function->describe();
    Json inputs = Json::object();
    for (const auto &[name, spec] : function->inputs())
        inputs[name] = spec.to_json();
    return {{"handle", publish(functions, std::move(function), Kind::function, "data.function:", 1024)},
            {"function", std::move(description)},
            {"inputs", std::move(inputs)}};
}
data::ValidationCacheLimits cache_limits(const Json &value) {
    keys(value, {"max_entries", "max_bytes", "profile_timing"});
    data::ValidationCacheLimits limits;
    limits.max_entries = count(value, "max_entries", limits.max_entries);
    limits.max_bytes = count(value, "max_bytes", limits.max_bytes);
    if (value.contains("profile_timing") && !value.at("profile_timing").is_boolean())
        throw Error("invalid_request", "profile_timing must be boolean");
    limits.profile_timing = value.value("profile_timing", false);
    return limits;
}
data::DataValue value_from_request(const Json &value) {
    if (value.is_string())
        return *acquire(values, value.get<std::string>());
    keys(value, {"spec", "payload", "provenance"});
    keys(value.at("payload"), {"type", "representation", "data", "data_base64"});
    std::size_t used = 0;
    return {data::ValueSpec::from_json(value.at("spec")),
            value_from_json(value.at("payload"), used, 64 * 1024 * 1024),
            value.value("provenance", Json::object())};
}
std::vector<data::Expr> expression_list(const Json &request, const char *name) {
    const auto &input = array(request, name);
    if (input.size() > 4096)
        throw Error("request_limit", "Expression list exceeds 4096");
    std::vector<data::Expr> result;
    for (const auto &value : input) {
        if (!value.is_string())
            throw Error("invalid_request", "Expression handle must be a string");
        result.push_back(acquire(expressions, value.get<std::string>()));
    }
    return result;
}
data::CallOptions options_from_json(const Json &input) {
    keys(input, {"selection", "max_input_bytes", "max_output_bytes"});
    data::CallOptions result;
    result.max_input_bytes = count(input, "max_input_bytes", result.max_input_bytes);
    result.max_output_bytes = count(input, "max_output_bytes", result.max_output_bytes);
    if (input.contains("selection")) {
        const auto &selection = input.at("selection");
        keys(selection, {"implementation", "backend", "device", "prefer_implementations"});
        if (selection.contains("implementation"))
            result.selection.implementation = text(selection, "implementation");
        if (selection.contains("backend"))
            result.selection.backend = text(selection, "backend");
        if (selection.contains("device"))
            result.selection.device = text(selection, "device");
        if (selection.contains("prefer_implementations"))
            for (const auto &id : array(selection, "prefer_implementations")) {
                if (!id.is_string())
                    throw Error("invalid_request", "Implementation preference must be a string");
                result.selection.prefer_implementations.push_back(id.get<std::string>());
            }
    }
    return result;
}
// Transport staging only: semantic construction and validation remain in modules.
template <class T> std::vector<T> binary_items(const std::string &handle, std::size_t limit) {
    const auto view = buffer_for(handle);
    const auto &element = view.element();
    if (element.format_id != "gr.opaque-octet@1" || element.item_size != 1 ||
        element.byte_order != memory::ByteOrder::NotApplicable || !element.parameters.empty())
        throw Error("invalid_request", "Constructor staging requires opaque octets containing packed "
                                       "little-endian items; use data.import for typed buffers");
    if (!view.c_contiguous() || view.logical_bytes() % sizeof(T))
        throw Error("invalid_request", "Constructor input must be contiguous complete little-endian items");
    const auto bytes = view.contiguous_bytes();
    if (bytes.size() / sizeof(T) > limit)
        throw Error("data_limit", "Constructor input exceeds element limit");
    std::vector<T> result(bytes.size() / sizeof(T));
    for (std::size_t i = 0; i < result.size(); ++i) {
        if constexpr (sizeof(T) == 1)
            result[i] = static_cast<T>(bytes[i]);
        else {
            std::uint64_t bits = 0;
            for (std::size_t j = 0; j < sizeof(T); ++j)
                bits |= static_cast<std::uint64_t>(bytes[i * sizeof(T) + j]) << (8 * j);
            result[i] = std::bit_cast<T>(bits);
        }
    }
    return result;
}
data::DataValue construct_value(Context &context, const Json &request) {
    const auto kind = text(request, "kind");
    if (kind == "scalar_i64" || kind == "scalar_u64" || kind == "scalar_f64" || kind == "scalar_c128")
        keys(request, {"abi", "op", "handle", "kind", "values"});
    else if (kind == "column_i64" || kind == "column_bool" || kind == "column_f64" || kind == "column_u64" ||
             kind == "numeric_column_f64" || kind == "column_c128")
        keys(request, {"abi", "op", "handle", "kind", "values", "validity"});
    else if (kind == "table")
        keys(request, {"abi", "op", "handle", "kind", "columns"});
    else if (kind == "csr_i64")
        keys(request, {"abi", "op", "handle", "kind", "rows", "columns", "indptr", "indices", "values"});
    auto &options = context.options;
    std::vector<std::uint8_t> validity;
    data::Validity validity_view;
    if (request.contains("validity")) {
        validity = binary_items<std::uint8_t>(text(request, "validity"), options.max_elements);
        validity_view = validity;
    }
    if (kind == "scalar_i64") {
        auto items = binary_items<std::int64_t>(text(request, "values"), 1);
        if (items.size() != 1 || validity_view)
            throw Error("invalid_request", "Scalar needs exactly one item and no validity");
        return data::scalar_i64(items[0], options);
    }
    if (kind == "column_i64")
        return data::column_i64(binary_items<std::int64_t>(text(request, "values"), options.max_elements),
                                validity_view, options);
    if (kind == "column_bool")
        return data::column_bool(binary_items<std::uint8_t>(text(request, "values"), options.max_elements),
                                 validity_view, options);
    if (kind == "column_f64")
        return data::column_f64(binary_items<double>(text(request, "values"), options.max_elements),
                                validity_view, options);
    if (kind == "column_u64")
        return data::numeric::column_u64(
            binary_items<std::uint64_t>(text(request, "values"), options.max_elements), validity_view,
            options);
    if (kind == "numeric_column_f64")
        return data::numeric::column_f64(binary_items<double>(text(request, "values"), options.max_elements),
                                         validity_view, options);
    if (kind == "scalar_u64") {
        const auto items = binary_items<std::uint64_t>(text(request, "values"), 1);
        if (items.size() != 1)
            throw Error("invalid_request", "Scalar needs exactly one item");
        return data::numeric::scalar_u64(items[0], options);
    }
    if (kind == "scalar_f64") {
        const auto items = binary_items<double>(text(request, "values"), 1);
        if (items.size() != 1)
            throw Error("invalid_request", "Scalar needs exactly one item");
        return data::numeric::scalar_f64(items[0], options);
    }
    if (kind == "column_c128" || kind == "scalar_c128") {
        const auto items = binary_items<double>(text(request, "values"),
                                                kind == "scalar_c128" ? 2 : options.max_elements * 2);
        if (items.size() % 2 || (kind == "scalar_c128" && items.size() != 2))
            throw Error("invalid_request", "Complex128 requires complete pairs of binary64 values");
        std::vector<data::numeric::Complex128> pairs;
        pairs.reserve(items.size() / 2);
        for (std::size_t i = 0; i < items.size(); i += 2)
            pairs.push_back({items[i], items[i + 1]});
        return kind == "scalar_c128" ? data::numeric::scalar_c128(pairs[0], options)
                                     : data::numeric::column_c128(pairs, validity_view, options);
    }
    if (validity_view)
        throw Error("invalid_request", "Validity is only supported for columns");
    if (kind == "table") {
        std::vector<std::pair<std::string, data::DataValue>> columns;
        for (const auto &column : array(request, "columns")) {
            keys(column, {"name", "value"});
            if (columns.size() >= options.max_columns)
                throw Error("data_limit", "Table exceeds column limit");
            columns.emplace_back(text(column, "name"), value_from_request(column.at("value")));
        }
        return data::make_table(columns, options);
    }
    if (kind == "csr_i64") {
        const auto rows = count(request, "rows", 0), columns = count(request, "columns", 0);
        return data::csr_i64(
            rows, columns, binary_items<std::int64_t>(text(request, "indptr"), options.max_elements + 1),
            binary_items<std::int64_t>(text(request, "indices"), options.max_elements),
            binary_items<std::int64_t>(text(request, "values"), options.max_elements), options);
    }
    throw Error("unsupported_operation", "Unknown data constructor", {{"kind", kind}});
}
Json items_json(const data::DataValue &value) {
    const auto &type = value.spec.type;
    if (type.family == "gr.data.int64@1")
        return data::get_scalar_i64(value);
    if (type.family == "gr.data.optional@1") {
        if (type.arguments.at(0).family == "gr.numeric.uint64@1") {
            const auto item = data::numeric::get_optional_u64(value);
            return item ? Json(*item) : Json(nullptr);
        }
        if (type.arguments.at(0).family == "gr.data.float64@1") {
            const auto item = data::numeric::get_optional_f64(value);
            return item ? Json(*item) : Json(nullptr);
        }
        if (type.arguments.at(0).family == "gr.numeric.complex@1") {
            const auto item = data::numeric::get_optional_c128(value);
            return item ? Json{{"complex_value", Json::array({item->real, item->imag})}} : Json(nullptr);
        }
        const auto item = data::get_optional_i64(value);
        return item ? Json(*item) : Json(nullptr);
    }
    if (type.family == "gr.numeric.uint64@1") {
        const auto item = data::numeric::get_optional_u64(value);
        return item ? Json(*item) : Json(nullptr);
    }
    if (type.family == "gr.data.float64@1") {
        const auto item = data::numeric::get_optional_f64(value);
        return item ? Json(*item) : Json(nullptr);
    }
    if (type.family == "gr.numeric.complex@1") {
        const auto item = data::numeric::get_optional_c128(value);
        return item ? Json{{"complex_value", Json::array({item->real, item->imag})}} : Json(nullptr);
    }
    if (type.family != "gr.data.column@1")
        throw Error("unsupported_conversion", "Use payload or explicit relational inspect for this type");
    Json result = Json::array();
    const auto family = type.arguments.at(0).family;
    if (family == "gr.data.int64@1")
        for (auto item : data::to_i64(value))
            result.push_back(item ? Json(*item) : Json(nullptr));
    else if (family == "gr.data.boolean@1")
        for (auto item : data::to_bool(value))
            result.push_back(item ? Json(*item) : Json(nullptr));
    else if (family == "gr.data.float64@1")
        for (auto item : value.spec.representation == "gr.numeric.column-le@1" ? data::numeric::to_f64(value)
                                                                               : data::to_f64(value))
            result.push_back(item ? Json(*item) : Json(nullptr));
    else if (family == "gr.numeric.uint64@1")
        for (auto item : data::numeric::to_u64(value))
            result.push_back(item ? Json(*item) : Json(nullptr));
    else if (family == "gr.numeric.complex@1")
        for (auto item : data::numeric::to_c128(value))
            result.push_back(item ? Json::array({item->real, item->imag}) : Json(nullptr));
    else
        throw Error("unsupported_conversion", "No Python item conversion for this column element type");
    return family == "gr.numeric.complex@1" ? Json{{"complex_values", result}} : result;
}
} // namespace
std::size_t data_export_checkpoint() {
    return exported.size();
}
void data_export_finish(std::size_t checkpoint, bool success) noexcept {
    while (exported.size() > checkpoint) {
        auto entry = std::move(exported.back());
        exported.pop_back();
        if (!success)
            try {
                release(entry.first, entry.second);
            } catch (...) {
            }
    }
}
std::optional<Json> data_call(const Json &request, const std::string &op) {
    if (op.rfind("data.", 0) != 0)
        return std::nullopt;
    if (op == "data.open") {
        keys(request, {"abi", "op", "modules", "validation_cache", "relational_cache"});
        auto context =
            std::make_shared<Context>(cache_limits(request.value("validation_cache", Json::object())));
        const auto relation_limits = cache_limits(request.value("relational_cache", Json::object()));
        std::set<std::string> modules;
        for (const auto &module : array(request, "modules")) {
            if (!module.is_string() || !modules.insert(module.get<std::string>()).second)
                throw Error("invalid_request", "Modules must be unique strings");
        }
        if (modules.contains("gr.data.sparse@1") && !modules.contains("gr.data.standard@1"))
            throw Error("missing_module", "The sparse module requires the standard module");
        if (modules.contains("gr.data.numeric@1") && !modules.contains("gr.data.standard@1"))
            throw Error("missing_module", "The numeric module requires the standard module");
        if (modules.contains("gr.data.standard@1"))
            data::install_standard(*context->library, context->options);
        if (modules.contains("gr.data.sparse@1"))
            data::install_sparse(*context->library, context->options);
        if (modules.contains("gr.data.numeric@1"))
            data::numeric::install(*context->library, context->options);
        if (modules.contains("gr.data.relational@1"))
            context->relational = data::relational::install(
                *context->library, {},
                {relation_limits.max_entries, relation_limits.max_bytes, relation_limits.profile_timing});
        for (const auto &module : modules)
            if (module != "gr.data.standard@1" && module != "gr.data.sparse@1" &&
                module != "gr.data.relational@1" && module != "gr.data.numeric@1")
                throw Error("unsupported_module", "Native data module is not installed",
                            {{"module", module}});
        context->library->seal();
        Json result{{"capabilities", context->library->capabilities()}};
        result["handle"] = publish(contexts, context, Kind::context, "data.context:", 32);
        return result;
    }
    if (op == "data.function.close") {
        keys(request, {"abi", "op", "handle"});
        release(Kind::function, text(request, "handle"));
        return Json{{"closed", true}};
    }
    if (op == "data.function.create") {
        keys(request, {"abi", "op", "outputs"});
        return publish_function(std::make_shared<const data::Function>(expression_list(request, "outputs")));
    }
    if (op.rfind("data.function.", 0) == 0) {
        auto function = acquire(functions, text(request, "handle"));
        if (op == "data.function.describe" || op == "data.function.retain") {
            keys(request, {"abi", "op", "handle"});
            return op == "data.function.describe" ? function->describe() : publish_function(function);
        }
        keys(request, {"abi", "op", "handle", "bindings"});
        const auto input = request.value("bindings", Json::object());
        if (!input.is_object() || input.size() > 4096)
            throw Error("invalid_request", "Bindings must be a bounded object");
        if (op == "data.function.bind") {
            data::Bindings bindings;
            for (auto it = input.begin(); it != input.end(); ++it)
                bindings.emplace(it.key(), value_from_request(it.value()));
            return publish_program(function->bind(bindings));
        }
        if (op == "data.function.instantiate") {
            std::map<std::string, data::Expr, std::less<>> bindings;
            for (auto it = input.begin(); it != input.end(); ++it) {
                if (!it.value().is_string())
                    throw Error("invalid_request", "Expression binding must be a handle");
                bindings.emplace(it.key(), acquire(expressions, it.value().get<std::string>()));
            }
            Json outputs = Json::array();
            for (auto expression : function->instantiate(bindings))
                outputs.push_back(publish_expression(std::move(expression)));
            return Json{{"outputs", std::move(outputs)}};
        }
        throw Error("unsupported_operation", "Unknown function operation");
    }
    if (op == "data.close" || op == "data.value.close" || op == "data.expr.close" ||
        op == "data.program.close") {
        keys(request, {"abi", "op", "handle"});
        release(op == "data.close"         ? Kind::context
                : op == "data.value.close" ? Kind::value
                : op == "data.expr.close"  ? Kind::expression
                                           : Kind::program,
                text(request, "handle"));
        return Json{{"closed", true}};
    }
    if (op.rfind("data.value.", 0) == 0) {
        keys(request, {"abi", "op", "handle"});
        auto value = acquire(values, text(request, "handle"));
        if (op == "data.value.retain")
            return publish_value(*value);
        if (op == "data.value.payload")
            return value_json(value->payload);
        if (op == "data.value.items")
            return items_json(*value);
        if (op == "data.value.inspect")
            return data::relational::inspect(*value);
        if (op == "data.value.describe")
            return Json{{"spec", value->spec.to_json()}, {"provenance", value->provenance}};
    }
    if (op == "data.expr.describe" || op == "data.expr.retain") {
        keys(request, {"abi", "op", "handle"});
        auto expression = acquire(expressions, text(request, "handle"));
        return op == "data.expr.describe" ? expression.describe() : publish_expression(std::move(expression));
    }
    if (op == "data.program.retain" || op == "data.program.describe") {
        keys(request, {"abi", "op", "handle"});
        auto program = acquire(programs, text(request, "handle"));
        return op == "data.program.describe" ? program->program.describe()
                                             : publish_program(program->program);
    }
    if (op == "data.compile") {
        keys(request, {"abi", "op", "outputs", "bindings"});
        data::Bindings bindings;
        const auto source = request.value("bindings", Json::object());
        if (!source.is_object() || source.size() > 4096)
            throw Error("invalid_request", "Bindings must be a bounded object");
        for (auto it = source.begin(); it != source.end(); ++it)
            bindings.emplace(it.key(), value_from_request(it.value()));
        return publish_program(data::compile(expression_list(request, "outputs"), bindings));
    }
    if (op == "data.result") {
        keys(request, {"abi", "op", "handle", "job", "timeout_ms"});
        const auto milliseconds = count(request, "timeout_ms", 0);
        if (milliseconds > 60000)
            throw Error("invalid_request", "wait exceeds60000ms");
        auto program = acquire(programs, text(request, "handle"));
        auto job = job_for(text(request, "job"));
        if (!job.wait_for(std::chrono::milliseconds(milliseconds)))
            return Json{{"ready", false}, {"status", job.status()}};
        const auto result = job.wait();
        Json output = Json::array();
        if (result.status == runtime::JobStatus::succeeded)
            for (auto value : program->program.materialize(result))
                output.push_back(publish_value(std::move(value)));
        return Json{{"ready", true},
                    {"result",
                     {{"id", result.id},
                      {"status", runtime::to_string(result.status)},
                      {"outputs", output},
                      {"publication", result.publication},
                      {"error", result.error},
                      {"diagnostics", result.diagnostics}}}};
    }
    auto context = acquire(contexts, text(request, "handle"));
    if (op == "data.validation.statistics" || op == "data.validation.clear") {
        keys(request, {"abi", "op", "handle"});
        if (op == "data.validation.clear")
            context->library->clear_validation_cache();
        return context->library->validation_statistics();
    }
    if (op == "data.numeric.capabilities") {
        keys(request, {"abi", "op", "handle"});
        return data::numeric::floating_capabilities();
    }
    if (op == "data.relational.statistics" || op == "data.relational.clear" ||
        op == "data.relational.inspect") {
        if (op == "data.relational.inspect")
            keys(request, {"abi", "op", "handle", "value"});
        else
            keys(request, {"abi", "op", "handle"});
        if (!context->relational)
            throw Error("missing_module", "Relational module is not installed");
        if (op == "data.relational.clear")
            context->relational->clear_cache();
        if (op == "data.relational.inspect")
            return context->relational->inspect(*acquire(values, text(request, "value")));
        return context->relational->statistics();
    }
    if (op == "data.capabilities") {
        keys(request, {"abi", "op", "handle"});
        return context->library->capabilities();
    }
    if (op == "data.create") {
        keys(request,
             {"abi", "op", "handle", "kind", "values", "validity", "columns", "rows", "indptr", "indices"});
        auto value = construct_value(*context, request);
        context->library->validate(value);
        return publish_value(std::move(value));
    }
    if (op == "data.import") {
        keys(request, {"abi", "op", "handle", "value"});
        auto value = value_from_request(request.at("value"));
        context->library->validate(value);
        return publish_value(std::move(value));
    }
    if (op == "data.history") {
        keys(request,
             {"abi", "op", "handle", "store", "workspace", "semantic_revision", "physical_revision"});
        runtime::TransactionCoordinator coordinator(store_for({{"handle", text(request, "store")}}));
        data::relational::HistoricalSelection selection{text(request, "workspace"),
                                                        request.value("semantic_revision", ""),
                                                        request.value("physical_revision", "")};
        auto value = data::relational::select_history(coordinator, selection);
        context->library->validate(value);
        return publish_value(std::move(value));
    }
    if (op == "data.literal") {
        keys(request, {"abi", "op", "handle", "value"});
        return publish_expression(data::literal(context->library, value_from_request(request.at("value"))));
    }
    if (op == "data.input") {
        keys(request, {"abi", "op", "handle", "name", "spec"});
        return publish_expression(data::input(context->library, text(request, "name"),
                                              data::ValueSpec::from_json(request.at("spec"))));
    }
    if (op == "data.call") {
        keys(request, {"abi", "op", "handle", "operation", "inputs", "attributes", "options"});
        auto result =
            data::call(context->library, text(request, "operation"), expression_list(request, "inputs"),
                       request.value("attributes", Json::object()),
                       options_from_json(request.value("options", Json::object())));
        Json outputs = Json::array();
        for (auto &expression : result)
            outputs.push_back(publish_expression(std::move(expression)));
        return Json{{"outputs", outputs}};
    }
    throw Error("unsupported_operation", "Unknown data operation", {{"op", op}});
}
} // namespace gr::binding
