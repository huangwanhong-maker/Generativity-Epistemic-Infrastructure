#include "gr/execution.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using namespace gr::execution;
using gr::kernel::Invocation;
using gr::kernel::Value;
namespace {
int checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F &&function) {
    try {
        function();
    } catch (const gr::Error &error) {
        require(error.code == code, ("Expected " + std::string(code) + ", got " + error.code).c_str());
        return;
    }
    throw std::runtime_error("Expected failure: " + std::string(code));
}
Value bytes(std::string_view text) {
    return Value::bytes("gr.bytes@1", {text.begin(), text.end()});
}
struct Context {
    gr::kernel::ValueTypeRegistry types;
    gr::kernel::OperationRegistry operations;
    ImplementationRegistry implementations;
    Context() { install_cpu_implementations(implementations, operations); }
    ImplementationDescriptor descriptor(std::string id, bool safe = true) {
        return {std::move(id),
                "gr.bytes.identity@1",
                operations.describe("gr.bytes.identity@1").fingerprint(),
                {"cpu.sequential", "cpu.threaded"},
                {"cpu"},
                {"host"},
                gr::kernel::Determinism::Exact,
                safe,
                gr::Json::object()};
    }
    BatchResult run(const ExecutionBackend &backend, std::string_view operation,
                    std::string_view implementation, const Batch &inputs,
                    const ExecutionOptions &options = {}) {
        return backend.execute(types, operations, implementations, operation, implementation, inputs,
                               options);
    }
};
void wait_stop(std::stop_token stop) {
    std::mutex mutex;
    std::condition_variable_any cv;
    std::unique_lock lock(mutex);
    cv.wait(lock, stop, [] { return false; });
}
void discovery_and_parity() {
    Context c;
    SequentialBackend serial;
    ThreadedCpuBackend threaded;
    const auto devices = discover_devices();
    require(devices.size() == 1 && devices[0]["id"] == "cpu:0", "Only actual host CPU advertised");
    require(!serial.capabilities()["parallel"].get<bool>() && threaded.capabilities()["parallel"].get<bool>(),
            "Backend descriptions");
    const Batch batch{{bytes("abc")}, {bytes("")}, {Value::bytes("gr.bytes@1", {0, 255, 0})}};
    const auto original = batch;
    ExecutionOptions options;
    options.max_threads = 3;
    const auto a = c.run(serial, "gr.bytes.sha256@1", "gr.cpu.sha256.oneshot@1", batch);
    const auto b = c.run(threaded, "gr.bytes.sha256@1", "gr.cpu.sha256.incremental@1", batch, options);
    require(a.outputs == b.outputs, "Provider/backend parity");
    require(batch == original, "Borrowed inputs unchanged");
    require(b.metadata["implementation_id"] == "gr.cpu.sha256.incremental@1", "Explicit provider provenance");
    require(b.metadata["threads_used"].get<std::size_t>() <= 3, "Compute worker cap");
    require(b.metadata["device_copies"] == 0, "No device transfers fabricated");
    require(c.implementations.compatible(c.operations.describe("gr.bytes.sha256@1")).size() == 2,
            "Two providers target the same semantic operation");
    require(c.run(serial, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", batch).outputs == batch,
            "Identity");
    const auto concat = c.run(serial, "gr.bytes.concat@1", "gr.cpu.concat@1", {{bytes("ab"), bytes("cd")}});
    require(concat.outputs == Batch{{bytes("abcd")}}, "Multi-input invocation");
    const auto empty = c.run(threaded, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", {});
    require(empty.outputs.empty() && empty.metadata["threads_used"] == 0, "Empty batch uses no worker");
    options.deadline = std::chrono::steady_clock::now() + 1h;
    require(c.run(serial, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", {{bytes("a")}}, options).outputs ==
                Batch{{bytes("a")}},
            "Early success joins a pending deadline monitor");
}
void preflight_refusals() {
    Context c;
    SequentialBackend serial;
    ThreadedCpuBackend threaded;
    std::atomic<int> calls{0};
    auto callback = [&](const Invocation &values, std::stop_token) {
        ++calls;
        return values;
    };
    auto wrong = c.descriptor("test.wrong-contract@1");
    wrong.contract_fingerprint = "sha256:" + std::string(64, '0');
    c.implementations.register_implementation(wrong, callback);
    fails("implementation_contract", [&] { c.run(serial, "gr.bytes.identity@1", wrong.id, {{bytes("a")}}); });
    auto device = c.descriptor("test.wrong-device@1");
    device.device_kinds = {"gpu"};
    c.implementations.register_implementation(device, callback);
    fails("implementation_device", [&] { c.run(serial, "gr.bytes.identity@1", device.id, {{bytes("a")}}); });
    auto backend = c.descriptor("test.wrong-backend@1");
    backend.backend_ids = {"future.accelerator"};
    c.implementations.register_implementation(backend, callback);
    fails("implementation_backend",
          [&] { c.run(serial, "gr.bytes.identity@1", backend.id, {{bytes("a")}}); });
    auto approximate = c.descriptor("test.approximate@1");
    approximate.determinism = gr::kernel::Determinism::DeclaredApproximate;
    c.implementations.register_implementation(approximate, callback);
    fails("implementation_determinism",
          [&] { c.run(serial, "gr.bytes.identity@1", approximate.id, {{bytes("a")}}); });
    auto tested = c.descriptor("test.count-calls@1");
    c.implementations.register_implementation(tested, callback);
    fails("input_type", [&] {
        c.run(serial, "gr.bytes.identity@1", tested.id, {{bytes("a")}, {Value::json("gr.json@1", 3)}});
    });
    fails("input_type", [&] { c.run(serial, "gr.bytes.identity@1", tested.id, {{bytes("a"), bytes("b")}}); });
    require(calls == 0, "Entire batch and provider compatibility checked before any callback");
    fails("unsupported_operation", [&] { c.run(serial, "test.missing@1", tested.id, {}); });
    fails("unsupported_implementation", [&] { c.run(serial, "gr.bytes.identity@1", "test.missing@1", {}); });
    fails("duplicate_implementation", [&] { c.implementations.register_implementation(tested, callback); });
    auto unsafe = c.descriptor("test.serial-only@1", false);
    c.implementations.register_implementation(unsafe, callback);
    fails("operation_not_thread_safe",
          [&] { c.run(threaded, "gr.bytes.identity@1", unsafe.id, {{bytes("a")}}); });
    require(c.run(serial, "gr.bytes.identity@1", unsafe.id, {{bytes("a")}}).outputs == Batch{{bytes("a")}},
            "Non-thread-safe provider supports serial use");
    ExecutionOptions options;
    options.device_id = "gpu:0";
    fails("unsupported_device", [&] { c.run(serial, "gr.bytes.identity@1", tested.id, {}, options); });
    options.device_id = "cpu:0";
    options.max_threads = 0;
    fails("execution_resource_limit", [&] { c.run(serial, "gr.bytes.identity@1", tested.id, {}, options); });
}
void resource_limits() {
    Context c;
    SequentialBackend serial;
    ThreadedCpuBackend threaded;
    const Batch input{{bytes("ab")}, {bytes("cd")}, {bytes("ef")}};
    for (const ExecutionBackend *backend :
         {static_cast<const ExecutionBackend *>(&serial), static_cast<const ExecutionBackend *>(&threaded)}) {
        ExecutionOptions options;
        options.max_threads = 3;
        options.max_input_bytes = 5;
        fails("execution_input_limit",
              [&] { c.run(*backend, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", input, options); });
        options.max_input_bytes = 6;
        options.max_output_bytes = 5;
        bool returned = false;
        fails("execution_output_limit", [&] {
            c.run(*backend, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", input, options);
            returned = true;
        });
        require(!returned, "No partial success result after failure");
        options.max_output_bytes = 6;
        require(c.run(*backend, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", input, options).outputs ==
                    input,
                "Inclusive byte limits");
        options.max_input_bytes = options.max_output_bytes = 0;
        options.max_batch_items = 4;
        const Batch many(5, Invocation{bytes("")});
        fails("execution_batch_limit",
              [&] { c.run(*backend, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", many, options); });
        require(c.run(*backend, "gr.bytes.identity@1", "gr.cpu.identity.copy@1",
                      Batch(4, Invocation{bytes("")}), options)
                        .outputs.size() == 4,
                "Zero-byte item count bounded independently");
    }
}
void workers_and_failure() {
    Context c;
    ThreadedCpuBackend threaded;
    SequentialBackend serial;
    std::atomic<std::size_t> active{0}, peak{0};
    auto descriptor = c.descriptor("test.instrumented@1");
    c.implementations.register_implementation(descriptor, [&](const Invocation &args, std::stop_token token) {
        struct Guard {
            std::atomic<std::size_t> &value;
            ~Guard() { --value; }
        } guard{active};
        const auto count = ++active;
        auto old = peak.load();
        while (old < count && !peak.compare_exchange_weak(old, count)) {
        }
        if (args[0].as_bytes() == gr::kernel::Bytes{42})
            throw std::runtime_error("intentional physical failure");
        if (!token.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(5 - args[0].as_bytes().size() % 5));
        return args;
    });
    ExecutionOptions options;
    options.max_threads = 2;
    const Batch inputs{{bytes("a")}, {bytes("bc")}, {bytes("def")}, {bytes("g")}};
    require(c.run(threaded, "gr.bytes.identity@1", descriptor.id, inputs, options).outputs == inputs,
            "Stable result order despite completion order");
    require(active == 0 && peak <= 2, "Observed callback concurrency honors worker budget");
    fails("execution_failed", [&] {
        c.run(threaded, "gr.bytes.identity@1", descriptor.id,
              {{bytes("a")}, {Value::bytes("gr.bytes@1", {42})}, {bytes("b")}}, options);
    });
    require(active == 0, "Every started worker joined after failure");
    auto unsafe = c.descriptor("test.shared-serial@1", false);
    active = 0;
    peak = 0;
    c.implementations.register_implementation(unsafe, [&](const Invocation &args, std::stop_token) {
        const auto count = ++active;
        auto old = peak.load();
        while (old < count && !peak.compare_exchange_weak(old, count)) {
        }
        std::this_thread::sleep_for(5ms);
        --active;
        return args;
    });
    auto first = std::async(std::launch::async,
                            [&] { return c.run(serial, "gr.bytes.identity@1", unsafe.id, inputs); });
    auto second = std::async(std::launch::async,
                             [&] { return c.run(serial, "gr.bytes.identity@1", unsafe.id, inputs); });
    require(first.get().outputs == inputs && second.get().outputs == inputs && peak == 1,
            "Non-thread-safe provider serialized across callers");
}
void cancellation() {
    Context c;
    SequentialBackend serial;
    ThreadedCpuBackend threaded;
    std::stop_source stopped;
    stopped.request_stop();
    ExecutionOptions options;
    options.stop_token = stopped.get_token();
    fails("execution_cancelled",
          [&] { c.run(serial, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", {{bytes("a")}}, options); });
    options.stop_token = {};
    options.deadline = std::chrono::steady_clock::now() - 1ms;
    fails("execution_deadline",
          [&] { c.run(serial, "gr.bytes.identity@1", "gr.cpu.identity.copy@1", {{bytes("a")}}, options); });
    auto cooperative = c.descriptor("test.cooperative@1");
    c.implementations.register_implementation(cooperative, [](const Invocation &args, std::stop_token stop) {
        wait_stop(stop);
        return args;
    });
    options.max_threads = 2;
    options.deadline = std::chrono::steady_clock::now() + 30ms;
    fails("execution_deadline", [&] {
        c.run(threaded, "gr.bytes.identity@1", cooperative.id, {{bytes("a")}, {bytes("b")}}, options);
    });
    options.deadline = std::chrono::steady_clock::now() + 30ms;
    fails("execution_deadline",
          [&] { c.run(serial, "gr.bytes.identity@1", cooperative.id, {{bytes("a")}}, options); });
    std::promise<void> started;
    auto ready = started.get_future();
    std::stop_source source;
    auto live = c.descriptor("test.live-stop@1");
    c.implementations.register_implementation(live, [&](const Invocation &args, std::stop_token stop) {
        started.set_value();
        wait_stop(stop);
        return args;
    });
    std::jthread cancel([&] {
        ready.wait_for(5s);
        source.request_stop();
    });
    options.deadline.reset();
    options.stop_token = source.get_token();
    fails("execution_cancelled",
          [&] { c.run(threaded, "gr.bytes.identity@1", live.id, {{bytes("a")}}, options); });
}
} // namespace
int main() {
    try {
        discovery_and_parity();
        preflight_refusals();
        resource_limits();
        workers_and_failure();
        cancellation();
        std::cout << "execution: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "execution failure: " << error.what() << '\n';
        return 1;
    }
}
