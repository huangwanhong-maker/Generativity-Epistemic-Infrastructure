#include "gr/execution/cpu.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <limits>
#include <picosha2.h>
#include <thread>

namespace gr::execution {
namespace {
std::size_t cpu_capacity() {
    return std::max(1u, std::thread::hardware_concurrency());
}
void require(bool condition, const char *code, const char *message) {
    if (!condition)
        throw Error(code, message);
}
bool contains(const std::vector<std::string> &values, std::string_view value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
void check_active(const ExecutionOptions &options, std::stop_token token) {
    if (options.stop_token.stop_requested())
        throw Error("execution_cancelled", "Execution was cancelled by the caller.");
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline)
        throw Error("execution_deadline", "Execution exceeded its cooperative deadline.");
    if (token.stop_requested())
        throw Error("execution_cancelled", "Execution was stopped.");
}
void check_token(std::stop_token token) {
    if (token.stop_requested())
        throw Error("execution_cancelled", "CPU implementation was stopped.");
}
kernel::Invocation invoke_provider(const RegisteredImplementation &implementation,
                                   const kernel::Invocation &inputs, std::stop_token token) {
    check_token(token);
    try {
        if (implementation.descriptor.thread_safe)
            return implementation.function(inputs, token);
        std::lock_guard lock(*implementation.serialization_mutex);
        check_token(token);
        return implementation.function(inputs, token);
    } catch (const Error &) {
        throw;
    } catch (const std::exception &error) {
        throw Error("execution_failed", error.what());
    } catch (...) {
        throw Error("execution_failed", "Native provider threw an unknown exception.");
    }
}
void check_bytes(const kernel::Invocation &values, std::size_t limit, const char *code) {
    std::size_t used = 0;
    for (const auto &value : values) {
        const auto size = value.byte_size();
        require(size <= limit - used, code, "Invocation exceeds its payload byte limit.");
        used += size;
    }
}
class HostExecutor final : public PreparedExecutor {
  public:
    HostExecutor(kernel::PreparedOperation operation, RegisteredImplementation implementation, Json metadata)
        : operation_(std::move(operation)), implementation_(std::move(implementation)),
          metadata_(std::move(metadata)) {}
    kernel::Invocation invoke(const kernel::Invocation &inputs, std::stop_token token,
                              std::size_t input_limit, std::size_t output_limit) const override {
        check_token(token);
        check_bytes(inputs, input_limit, "execution_input_limit");
        operation_.validate_inputs(inputs);
        check_token(token);
        auto outputs = invoke_provider(implementation_, inputs, token);
        check_token(token);
        check_bytes(outputs, output_limit, "execution_output_limit");
        operation_.validate_outputs(inputs, outputs);
        check_token(token);
        return outputs;
    }
    std::size_t cpu_slots() const override { return 1; }
    Json description() const override { return metadata_; }

  private:
    kernel::PreparedOperation operation_;
    RegisteredImplementation implementation_;
    Json metadata_;
};
Json description(std::string_view backend, bool parallel) {
    return {{"backend", backend},
            {"version", "2"},
            {"device_kind", "cpu"},
            {"device_ids", Json::array({"cpu:0"})},
            {"parallel", parallel},
            {"binding", "gr.host-values/1"},
            {"memory", "host"},
            {"input_ownership", "borrowed_until_return"},
            {"output_ownership", "owned"},
            {"device_transfer", false},
            {"effects", Json::array({"pure"})},
            {"determinism", Json::array({"exact"})},
            {"stable_output_order", true},
            {"partial_results", false},
            {"cancellation", "cooperative"},
            {"deadline", "cooperative"},
            {"max_threads_scope", "compute_workers"},
            {"deadline_monitor_threads_max", 1},
            {"batch_item_limit", true},
            {"byte_limits_scope", "aggregate_payload_bytes"},
            {"callback_allocation_limit", false},
            {"callback_isolation", false},
            {"callback_effect_rollback", false}};
}
BatchResult run(const kernel::ValueTypeRegistry &types, const kernel::OperationRegistry &operations,
                const ImplementationRegistry &implementations, std::string_view operation_id,
                std::string_view implementation_id, const Batch &inputs, const ExecutionOptions &options,
                bool parallel, std::string_view backend) {
    require(options.device_id == "cpu:0", "unsupported_device", "This backend supports only cpu:0.");
    require(options.max_threads > 0, "execution_resource_limit", "max_threads must be positive.");
    require(inputs.size() <= options.max_batch_items, "execution_batch_limit",
            "Batch exceeds max_batch_items.");
    const auto prepared_operation = operations.prepare(operation_id, types);
    const auto &operation = prepared_operation.descriptor();
    const auto implementation = implementations.resolve(implementation_id);
    const auto &provider = implementation.descriptor;
    require(operation.effect == kernel::Effect::Pure, "unsupported_effect",
            "Host execution currently supports pure operations only.");
    require(operation.determinism == kernel::Determinism::Exact, "unsupported_determinism",
            "Host execution currently supports exact operations only.");
    require(provider.operation == operation.id &&
                provider.contract_fingerprint == prepared_operation.fingerprint(),
            "implementation_contract",
            "Implementation does not target this exact semantic operation contract.");
    require(provider.determinism == operation.determinism, "implementation_determinism",
            "Implementation does not promise the required determinism.");
    require(contains(provider.backend_ids, backend), "implementation_backend",
            "Implementation does not support the selected backend.");
    require(contains(provider.device_kinds, "cpu") && contains(provider.memory_domains, "host"),
            "implementation_device", "Implementation does not support host CPU memory.");
    require(!parallel || provider.thread_safe, "operation_not_thread_safe",
            "Threaded execution requires a thread-safe implementation.");

    std::stop_source cancellation;
    std::stop_callback caller_stop(options.stop_token, [&] { cancellation.request_stop(); });
    check_active(options, cancellation.get_token());
    std::size_t input_bytes = 0;
    Json input_types = Json::object(), output_types = Json::object();
    for (const auto &type : operation.inputs)
        input_types[type] = types.describe(type).fingerprint();
    for (const auto &type : operation.outputs)
        output_types[type] = types.describe(type).fingerprint();
    // Validate the entire batch before invoking any physical callback.
    for (const auto &arguments : inputs) {
        check_active(options, cancellation.get_token());
        prepared_operation.validate_inputs(arguments);
        for (const auto &value : arguments) {
            const auto size = value.byte_size();
            require(size <= options.max_input_bytes - input_bytes, "execution_input_limit",
                    "Batch exceeds max_input_bytes.");
            input_bytes += size;
        }
    }
    std::jthread monitor;
    if (options.deadline && !inputs.empty()) {
        monitor = std::jthread([&cancellation, deadline = *options.deadline](std::stop_token stop) {
            std::mutex mutex;
            std::condition_variable_any changed;
            std::unique_lock lock(mutex);
            changed.wait_until(lock, stop, deadline, [] { return false; });
            if (!stop.stop_requested() && std::chrono::steady_clock::now() >= deadline)
                cancellation.request_stop();
        });
    }
    Batch outputs(inputs.size());
    std::size_t output_bytes = 0;
    std::mutex output_mutex;
    auto one = [&](std::size_t index) {
        check_active(options, cancellation.get_token());
        kernel::Invocation output;
        try {
            output = invoke_provider(implementation, inputs[index], cancellation.get_token());
        } catch (const Error &) {
            check_active(options, cancellation.get_token());
            throw;
        } catch (const std::exception &error) {
            check_active(options, cancellation.get_token());
            throw Error("execution_failed", error.what(),
                        {{"implementation", implementation_id}, {"input_index", index}});
        } catch (...) {
            check_active(options, cancellation.get_token());
            throw Error("execution_failed", "Native implementation threw an unknown exception.");
        }
        check_active(options, cancellation.get_token());
        prepared_operation.validate_outputs(inputs[index], output);
        std::size_t size = 0;
        for (const auto &value : output) {
            const auto next = value.byte_size();
            require(next <= options.max_output_bytes - size, "execution_output_limit",
                    "Result exceeds max_output_bytes.");
            size += next;
        }
        std::lock_guard lock(output_mutex);
        require(size <= options.max_output_bytes - output_bytes, "execution_output_limit",
                "Batch exceeds max_output_bytes.");
        output_bytes += size;
        outputs[index] = std::move(output);
    };
    const std::size_t worker_count =
        inputs.empty() ? 0 : (parallel ? std::min({options.max_threads, cpu_capacity(), inputs.size()}) : 1);
    if (!parallel) {
        for (std::size_t i = 0; i < inputs.size(); ++i)
            one(i);
    } else {
        std::atomic<std::size_t> next{0};
        std::exception_ptr failure;
        std::mutex failure_mutex;
        std::vector<std::jthread> workers;
        workers.reserve(worker_count);
        try {
            for (std::size_t i = 0; i < worker_count; ++i)
                workers.emplace_back([&] {
                    try {
                        while (!cancellation.stop_requested()) {
                            const auto index = next.fetch_add(1);
                            if (index >= inputs.size())
                                break;
                            one(index);
                        }
                    } catch (...) {
                        {
                            std::lock_guard lock(failure_mutex);
                            if (!failure)
                                failure = std::current_exception();
                        }
                        cancellation.request_stop();
                    }
                });
        } catch (...) {
            cancellation.request_stop();
            for (auto &worker : workers)
                worker.join();
            throw;
        }
        for (auto &worker : workers)
            worker.join();
        if (failure)
            std::rethrow_exception(failure);
    }
    check_active(options, cancellation.get_token());
    return {std::move(outputs),
            {{"backend", backend},
             {"backend_version", "2"},
             {"binding", "gr.host-values/1"},
             {"operation", operation.id},
             {"contract_fingerprint", operation.fingerprint()},
             {"implementation_id", provider.id},
             {"implementation", provider.to_json()},
             {"device", options.device_id},
             {"input_type_fingerprints", input_types},
             {"output_type_fingerprints", output_types},
             {"effect", kernel::to_string(operation.effect)},
             {"determinism", kernel::to_string(provider.determinism)},
             {"threads_used", worker_count},
             {"deadline_monitor_threads", monitor.joinable() ? 1 : 0},
             {"input_count", inputs.size()},
             {"input_bytes", input_bytes},
             {"output_bytes", output_bytes},
             {"buffer_accounting",
              "per value max(logical bytes, distinct retained owner bytes) plus descriptors"},
             {"memory", "host"},
             {"input_ownership", "borrowed_until_return"},
             {"output_ownership", "owned"},
             {"device_copies", 0},
             {"validation_scope", "exact descriptors and types, declared native pre/postconditions; not "
                                  "universal algorithm verification"}}};
}
} // namespace
std::shared_ptr<const PreparedExecutor> ExecutionBackend::prepare(const kernel::ValueTypeRegistry &,
                                                                  const kernel::OperationRegistry &,
                                                                  const ImplementationRegistry &,
                                                                  std::string_view, std::string_view,
                                                                  const ExecutionTarget &) const {
    throw Error("unsupported_preparation", "Backend does not implement prepared execution.");
}
std::shared_ptr<const PreparedExecutor>
prepare_host_execution(const kernel::ValueTypeRegistry &types, const kernel::OperationRegistry &operations,
                       const ImplementationRegistry &implementations, std::string_view operation_id,
                       std::string_view implementation_id, std::string_view backend,
                       const ExecutionTarget &target) {
    require(target.device == "cpu:0" && target.kind == "cpu" && target.memory == "host" &&
                target.binding == "gr.host-values/1",
            "unsupported_device", "Host binding requires cpu:0 and host memory.");
    auto operation = operations.prepare(operation_id, types);
    auto implementation = implementations.resolve(implementation_id);
    const auto &provider = implementation.descriptor;
    require(operation.descriptor().effect == kernel::Effect::Pure, "unsupported_effect",
            "Host binding executes only pure operations.");
    require(operation.descriptor().determinism == kernel::Determinism::Exact, "unsupported_determinism",
            "Host binding executes only exact operations.");
    require(provider.operation == operation.descriptor().id &&
                provider.contract_fingerprint == operation.fingerprint(),
            "implementation_contract", "Provider does not match the exact semantic contract.");
    require(provider.determinism == kernel::Determinism::Exact, "implementation_determinism",
            "Provider does not promise exact results.");
    require(contains(provider.backend_ids, backend), "implementation_backend",
            "Provider does not support this backend.");
    require(contains(provider.device_kinds, target.kind) && contains(provider.memory_domains, target.memory),
            "implementation_device", "Provider does not support this target.");
    Json metadata{{"backend", backend},
                  {"backend_version", "3"},
                  {"device", target.device},
                  {"device_kind", target.kind},
                  {"memory", target.memory},
                  {"binding", target.binding},
                  {"operation", operation.descriptor().id},
                  {"contract_fingerprint", operation.fingerprint()},
                  {"implementation_id", provider.id},
                  {"implementation", provider.to_json()},
                  {"cpu_slots", 1},
                  {"execution", "single invocation on caller-owned thread"},
                  {"validation_scope",
                   "pinned descriptions and native value/pre/postconditions; no universal code proof"}};
    return std::make_shared<HostExecutor>(std::move(operation), std::move(implementation),
                                          std::move(metadata));
}
Json discover_devices() {
    return Json::array({{{"id", "cpu:0"},
                         {"kind", "cpu"},
                         {"memory", "host"},
                         {"logical_threads_reported", std::thread::hardware_concurrency()},
                         {"worker_capacity", cpu_capacity()},
                         {"discovery_scope", "host_cpu_only"}}});
}
void install_cpu_implementations(ImplementationRegistry &implementations,
                                 const kernel::OperationRegistry &operations) {
    auto add = [&](std::string id, std::string operation, ImplementationFunction function, Json metadata) {
        implementations.register_implementation({std::move(id),
                                                 operation,
                                                 operations.describe(operation).fingerprint(),
                                                 {"cpu.sequential", "cpu.threaded"},
                                                 {"cpu"},
                                                 {"host"},
                                                 kernel::Determinism::Exact,
                                                 true,
                                                 std::move(metadata)},
                                                std::move(function));
    };
    add("gr.cpu.identity.copy@1", "gr.bytes.identity@1",
        [](const kernel::Invocation &args, std::stop_token token) {
            check_token(token);
            return kernel::Invocation{kernel::Value::bytes("gr.bytes@1", args[0].as_bytes())};
        },
        {{"algorithm", "owned vector copy"}});
    add("gr.cpu.sha256.oneshot@1", "gr.bytes.sha256@1",
        [](const kernel::Invocation &args, std::stop_token token) {
            check_token(token);
            const auto value = gr::sha256(std::span<const std::uint8_t>(args[0].as_bytes()));
            return kernel::Invocation{
                kernel::Value::bytes("gr.sha256-hex@1", kernel::Bytes(value.begin(), value.end()))};
        },
        {{"algorithm", "whole-buffer SHA-256"},
         {"library", "PicoSHA2"},
         {"cancellation_granularity", "before and after hashing"}});
    add("gr.cpu.sha256.incremental@1", "gr.bytes.sha256@1",
        [](const kernel::Invocation &args, std::stop_token token) {
            const auto &bytes = args[0].as_bytes();
            picosha2::hash256_one_by_one hash;
            constexpr std::size_t chunk = 4096;
            for (std::size_t offset = 0; offset < bytes.size();) {
                check_token(token);
                const auto count = std::min(chunk, bytes.size() - offset);
                hash.process(bytes.begin() + offset, bytes.begin() + offset + count);
                offset += count;
            }
            check_token(token);
            hash.finish();
            const auto value = picosha2::get_hash_hex_string(hash);
            return kernel::Invocation{
                kernel::Value::bytes("gr.sha256-hex@1", kernel::Bytes(value.begin(), value.end()))};
        },
        {{"algorithm", "incremental SHA-256"},
         {"library", "PicoSHA2"},
         {"chunk_bytes", 4096},
         {"independence_limit", "Separate provider code paths share the pinned cryptographic primitive."}});
    add("gr.cpu.concat@1", "gr.bytes.concat@1",
        [](const kernel::Invocation &args, std::stop_token token) {
            check_token(token);
            const auto &a = args[0].as_bytes();
            const auto &b = args[1].as_bytes();
            require(b.size() <= std::numeric_limits<std::size_t>::max() - a.size(), "execution_output_limit",
                    "Concatenation size overflow.");
            kernel::Bytes value;
            value.reserve(a.size() + b.size());
            value.insert(value.end(), a.begin(), a.end());
            check_token(token);
            value.insert(value.end(), b.begin(), b.end());
            return kernel::Invocation{kernel::Value::bytes("gr.bytes@1", std::move(value))};
        },
        {{"algorithm", "ordered vector concatenation"}});
}
std::string SequentialBackend::id() const {
    return "cpu.sequential";
}
Json SequentialBackend::capabilities() const {
    return description(id(), false);
}
std::vector<ExecutionTarget> SequentialBackend::targets() const {
    return {{"cpu:0", "cpu", "host"}};
}
std::shared_ptr<const PreparedExecutor>
SequentialBackend::prepare(const kernel::ValueTypeRegistry &types,
                           const kernel::OperationRegistry &operations,
                           const ImplementationRegistry &implementations, std::string_view operation,
                           std::string_view implementation, const ExecutionTarget &target) const {
    return prepare_host_execution(types, operations, implementations, operation, implementation, id(),
                                  target);
}
BatchResult SequentialBackend::execute(const kernel::ValueTypeRegistry &types,
                                       const kernel::OperationRegistry &operations,
                                       const ImplementationRegistry &implementations,
                                       std::string_view operation, std::string_view implementation,
                                       const Batch &inputs, const ExecutionOptions &options) const {
    return run(types, operations, implementations, operation, implementation, inputs, options, false, id());
}
std::string ThreadedCpuBackend::id() const {
    return "cpu.threaded";
}
Json ThreadedCpuBackend::capabilities() const {
    return description(id(), true);
}
std::vector<ExecutionTarget> ThreadedCpuBackend::targets() const {
    return {{"cpu:0", "cpu", "host"}};
}
std::shared_ptr<const PreparedExecutor>
ThreadedCpuBackend::prepare(const kernel::ValueTypeRegistry &types,
                            const kernel::OperationRegistry &operations,
                            const ImplementationRegistry &implementations, std::string_view operation,
                            std::string_view implementation, const ExecutionTarget &target) const {
    return prepare_host_execution(types, operations, implementations, operation, implementation, id(),
                                  target);
}
BatchResult ThreadedCpuBackend::execute(const kernel::ValueTypeRegistry &types,
                                        const kernel::OperationRegistry &operations,
                                        const ImplementationRegistry &implementations,
                                        std::string_view operation, std::string_view implementation,
                                        const Batch &inputs, const ExecutionOptions &options) const {
    return run(types, operations, implementations, operation, implementation, inputs, options, true, id());
}
} // namespace gr::execution
