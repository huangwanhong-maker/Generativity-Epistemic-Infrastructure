#include "gr/execution/cpu.hpp"
#include <iostream>
using namespace gr;
using namespace gr::kernel;
using namespace gr::execution;
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
        check(error.code == code, "Expected prepared failure code");
        return;
    }
    throw std::runtime_error("Expected prepared failure");
}
} // namespace
int main() {
    try {
        std::shared_ptr<const PreparedExecutor> retained;
        {
            ValueTypeRegistry types;
            OperationRegistry operations;
            ImplementationRegistry implementations;
            install_cpu_implementations(implementations, operations);
            SequentialBackend backend;
            retained = backend.prepare(types, operations, implementations, "gr.bytes.sha256@1",
                                       "gr.cpu.sha256.incremental@1", backend.targets()[0]);
            const Invocation input{Value::bytes("gr.bytes@1", {0, 1, 2, 255})};
            check(retained->invoke(input, {}, 4, 64) == backend
                                                            .execute(types, operations, implementations,
                                                                     "gr.bytes.sha256@1",
                                                                     "gr.cpu.sha256.incremental@1", {input})
                                                            .outputs[0],
                  "Prepared/direct conformance");
            check(retained->cpu_slots() == 1, "Prepared execution uses caller-owned single slot");
        }
        const Invocation empty{Value::bytes("gr.bytes@1", {})};
        const auto result = retained->invoke(empty, {}, 0, 64);
        check(std::string(result[0].as_bytes().begin(), result[0].as_bytes().end()) ==
                  "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
              "Independent vector after registries destroyed");
        fails("input_type", [&] { retained->invoke({Value::json("gr.json@1", 1)}, {}, 100, 100); });
        fails("execution_input_limit",
              [&] { retained->invoke({Value::bytes("gr.bytes@1", {1})}, {}, 0, 100); });
        fails("execution_output_limit", [&] { retained->invoke(empty, {}, 0, 63); });
        std::stop_source stop;
        stop.request_stop();
        fails("execution_cancelled", [&] { retained->invoke(empty, stop.get_token(), 0, 64); });
        auto original = Value::bytes("gr.bytes@1", {1, 2, 3});
        auto copied = original;
        check(&copied.as_bytes() == &original.as_bytes(), "Byte values share immutable owned payload");
        check(copied == original && copied != Value::bytes("gr.bytes@1", {3, 2, 1}),
              "Shared value equality remains semantic");
        auto json = Value::json("gr.json@1", Json{{"a", 2}});
        auto json_copy = json;
        check(&json.as_json() == &json_copy.as_json(), "JSON values share immutable owned payload");
        std::cout << "prepared: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "prepared failure: " << error.what() << '\n';
        return 1;
    }
}
