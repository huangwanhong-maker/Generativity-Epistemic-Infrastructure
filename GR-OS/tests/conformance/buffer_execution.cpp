#include "gr/execution/cpu.hpp"
#include "gr/runtime/scheduler.hpp"
#include <condition_variable>
#include <future>
#include <iostream>
using namespace gr;
using namespace gr::kernel;
using namespace gr::memory;
using namespace gr::execution;
using namespace gr::runtime;
using namespace std::chrono_literals;
namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F function) {
    try {
        function();
    } catch (const Error &error) {
        if (error.code != code)
            throw std::runtime_error(std::string("Expected ") + code + ", got " + error.code);
        ++checks;
        return;
    }
    throw std::runtime_error(std::string("Expected ") + code);
}
ValueTypeDescriptor type() {
    return {"fixture.buffer-value@1",
            Representation::Buffers,
            "One named data buffer; physical byte view preserved exactly.",
            {{"min_buffers", 1}, {"max_buffers", 1}, {"required_buffers", {"data"}}}};
}
OperationDescriptor operation() {
    OperationDescriptor op;
    op.id = "fixture.buffer-identity@1";
    op.inputs = op.outputs = {"fixture.buffer-value@1"};
    op.meaning = "Return exactly the supplied immutable physical buffer-set representation.";
    return op;
}
ImplementationDescriptor provider(const OperationDescriptor &op) {
    return {"fixture.buffer-provider@1",
            op.id,
            op.fingerprint(),
            {"cpu.sequential", "cpu.threaded"},
            {"cpu"},
            {"host"},
            Determinism::Exact,
            true};
}
Value input(std::shared_ptr<AllocationBudget> budget = {}) {
    const std::vector<std::uint8_t> bytes(64, 7);
    BufferView view(Buffer::copy(bytes, 64, std::move(budget)), {}, {64});
    return Value::buffers("fixture.buffer-value@1", BufferSet({{"data", std::move(view)}}));
}
void direct_and_prepared() {
    ValueTypeRegistry types;
    types.register_type(type());
    OperationRegistry operations;
    auto op = operation();
    pin_types(op, types);
    operations.register_operation(op, {}, [](const Invocation &in, const Invocation &out) {
        if (in != out)
            throw Error("postcondition", "Buffer identity changed its value.");
    });
    ImplementationRegistry implementations;
    implementations.register_implementation(provider(op),
                                            [](const Invocation &in, std::stop_token) { return in; });
    const Invocation values{input()};
    SequentialBackend serial;
    ThreadedCpuBackend threaded;
    const auto prepared =
        serial.prepare(types, operations, implementations, op.id, provider(op).id, serial.targets()[0]);
    const auto output = prepared->invoke(values, {}, 4096, 4096);
    check(output == values && output[0].as_buffers().at("data").buffer().owner_id() ==
                                  values[0].as_buffers().at("data").buffer().owner_id(),
          "Prepared calls retain immutable buffer without payload copy");
    auto limit = values[0].byte_size();
    fails("execution_input_limit", [&] { prepared->invoke(values, {}, limit - 1, 4096); });
    fails("execution_output_limit", [&] { prepared->invoke(values, {}, 4096, limit - 1); });
    check(values[0].logical_bytes() == 64 && values[0].retained_bytes() == 64 && values[0].byte_size() > 64,
          "Value distinguishes logical owner and accounted bytes");
    ExecutionOptions options;
    options.max_threads = 4;
    Batch batch(16, values);
    check(serial.execute(types, operations, implementations, op.id, provider(op).id, batch).outputs ==
              threaded.execute(types, operations, implementations, op.id, provider(op).id, batch, options)
                  .outputs,
          "Sequential and threaded buffer batches agree");
    std::stop_source stopped;
    stopped.request_stop();
    fails("execution_cancelled", [&] { prepared->invoke(values, stopped.get_token(), 4096, 4096); });
    fails("representation", [&] { values[0].as_json(); });
    fails("input_type", [&] { prepared->invoke({Value::bytes("gr.bytes@1", {})}, {}, 4096, 4096); });
    fails("buffer_name", [&] {
        types.validate(Value::buffers(type().id, BufferSet({{"wrong", values[0].as_buffers().at("data")}})));
    });
    auto broken = type();
    broken.id = "fixture.bad-constraints@1";
    broken.constraints["max_buffers"] = 0;
    fails("type_constraints", [&] { types.register_type(broken); });
}
struct Gate {
    std::promise<void> entered;
    std::mutex mutex;
    std::condition_variable_any changed;
    bool release = false;
};
std::shared_ptr<const PreparedPlan> plan(std::shared_ptr<AllocationBudget> budget,
                                         std::shared_ptr<Gate> gate) {
    auto catalog = std::make_shared<Catalog>();
    catalog->register_type(type());
    auto op = operation();
    catalog->pin_types(op);
    catalog->register_operation(op);
    catalog->register_implementation(provider(op), [gate](const Invocation &in, std::stop_token stop) {
        gate->entered.set_value();
        std::unique_lock lock(gate->mutex);
        if (!gate->changed.wait(lock, stop, [&] { return gate->release; }))
            throw Error("execution_cancelled",
                        "Cancellation releases owned operands after callback completion.");
        return in;
    });
    catalog->seal();
    return Planner(catalog).compile(Graph{{{"copy", op.id, {input(std::move(budget))}}}, {{"copy", 0}}});
}
void asynchronous_lifetime(bool cancel) {
    auto budget = std::make_shared<AllocationBudget>(64);
    auto gate = std::make_shared<Gate>();
    auto entered = gate->entered.get_future();
    auto prepared = plan(budget, gate);
    Runtime runtime({.workers = 1});
    JobOptions options;
    options.max_retained_bytes = 4096;
    auto job = runtime.submit(prepared, options);
    prepared.reset();
    check(entered.wait_for(5s) == std::future_status::ready, "Asynchronous callback reached gate");
    check(budget->used() == 64, "Job pins allocation after all caller/catalog/plan owners are gone");
    if (cancel)
        job.cancel();
    else {
        {
            std::lock_guard lock(gate->mutex);
            gate->release = true;
        }
        gate->changed.notify_all();
    }
    auto result = job.wait();
    runtime.shutdown();
    job.release();
    if (cancel) {
        check(result.status == JobStatus::cancelled && result.outputs.empty(),
              "Cancelled buffer job has no partial result");
        result = {};
        check(budget->used() == 0, "Cancelled callback joined and all buffer ownership released");
    } else {
        check(result.status == JobStatus::succeeded &&
                  result.outputs[0].as_buffers().at("data").contiguous_bytes()[0] == 7,
              "Completed result retains correct asynchronous bytes");
        auto extracted = result.outputs[0];
        result = {};
        check(budget->used() == 64,
              "Allocation budget survives extracted Value aliases outside runtime quota");
        extracted = {};
        check(budget->used() == 0, "Final extracted alias releases allocation budget");
    }
}
} // namespace
int main() {
    try {
        direct_and_prepared();
        asynchronous_lifetime(false);
        asynchronous_lifetime(true);
        std::cout << "buffer execution: " << checks << " checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << "buffer execution failure: " << error.what() << '\n';
        return 1;
    }
}
