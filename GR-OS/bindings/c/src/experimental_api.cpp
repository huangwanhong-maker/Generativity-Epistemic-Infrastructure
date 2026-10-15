#include "gr/experimental/data.hpp"
#include "internal.hpp"

namespace gr::binding {
std::optional<Json> experimental_call(const Json &request, const std::string &op) {
    if (op != "experimental.data.select" && op != "experimental.data.analyze")
        return {};
    runtime::TransactionCoordinator coordinator(store_for(request));
    auto selection = experimental::data::select(
        coordinator, text(request, "workspace"), text(request, "record_id"), text(request, "payload_pointer"),
        request.value("semantic_revision", std::string{}), request.value("physical_revision", std::string{}));
    if (op == "experimental.data.select")
        return Json{{"column", value_json(selection.column)}, {"provenance", selection.provenance}};
    auto parameter = [&](const char *key, std::int64_t fallback) {
        if (!request.contains(key))
            return fallback;
        const auto &value = request.at(key);
        if (!value.is_number_integer() ||
            (value.is_number_unsigned() && value.get<std::uint64_t>() > INT64_MAX))
            throw Error("invalid_request", std::string(key) + " must be a canonical integer");
        (void)canonical(value);
        return value.get<std::int64_t>();
    };
    const auto multiplier = parameter("multiplier", 1), addend = parameter("addend", 0);
    auto catalog = std::make_shared<runtime::Catalog>();
    experimental::data::install(*catalog);
    catalog->seal();
    runtime::Planner planner(catalog);
    auto plan = planner.compile(experimental::data::transform_sum(selection, multiplier, addend));
    runtime::RuntimeConfig config;
    config.workers = count(request, "workers", 2);
    runtime::Runtime runtime(config);
    runtime::JobOptions options;
    if (request.contains("serial") && !request.at("serial").is_boolean())
        throw Error("invalid_request", "serial must be boolean");
    options.serial = request.value("serial", false);
    auto result = runtime.run(plan, options);
    Json outputs = Json::array();
    for (const auto &value : result.outputs)
        outputs.push_back(value_json(value));
    auto provenance = selection.provenance;
    provenance["recipe"] = {{"module", "gr.experimental.data@1"},
                            {"multiplier", multiplier},
                            {"addend", addend},
                            {"plan", plan->describe()}};
    provenance["attribution"] = {{"agent", "gr.experimental.data@1"},
                                 {"method", "explicit selection and checked ordered transform/sum"}};
    return Json{{"status", runtime::to_string(result.status)},
                {"outputs", outputs},
                {"provenance", provenance},
                {"diagnostics", result.diagnostics},
                {"error", result.error}};
}
} // namespace gr::binding
