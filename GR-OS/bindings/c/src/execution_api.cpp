#include "gr/execution.hpp"
#include "gr/kernel/operations.hpp"
#include "internal.hpp"
#include <chrono>
#include <memory>
#include <set>
#include <stop_token>

namespace gr::binding {
namespace {
void charge(std::size_t size, std::size_t &total, std::size_t limit) {
    if (total > limit || size > limit - total)
        throw Error("execution_input_limit", "Input exceeds configured byte limit");
    total += size;
}
std::vector<std::uint8_t> bounded_decode(const std::string &value, std::size_t &total, std::size_t limit) {
    std::size_t estimated = value.size() / 4 * 3;
    if (!value.empty() && value.back() == '=' && estimated)
        --estimated;
    if (value.size() > 1 && value[value.size() - 2] == '=' && estimated)
        --estimated;
    charge(estimated, total, limit);
    return decode(value);
}
} // namespace
kernel::Value value_from_json(const Json &value, std::size_t &used, std::size_t limit) {
    const auto type = text(value, "type"), representation = text(value, "representation");
    if (representation == "bytes")
        return kernel::Value::bytes(type, bounded_decode(text(value, "data_base64"), used, limit));
    if (representation == "json") {
        charge(canonical(value.at("data")).size(), used, limit);
        return kernel::Value::json(type, value.at("data"));
    }
    if (representation == "buffers") {
        std::vector<memory::NamedBuffer> named;
        const auto &carrier = value.at("data");
        for (const auto &entry : array(carrier, "buffers")) {
            if (named.size() >= 64)
                throw Error("request_limit", "Buffer carrier exceeds 64 named views");
            named.push_back({text(entry, "name"), buffer_for(text(entry, "handle"))});
        }
        auto result = kernel::Value::buffers(
            type, memory::BufferSet(std::move(named), carrier.value("metadata", Json::object())));
        charge(result.byte_size(), used, limit);
        return result;
    }
    throw Error("invalid_request", "Value representation must be json, bytes or buffers");
}
Json value_json(const kernel::Value &value) {
    if (value.representation() == kernel::Representation::Buffers) {
        Json named = Json::array();
        for (const auto &entry : value.as_buffers().buffers()) {
            auto item = publish_buffer(entry.view);
            item["name"] = entry.name;
            named.push_back(std::move(item));
        }
        return {{"type", value.type},
                {"representation", "buffers"},
                {"data", {{"buffers", named}, {"metadata", value.as_buffers().metadata()}}}};
    }
    if (value.representation() == kernel::Representation::Bytes)
        return {{"type", value.type}, {"representation", "bytes"}, {"data_base64", encode(value.as_bytes())}};
    return {{"type", value.type}, {"representation", "json"}, {"data", value.as_json()}};
}
std::optional<Json> execution_call(const Json &request, const std::string &op) {
    if (op == "execution.devices")
        return execution::discover_devices();
    if (op != "execution.execute" && op != "execution.execute_typed" && op != "execution.capabilities" &&
        op != "execution.contracts")
        return std::nullopt;
    kernel::ValueTypeRegistry types;
    kernel::OperationRegistry operations;
    execution::ImplementationRegistry implementations;
    execution::install_cpu_implementations(implementations, operations);
    if (op == "execution.contracts")
        return Json{{"types", types.capabilities()},
                    {"operations", operations.capabilities()},
                    {"implementations", implementations.capabilities()}};
    std::unique_ptr<execution::ExecutionBackend> backend;
    const auto selected = request.value("backend", std::string("cpu.sequential"));
    if (selected == "cpu.sequential")
        backend = std::make_unique<execution::SequentialBackend>();
    else if (selected == "cpu.threaded")
        backend = std::make_unique<execution::ThreadedCpuBackend>();
    else
        throw Error("unsupported_backend", "Execution backend must be cpu.sequential or cpu.threaded");
    if (op == "execution.capabilities")
        return Json{{"backend", backend->capabilities()},
                    {"operations", operations.capabilities()},
                    {"implementations", implementations.capabilities()}};
    const auto config = request.value("options", Json::object());
    if (!config.is_object())
        throw Error("invalid_request", "Execution options must be an object");
    static const std::set<std::string> allowed = {"device_id",       "max_threads",      "max_batch_items",
                                                  "max_input_bytes", "max_output_bytes", "cancelled",
                                                  "deadline_ms"};
    for (auto it = config.begin(); it != config.end(); ++it)
        if (!allowed.contains(it.key()))
            throw Error("invalid_request", "Unknown execution option: " + it.key());
    execution::ExecutionOptions options;
    options.device_id = config.value("device_id", options.device_id);
    options.max_threads = count(config, "max_threads", options.max_threads);
    options.max_batch_items = count(config, "max_batch_items", options.max_batch_items);
    options.max_input_bytes = count(config, "max_input_bytes", options.max_input_bytes);
    options.max_output_bytes = count(config, "max_output_bytes", options.max_output_bytes);
    std::stop_source cancellation;
    if (config.value("cancelled", false))
        cancellation.request_stop();
    options.stop_token = cancellation.get_token();
    if (config.contains("deadline_ms")) {
        const auto ms = count(config, "deadline_ms", 0);
        if (ms > 86400000)
            throw Error("invalid_request", "deadline_ms exceeds one day");
        options.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    }
    const bool legacy = op == "execution.execute";
    std::string operation = text(request, "operation"), implementation;
    if (legacy) {
        if (request.value("version", std::string("1")) != "1")
            throw Error("unsupported_operation_version", "Byte convenience interface supports version 1");
        if (operation == "identity") {
            operation = "gr.bytes.identity@1";
            implementation = "gr.cpu.identity.copy@1";
        } else if (operation == "sha256") {
            operation = "gr.bytes.sha256@1";
            implementation = "gr.cpu.sha256.oneshot@1";
        } else
            throw Error("unsupported_operation", "Unknown byte convenience operation");
    } else
        implementation = text(request, "implementation");
    const auto &encoded = array(request, legacy ? "inputs_base64" : "invocations");
    if (encoded.size() > options.max_batch_items)
        throw Error("execution_batch_limit", "Batch item count exceeds max_batch_items");
    execution::Batch inputs;
    std::size_t used = 0;
    for (const auto &entry : encoded) {
        kernel::Invocation arguments;
        if (legacy) {
            if (!entry.is_string())
                throw Error("invalid_request", "Each batch input must be base64 text");
            arguments.push_back(kernel::Value::bytes(
                "gr.bytes@1", bounded_decode(entry.get<std::string>(), used, options.max_input_bytes)));
        } else {
            if (!entry.is_array())
                throw Error("invalid_request", "Each invocation must be an argument array");
            if (entry.size() > 1024)
                throw Error("request_limit", "Invocation arity exceeds 1024");
            for (const auto &argument : entry)
                arguments.push_back(value_from_json(argument, used, options.max_input_bytes));
        }
        inputs.push_back(std::move(arguments));
    }
    auto result =
        backend->execute(types, operations, implementations, operation, implementation, inputs, options);
    Json outputs = Json::array();
    for (const auto &invocation : result.outputs) {
        if (legacy)
            outputs.push_back(encode(invocation.at(0).as_bytes()));
        else {
            Json values = Json::array();
            for (const auto &value : invocation)
                values.push_back(value_json(value));
            outputs.push_back(std::move(values));
        }
    }
    return Json{{legacy ? "outputs_base64" : "outputs", outputs}, {"metadata", result.metadata}};
}
} // namespace gr::binding
