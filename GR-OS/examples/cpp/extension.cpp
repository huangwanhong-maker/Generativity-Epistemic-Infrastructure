#include <algorithm>
#include <gr/execution.hpp>
#include <iostream>

int main() {
    using namespace gr::kernel;
    using namespace gr::execution;
    ValueTypeRegistry types;
    OperationRegistry operations;
    OperationDescriptor reverse;
    reverse.id = "example.bytes.reverse@1";
    reverse.inputs = {"gr.bytes@1"};
    reverse.outputs = {"gr.bytes@1"};
    reverse.meaning = "Reverse the order of input octets exactly.";
    reverse.invariants = {"Output length equals input length", "Applying twice restores input"};
    pin_types(reverse, types);
    operations.register_operation(reverse, {}, [](const Invocation &input, const Invocation &output) {
        const auto &a = input[0].as_bytes();
        const auto &b = output[0].as_bytes();
        if (a.size() != b.size() || !std::equal(a.begin(), a.end(), b.rbegin()))
            throw gr::Error("reverse_conformance", "Provider did not reverse the octets");
    });
    ImplementationDescriptor descriptor;
    descriptor.id = "example.cpu.reverse@1";
    descriptor.operation = reverse.id;
    descriptor.contract_fingerprint = reverse.fingerprint();
    descriptor.backend_ids = {"cpu.sequential", "cpu.threaded"};
    descriptor.device_kinds = {"cpu"};
    descriptor.memory_domains = {"host"};
    descriptor.thread_safe = true;
    ImplementationRegistry implementations;
    implementations.register_implementation(descriptor, [](const Invocation &input, std::stop_token stop) {
        if (stop.stop_requested())
            throw gr::Error("execution_cancelled", "Cancelled before reverse");
        auto bytes = input[0].as_bytes();
        std::reverse(bytes.begin(), bytes.end());
        return Invocation{Value::bytes("gr.bytes@1", std::move(bytes))};
    });
    ExecutionOptions options;
    options.max_threads = 2;
    auto result = ThreadedCpuBackend{}.execute(
        types, operations, implementations, reverse.id, descriptor.id,
        {{Value::bytes("gr.bytes@1", {1, 2, 3})}, {Value::bytes("gr.bytes@1", {})}}, options);
    if (result.outputs[0][0].as_bytes() != Bytes({3, 2, 1}))
        return 1;
    std::cout << result.metadata.dump(2) << '\n';
}
