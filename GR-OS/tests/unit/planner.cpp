#include "gr/runtime/planner.hpp"
#include "gr/core.hpp"
#include "gr/execution/cpu.hpp"
#include <iostream>
using namespace gr;
using namespace gr::kernel;
using namespace gr::runtime;
namespace {
int checks = 0;
void check(bool result, const char *message) {
    ++checks;
    if (!result)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F function) {
    try {
        function();
    } catch (const Error &error) {
        check(error.code == code, (std::string(code) + ": got " + error.code).c_str());
        return;
    }
    throw std::runtime_error(std::string("Expected ") + code);
}
Value bytes(std::string_view text) {
    return Value::bytes("gr.bytes@1", Bytes(text.begin(), text.end()));
}
Graph single(std::string_view text = "a") {
    return {{{"one", "gr.bytes.identity@1", {bytes(text)}}}, {{"one", 0}}};
}
Invocation evaluate(const PreparedPlan &plan) {
    std::vector<std::shared_ptr<const Invocation>> results(plan.nodes().size());
    std::size_t remaining = results.size();
    while (remaining) {
        bool progress = false;
        for (std::size_t i = 0; i < results.size(); ++i) {
            if (results[i])
                continue;
            bool ready = true;
            for (auto dependency : plan.nodes()[i].dependencies)
                ready &= bool(results[dependency]);
            if (!ready)
                continue;
            results[i] = std::make_shared<Invocation>(
                plan.execute_node(i, plan.assemble_inputs(i, results), {}, 1024 * 1024));
            --remaining;
            progress = true;
        }
        check(progress, "Validated graph progresses");
    }
    Invocation output;
    for (const auto &ref : plan.outputs())
        output.push_back(results[ref.node]->at(ref.output));
    return output;
}
void graphs_and_cache() {
    auto catalog = std::make_shared<Catalog>();
    fails("catalog_unsealed", [&] { Planner unused(catalog); });
    catalog->seal();
    Planner planner(catalog, {.max_nodes = 100,
                              .max_edges = 100,
                              .max_literal_bytes = 1024,
                              .max_plan_bytes = 1024 * 1024,
                              .cache_entries = 1,
                              .cache_payload_bytes = 1024 * 1024});
    auto graph = single();
    graph.nodes.push_back({"two", "gr.bytes.concat@1", {OutputRef{"one", 0}, bytes("b")}, {"one"}});
    graph.outputs = {{"two", 0}};
    auto first = planner.compile(graph);
    check(evaluate(*first) == Invocation{bytes("ab")}, "Data/control dependencies compose semantic calls");
    check(first->nodes()[1].dependencies.size() == 1, "Repeated data/control edge counts once in schedule");
    check(planner.compile(graph) == first, "Equivalent request reuses prepared cache entry");
    const auto assembled = first->assemble_inputs(0, {});
    check(&assembled[0].as_bytes() == &std::get<Value>(graph.nodes[0].inputs[0]).as_bytes(),
          "Literal payload shared without whole-buffer edge copy");
    auto altered = graph;
    altered.nodes[0].inputs[0] = bytes("c");
    auto second = planner.compile(altered);
    check(second->id() != first->id(), "Literal change changes plan identity");
    check(planner.cache_info()["entries"] == 1, "Cache entry cap enforced");
    check(evaluate(*first) == Invocation{bytes("ab")}, "Eviction does not invalidate a live plan");
    altered = graph;
    altered.nodes[1].max_output_bytes = 7;
    check(planner.compile(altered)->id() != first->id(), "Resource contract changes plan identity");
    altered = graph;
    altered.nodes[1].after.clear();
    check(planner.compile(altered)->id() != first->id(), "Declared control constraints are in plan identity");
    altered = graph;
    altered.context = {{"source", "revision-a"}};
    auto contextual = planner.compile(altered);
    check(contextual->id() != first->id() && contextual->describe()["context"] == altered.context,
          "Explicit context is pinned and described without an execution node");
    check(evaluate(*contextual) == evaluate(*first), "Context does not change computational output");
    altered.context["source"] = "revision-b";
    check(planner.compile(altered)->id() != contextual->id(),
          "Context changes invalidate plan identity/cache");
    altered.context = Json::object();
    check(planner.compile(altered)->id() == first->id() && !first->describe().contains("context"),
          "Empty context preserves prior plan identity and description");
    altered.context = Json::array();
    fails("plan_context", [&] { planner.compile(altered); });
    altered.context = {{"too_large", std::string(1024 * 1024, 'x')}};
    fails("value_limit", [&] { planner.compile(altered); });
    check(planner.compile({})->nodes().empty(), "Empty graph is an explicit empty plan");
    auto duplicate = graph;
    duplicate.nodes[1].id = "one";
    fails("duplicate_node", [&] { planner.compile(duplicate); });
    auto cycle = graph;
    cycle.nodes[0].after = {"two"};
    fails("graph_cycle", [&] { planner.compile(cycle); });
    auto missing = graph;
    missing.nodes[1].inputs[0] = OutputRef{"missing", 0};
    fails("graph_reference", [&] { planner.compile(missing); });
    auto slot = graph;
    slot.outputs[0].output = 1;
    fails("graph_reference", [&] { planner.compile(slot); });
    auto wrong = graph;
    wrong.nodes[0].inputs[0] = Value::json("gr.json@1", 1);
    fails("input_type", [&] { planner.compile(wrong); });
    auto wrongedge = graph;
    wrongedge.nodes[0].operation = "gr.bytes.sha256@1";
    fails("input_type", [&] { planner.compile(wrongedge); });
    auto unused = graph;
    unused.nodes.push_back({"unselected", "future.missing@1", {}});
    fails("unsupported_operation", [&] { planner.compile(unused); });
    auto unavailable = graph;
    unavailable.nodes[0].selection.device = "gpu:0";
    fails("no_compatible_implementation", [&] { planner.compile(unavailable); });
    auto large = single(std::string(1025, 'x'));
    fails("plan_limit", [&] { planner.compile(large); });
    auto limited = single();
    limited.nodes[0].max_output_bytes = 0;
    fails("execution_output_limit", [&] { evaluate(*planner.compile(limited)); });
    fails("dependency_unavailable", [&] { first->assemble_inputs(1, {}); });
    std::stop_source cancelled;
    cancelled.request_stop();
    fails("execution_cancelled", [&] { first->execute_node(0, assembled, cancelled.get_token(), 100); });
    fails("catalog_sealed", [&] { catalog->register_module({"test.module@1", {}}); });
}
void selection_and_lifetime() {
    std::shared_ptr<const PreparedPlan> retained;
    for (int order = 0; order < 2; ++order) {
        auto catalog = std::make_shared<Catalog>();
        OperationRegistry operations;
        const auto semantic = operations.describe("gr.bytes.identity@1");
        std::vector<std::string> ids = order ? std::vector<std::string>{"aaa.last@1", "aaa.first@1"}
                                             : std::vector<std::string>{"aaa.first@1", "aaa.last@1"};
        for (auto id : ids)
            catalog->register_implementation(
                {id,
                 semantic.id,
                 semantic.fingerprint(),
                 {"cpu.sequential"},
                 {"cpu"},
                 {"host"},
                 Determinism::Exact,
                 true},
                [](const Invocation &inputs, std::stop_token) { return inputs; });
        catalog->seal();
        Planner planner(catalog);
        auto graph = single();
        const auto result = planner.compile(graph);
        check(result->nodes()[0].selection["implementation_id"] == "aaa.first@1",
              "Stable tie break independent of registration order");
        graph.nodes[0].selection.prefer_implementations = {"aaa.last@1"};
        retained = planner.compile(graph);
        check(retained->nodes()[0].selection["implementation_id"] == "aaa.last@1",
              "Explicit preference controls compatible choice");
    }
    check(evaluate(*retained) == Invocation{bytes("a")},
          "Prepared plan survives registry/planner/catalog destruction");
    auto missing = std::make_shared<Catalog>(false);
    missing->register_module({"test.module@1", {"test.dependency@2"}});
    fails("module_dependency", [&] { missing->seal(); });
    missing->register_module({"test.dependency@2", {}});
    missing->seal();
    auto cycle = std::make_shared<Catalog>(false);
    cycle->register_module({"test.a@1", {"test.b@1"}});
    cycle->register_module({"test.b@1", {"test.a@1"}});
    fails("module_cycle", [&] { cycle->seal(); });
}
void record_operation() {
    auto catalog = std::make_shared<Catalog>();
    catalog->seal();
    Planner planner(catalog);
    auto record = core::construct({{"format", "gr.record/1"},
                                   {"id", "fixture"},
                                   {"type", "gr.entity@1"},
                                   {"payload", Json::object()},
                                   {"attribution", {{"agent", "fixture:author"}, {"method", "reported"}}}});
    Graph graph{{{"verify", "gr.record.verify@1", {Value::json("gr.json@1", record)}}}, {{"verify", 0}}};
    auto result = evaluate(*planner.compile(graph));
    check(result[0].as_json() == core::verify(record),
          "Runtime instruction uses actual kernel verification semantics");
    record["document"]["payload"]["changed"] = true;
    graph.nodes[0].inputs[0] = Value::json("gr.json@1", record);
    result = evaluate(*planner.compile(graph));
    check(!result[0].as_json()["valid"].get<bool>(),
          "Invalid record is a qualified verification result, not runtime success-as-truth");
}
class ExtensionBackend final : public execution::ExecutionBackend {
  public:
    std::string id() const override { return "fixture.custom-host"; }
    Json capabilities() const override {
        return {{"backend", id()}, {"version", "1"}, {"scope", "test host adapter"}};
    }
    std::vector<execution::ExecutionTarget> targets() const override { return {{"cpu:0", "cpu", "host"}}; }
    std::shared_ptr<const execution::PreparedExecutor>
    prepare(const ValueTypeRegistry &types, const OperationRegistry &operations,
            const execution::ImplementationRegistry &implementations, std::string_view operation,
            std::string_view implementation, const execution::ExecutionTarget &target) const override {
        return execution::prepare_host_execution(types, operations, implementations, operation,
                                                 implementation, id(), target);
    }
    execution::BatchResult execute(const ValueTypeRegistry &, const OperationRegistry &,
                                   const execution::ImplementationRegistry &, std::string_view,
                                   std::string_view, const execution::Batch &,
                                   const execution::ExecutionOptions &) const override {
        throw Error("unsupported_batch", "This independent fixture implements prepared calls only.");
    }
};
void independent_backend() {
    auto catalog = std::make_shared<Catalog>();
    auto backend = std::make_shared<ExtensionBackend>();
    std::weak_ptr<ExtensionBackend> lifetime = backend;
    catalog->register_backend(backend);
    OperationRegistry operations;
    auto semantic = operations.describe("gr.bytes.identity@1");
    auto calls = std::make_shared<int>(0);
    catalog->register_implementation({"fixture.custom-provider@1",
                                      semantic.id,
                                      semantic.fingerprint(),
                                      {backend->id()},
                                      {"cpu"},
                                      {"host"},
                                      Determinism::Exact,
                                      true},
                                     [calls](const Invocation &input, std::stop_token) {
                                         ++*calls;
                                         return input;
                                     });
    catalog->seal();
    std::shared_ptr<const PreparedPlan> retained;
    {
        Planner planner(catalog);
        auto graph = single();
        graph.nodes[0].selection.backend = backend->id();
        auto malformed = graph;
        malformed.nodes.push_back({"bad", "future.missing@1", {}});
        fails("unsupported_operation", [&] { planner.compile(malformed); });
        check(*calls == 0, "Whole graph rejected before any native physical invocation");
        retained = planner.compile(graph);
        check(evaluate(*retained) == Invocation{bytes("a")} && *calls == 1,
              "Independent prepared backend works without planner dispatch edits");
        auto changed = semantic;
        changed.id = "fixture.external@1";
        changed.effect = Effect::External;
        fails("catalog_sealed", [&] { catalog->register_operation(changed); });
    }
    backend.reset();
    catalog.reset();
    check(!lifetime.expired(), "Prepared plan retains backend lifetime");
    check(evaluate(*retained) == Invocation{bytes("a")},
          "Independent backend remains executable after catalog destruction");
    retained.reset();
    check(lifetime.expired(), "Backend releases after final prepared plan reference");
}
} // namespace
int main() {
    try {
        graphs_and_cache();
        selection_and_lifetime();
        record_operation();
        independent_backend();
        std::cout << "planner: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "planner failure: " << error.what() << '\n';
        return 1;
    }
}
