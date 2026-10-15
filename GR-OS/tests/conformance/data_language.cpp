#include "gr/data/language.hpp"
#include <atomic>
#include <iostream>
#include <thread>
using namespace gr;
using namespace gr::data;
namespace {
int checks = 0;
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
template <class F> void fails(const char *code, F fn) {
    try {
        fn();
    } catch (const Error &e) {
        if (e.code != code)
            throw std::runtime_error(std::string("expected ") + code + ", got " + e.code + ": " + e.what());
        ++checks;
        return;
    }
    throw std::runtime_error(std::string("expected failure ") + code);
}
Type number() {
    return {"test.number@1"};
}
ValueSpec spec() {
    return {number(), "test.json-number@1"};
}
DataValue value(int n) {
    return {spec(), kernel::Value::json("test.payload@1", n), {{"source", "test"}}};
}
std::shared_ptr<Library> make_library(std::shared_ptr<std::atomic<int>> invoked,
                                      std::string meaning = "Bounded exact natural number",
                                      std::shared_ptr<std::atomic<int>> specialized = {}) {
    auto library = std::make_shared<Library>();
    TypeFamily f;
    f.id = "test.number@1";
    f.meaning = std::move(meaning);
    f.validate = [](const Type &t) {
        if (!t.parameters.empty())
            throw Error("test_parameters", "No parameters.");
    };
    library->register_type(std::move(f));
    TypeFamily box;
    box.id = "test.box@1";
    box.meaning = "One parameterized child type";
    box.min_arguments = box.max_arguments = 1;
    box.validate = [](const Type &t) {
        if (!t.parameters.empty())
            throw Error("test_parameters", "No parameters.");
    };
    library->register_type(std::move(box));
    Representation r;
    r.id = "test.json-number@1";
    r.carrier = kernel::Representation::Json;
    r.meaning = "Exact JSON integer [0,1000]";
    r.supports = [](const Type &t) { return t.family == "test.number@1" || t.family == "test.box@1"; };
    r.validate = [](const Type &, const kernel::Value &v) {
        if (!v.as_json().is_number_integer() || v.as_json() < 0 || v.as_json() > 1000)
            throw Error("test_value", "Outside exact natural domain.");
    };
    library->register_representation(std::move(r));
    Representation b;
    b.id = "test.byte-number@1";
    b.carrier = kernel::Representation::Bytes;
    b.meaning = "One byte natural";
    b.supports = [](const Type &t) { return t == number(); };
    b.validate = [](const Type &, const kernel::Value &v) {
        if (v.as_bytes().size() != 1)
            throw Error("test_value", "Need one byte.");
    };
    library->register_representation(std::move(b));
    for (const std::string id : {"test.add@1", "test.bad@1", "test.pair@1", "test.unused@1"}) {
        Operator op;
        op.id = id;
        op.meaning = "Test exact natural arithmetic extension";
        op.infer = [id](const std::vector<ValueSpec> &in, const Json &attrs) {
            if (!attrs.empty())
                throw Error("test_attributes", "No attributes accepted.");
            if (in.size() != 2 || in[0] != spec() || in[1] != spec())
                throw Error("test_types", "Two JSON naturals required.");
            return std::vector<ValueSpec>(id == "test.pair@1" ? 2 : 1, spec());
        };
        op.specialize = [id, invoked, specialized](const auto &, const auto &, const Json &) {
            if (specialized)
                ++*specialized;
            Specialization s;
            for (const std::string provider_id : {"test.provider.a@1", "test.provider.b@1"}) {
                Provider p;
                p.descriptor.id = provider_id;
                p.descriptor.backend_ids = {"cpu.sequential", "cpu.threaded"};
                p.descriptor.device_kinds = {"cpu"};
                p.descriptor.memory_domains = {"host"};
                p.descriptor.thread_safe = true;
                p.function = [id, invoked](const kernel::Invocation &in, std::stop_token) {
                    ++*invoked;
                    if (id == "test.unused@1")
                        throw Error("test_unreachable", "Unused expression must never execute.");
                    if (id == "test.bad@1")
                        return kernel::Invocation{kernel::Value::json("test.payload@1", -1)};
                    const auto a = in[0].as_json().get<int>(), b = in[1].as_json().get<int>();
                    kernel::Invocation out{kernel::Value::json("test.payload@1", a + b)};
                    if (id == "test.pair@1")
                        out.push_back(kernel::Value::json("test.payload@1", a * b));
                    return out;
                };
                s.providers.push_back(std::move(p));
            }
            return s;
        };
        library->register_operator(std::move(op));
    }
    return library;
}
CallOptions options(std::string provider = "test.provider.a@1") {
    CallOptions o;
    o.selection.implementation = std::move(provider);
    return o;
}
int result(runtime::Runtime &runtime, const Program &program) {
    auto job = runtime.run(program.plan());
    check(job.status == runtime::JobStatus::succeeded, "Successful native execution");
    return program.materialize(job)[0].payload.as_json().get<int>();
}
std::shared_ptr<Library> cache_library(ValidationCacheLimits limits,
                                       std::shared_ptr<std::atomic<int>> validations) {
    auto library = std::make_shared<Library>(limits);
    TypeFamily family;
    family.id = "test.number@1";
    family.meaning = "Cache conformance bounded integer";
    family.validate = [](const Type &t) {
        if (!t.parameters.is_object() || t.parameters.size() > 1 ||
            (!t.parameters.empty() &&
             (!t.parameters.contains("maximum") || !t.parameters["maximum"].is_number_integer())))
            throw Error("test_parameters", "Expected optional integer maximum.");
    };
    library->register_type(std::move(family));
    Representation rep;
    rep.id = "test.json-number@1";
    rep.carrier = kernel::Representation::Json;
    rep.meaning = "Cache conformance JSON integer";
    rep.supports = [](const Type &t) { return t.family == "test.number@1"; };
    rep.validate = [validations](const Type &t, const kernel::Value &v) {
        ++*validations;
        if (v.type == "test.forbidden@1" || !v.as_json().is_number_integer() || v.as_json() < 0 ||
            v.as_json() > t.parameters.value("maximum", 1000))
            throw Error("test_value", "Invalid value or transport tag.");
    };
    library->register_representation(std::move(rep));
    library->seal();
    return library;
}
void cache_checks() {
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto library = cache_library({2, 4096, true}, calls);
    auto a = value(4), b = value(5), c = value(6);
    library->validate(a);
    library->validate(a);
    check(*calls == 1 && library->validation_statistics()["hits"] == 1,
          "Exact immutable owner is validated once");
    auto alias = a;
    alias.provenance = {{"new-source", "different qualifications"}};
    library->validate(alias);
    check(*calls == 1, "Independent valid provenance is still checked without rescanning payload");
    alias.provenance = Json::array();
    fails("data_descriptor", [&] { library->validate(alias); });
    alias = a;
    alias.payload.type = "test.forbidden@1";
    fails("test_value", [&] { library->validate(alias); });
    alias = a;
    alias.spec.type.parameters = {{"maximum", 3}};
    fails("test_value", [&] { library->validate(alias); });
    auto copied = value(4);
    const int before_copy = *calls;
    library->validate(copied);
    check(*calls == before_copy + 1, "Equal contents in a new owner require validation");
    library->validate(b);
    library->validate(c);
    const int before_eviction = *calls;
    library->validate(a);
    check(*calls == before_eviction + 1 && library->validation_statistics()["evictions"] != 0,
          "Bounded LRU eviction causes safe revalidation");
    const auto bad = value(-1);
    fails("test_value", [&] { library->validate(bad); });
    fails("test_value", [&] { library->validate(bad); });
    check(library->validation_statistics()["failures"] == 4, "Failed validations are never cached");
    auto other_calls = std::make_shared<std::atomic<int>>(0);
    cache_library({2, 4096}, other_calls)->validate(a);
    check(*other_calls == 1, "Another sealed library cannot reuse this certificate");
    auto disabled_calls = std::make_shared<std::atomic<int>>(0);
    auto disabled = cache_library({0, 4096}, disabled_calls);
    disabled->validate(a);
    disabled->validate(a);
    check(*disabled_calls == 2 && disabled->validation_statistics()["entries"] == 0,
          "Disabling reuse preserves validation behavior");
    auto tiny = cache_library({2, 1}, disabled_calls);
    tiny->validate(a);
    check(tiny->validation_statistics()["entries"] == 0, "Oversized entries are not retained");
    std::weak_ptr<const void> owner;
    {
        const auto ephemeral = value(19);
        owner = ephemeral.payload.owner_token();
        library->validate(ephemeral);
    }
    check(owner.expired(), "Validation cache does not extend payload lifetime");
    check(library->validation_statistics()["expired"] != 0, "Expired owners are pruned safely");
    library->clear_validation_cache();
    std::atomic<int> failures = 0;
    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < 4; ++i)
            threads.emplace_back([&] {
                try {
                    for (int j = 0; j < 64; ++j) {
                        library->validate(a);
                        if (j % 8 == 0)
                            library->clear_validation_cache();
                    }
                } catch (...) {
                    ++failures;
                }
            });
    }
    check(failures == 0 && library->validation_statistics()["entries"].get<int>() <= 2,
          "Concurrent validation and clearing preserve correctness and bounds");
    library->clear_validation_cache();
    check(library->validation_statistics()["charged_bytes"] == 0, "Clear releases all cache charges");
}
} // namespace
int main() {
    try {
        cache_checks();
        auto invoked = std::make_shared<std::atomic<int>>(0);
        auto library = make_library(invoked);
        fails("data_library_unsealed", [&] { literal(library, value(1)); });
        library->seal();
        check(library->capabilities()["operators"].size() == 4, "Discover independent logical operators");
        fails("data_library_sealed", [&] {
            TypeFamily f;
            f.id = "test.extra@1";
            f.meaning = "x";
            f.validate = [](const Type &) {};
            library->register_type(f);
        });
        check(Type::from_json(number().to_json()) == number(), "Type round trip");
        check(ValueSpec::from_json(spec().to_json()) == spec(), "ValueSpec round trip");
        auto invalid = number().to_json();
        invalid["typo"] = 1;
        fails("data_descriptor", [&] { Type::from_json(invalid); });
        fails("data_type_arity", [&] { library->validate(Type{"test.box@1"}); });
        fails("data_unknown_type", [&] { library->validate(Type{"test.unknown@1"}); });
        auto params = number();
        params.parameters["units"] = "m";
        fails("test_parameters", [&] { library->validate(params); });
        auto bad_carrier = value(1);
        bad_carrier.payload = kernel::Value::bytes("test.payload@1", {1});
        fails("data_carrier", [&] { literal(library, bad_carrier); });
        fails("test_value", [&] { literal(library, value(-1)); });
        auto deep = number();
        for (int i = 0; i < 34; ++i)
            deep = Type{"test.box@1", {std::move(deep)}};
        fails("data_type_limit", [&] { deep.to_json(); });
        auto other = make_library(invoked, "Different semantic definition");
        other->seal();
        check(library->binding_fingerprint(spec()) != other->binding_fingerprint(spec()),
              "Family semantics pinned despite identical nominal ID");
        ValueSpec nested{{"test.box@1", {number()}}, "test.json-number@1"};
        check(library->binding_fingerprint(nested) != other->binding_fingerprint(nested),
              "Child family semantics recursively pinned");
        auto bytes_spec = ValueSpec{number(), "test.byte-number@1"};
        check(library->binding_fingerprint(spec()) != library->binding_fingerprint(bytes_spec),
              "Representation separate and pinned");
        auto a = literal(library, value(3));
        auto b = literal(library, value(4));
        fails("data_expression_library",
              [&] { apply(library, "test.add@1", {a, literal(other, value(1))}); });
        fails("test_types", [&] { apply(library, "test.add@1", {a}); });
        fails("test_attributes", [&] { apply(library, "test.add@1", {a, b}, {{"unknown", true}}); });
        auto bytes = literal(library, {bytes_spec, kernel::Value::bytes("test.payload@1", {3})});
        fails("test_types", [&] { apply(library, "test.add@1", {a, bytes}); });
        auto shared = apply(library, "test.add@1", {a, b}, Json::object(), options());
        auto unused = apply(library, "test.unused@1", {a, b}, Json::object(), options());
        auto total = apply(library, "test.add@1", {shared, shared}, Json::object(), options());
        check(invoked->load() == 0, "Composition is lazy");
        auto p = compile({total});
        check(p.plan()->nodes().size() == 2, "Explicit sharing lowers once and unused branch is absent");
        check(p.plan()->id() == compile({total}).plan()->id(), "Stable concrete bindings/plan identity");
        runtime::Runtime runtime({.workers = 2});
        check(result(runtime, p) == 14 && invoked->load() == 2, "Shared result reused by both operands");
        auto completed = runtime.run(p.plan());
        auto materialized = p.materialize(completed);
        check(materialized[0].provenance["recipe"]["nodes"].size() == 4,
              "Lineage retains source literals and transformations");
        auto differently_qualified = value(3);
        differently_qualified.provenance["source"] = "another-observer";
        auto other_source = compile({literal(library, differently_qualified)});
        auto original_source = compile({a});
        check(other_source.plan()->id() != original_source.plan()->id(),
              "Provenance-only source differences pin distinct plans");
        fails("data_result_plan", [&] { other_source.materialize(runtime.run(original_source.plan())); });
        auto oversized_lineage = value(3);
        const auto base_recipe_bytes = canonical(original_source.describe().at("recipe")).size();
        oversized_lineage.provenance["source"] = std::string(1024 * 1024 - 16 - base_recipe_bytes + 4, 'x');
        auto near_limit_literal = literal(library, std::move(oversized_lineage));
        fails("value_limit", [&] { compile({near_limit_literal}); });
        auto literal_program = compile({a, a});
        check(literal_program.plan()->nodes().size() == 1, "Repeated literal output has one identity node");
        auto literal_result = runtime.run(literal_program.plan());
        check(literal_program.materialize(literal_result).size() == 2,
              "Literal-only program uses normal runtime");
        runtime::PlannerLimits lineage_limit;
        lineage_limit.max_plan_bytes =
            canonical(literal_program.materialize(literal_result)[0].provenance).size();
        fails("data_lineage_limit", [&] { compile({a, a}, {}, lineage_limit); });
        fails("data_result_plan", [&] { p.materialize(literal_result); });
        auto failed_result = completed;
        failed_result.status = runtime::JobStatus::failed;
        fails("data_result_status", [&] { p.materialize(failed_result); });
        auto forged = completed;
        forged.outputs[0].type = "test.payload@1";
        fails("data_result_type", [&] { p.materialize(forged); });
        auto pair = call(library, "test.pair@1", {a, b}, Json::object(), options());
        auto pair_program = compile({pair[1], pair[0]});
        auto pair_values = pair_program.materialize(runtime.run(pair_program.plan()));
        check(pair_values[0].payload.as_json() == 12 && pair_values[1].payload.as_json() == 7,
              "Multiple outputs preserve selected order");
        fails("data_output_arity", [&] { apply(library, "test.pair@1", {a, b}); });
        auto x = input(library, "x", spec());
        auto template_expr =
            apply(library, "test.add@1", {x, a}, Json::object(), options("test.provider.b@1"));
        auto bound = compile({template_expr}, {{"x", value(8)}});
        check(result(runtime, bound) == 11, "Named template binding and alternate physical provider");
        check(result(runtime, compile({template_expr}, {{"x", value(9)}})) == 12,
              "Rebind another immutable chunk");
        fails("data_input_missing", [&] { compile({template_expr}); });
        fails("data_input_unused", [&] { compile({a}, {{"x", value(1)}}); });
        fails("data_input_type", [&] {
            compile({template_expr}, {{"x", {bytes_spec, kernel::Value::bytes("test.payload@1", {8})}}});
        });
        fails("data_selection", [&] {
            compile({apply(library, "test.add@1", {a, b}, Json::object(), options("test.absent@1"))});
        });
        runtime::PlannerLimits tiny;
        tiny.max_nodes = 2;
        fails("data_expression_limit", [&] { compile({total}, {}, tiny); });
        auto bad = compile({apply(library, "test.bad@1", {a, b}, Json::object(), options())});
        auto bad_result = runtime.run(bad.plan());
        check(bad_result.status == runtime::JobStatus::failed && bad_result.error["code"] == "test_value",
              "Provider output independently validates representation");
        runtime::RuntimeConfig denied;
        denied.policy.allow_all_pure = false;
        runtime::Runtime restricted(denied);
        bool rejected = false;
        try {
            auto r = restricted.run(p.plan());
            rejected = r.status == runtime::JobStatus::failed;
        } catch (const Error &) {
            rejected = true;
        }
        check(rejected, "Logical names cannot bypass exact runtime policy");
        runtime::RuntimeConfig allowed;
        allowed.policy.allow_all_pure = false;
        for (const auto &n : p.plan()->nodes())
            allowed.policy.allowed_operations.insert(n.operation);
        runtime::Runtime permitted(allowed);
        check(result(permitted, p) == 14, "Explicit lowered-operation allowlist works");
        // Programs own all callbacks/context after every frontend lease is gone.
        Program retained;
        {
            auto temporary = make_library(invoked);
            temporary->seal();
            retained = compile(
                {apply(temporary, "test.add@1", {literal(temporary, value(1)), literal(temporary, value(2))},
                       Json::object(), options())});
        }
        check(result(runtime, retained) == 3, "Program owns library/validators/provider lifetimes");
        auto specializations = std::make_shared<std::atomic<int>>(0);
        auto functions = make_library(invoked, "Function conformance", specializations);
        functions->seal();
        auto arg = input(functions, "arg", spec());
        auto one = literal(functions, value(1));
        auto increment = apply(functions, "test.add@1", {arg, one}, Json::object(), options());
        Function twice({apply(functions, "test.add@1", {increment, increment}, Json::object(), options())});
        check(twice.inputs().size() == 1 && twice.describe()["outputs"].size() == 1,
              "Function exposes exact named input and output contracts");
        check(result(runtime, twice.bind({{"arg", value(5)}})) == 12, "Function binds immutable values");
        check(result(runtime, twice.bind({{"arg", value(7)}})) == 16 && *specializations == 2,
              "Repeated binding reuses each immutable logical specialization");
        auto composed = twice.instantiate({{"arg", increment}});
        auto composed_program = Function(composed).bind({{"arg", value(2)}});
        check(result(runtime, composed_program) == 8 && composed_program.plan()->nodes().size() == 3,
              "Function substitution preserves explicit shared dependencies");
        fails("data_input_missing", [&] { twice.instantiate({}); });
        fails("data_input_unused", [&] { twice.instantiate({{"arg", one}, {"extra", one}}); });
        fails("data_expression_library", [&] { twice.instantiate({{"arg", a}}); });
        fails("data_input_type", [&] {
            auto wrong = input(functions, "wrong", {{"test.box@1", {number()}}, "test.json-number@1"});
            twice.instantiate({{"arg", wrong}});
        });
        fails("data_input_type", [&] {
            Function conflicting(
                {arg, input(functions, "arg", {{"test.box@1", {number()}}, "test.json-number@1"})});
        });
        check(result(runtime, Function({one}).bind()) == 1, "Closed function needs no parameters");
        auto concurrent_calls = std::make_shared<std::atomic<int>>(0);
        auto concurrent_library = make_library(invoked, "Concurrent specialization", concurrent_calls);
        concurrent_library->seal();
        auto parameter = input(concurrent_library, "x", spec());
        Function concurrent(
            {apply(concurrent_library, "test.add@1", {parameter, parameter}, Json::object(), options())});
        std::atomic<int> preparation_failures = 0;
        {
            std::vector<std::jthread> threads;
            for (int i = 0; i < 4; ++i)
                threads.emplace_back([&, i] {
                    try {
                        auto prepared = concurrent.bind({{"x", value(i)}});
                        if (prepared.plan()->nodes().size() != 1)
                            ++preparation_failures;
                    } catch (...) {
                        ++preparation_failures;
                    }
                });
        }
        check(preparation_failures == 0 && *concurrent_calls == 1,
              "Concurrent bindings specialize once while constructing independent plans");
        std::cout << "data language: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "data language failure: " << e.what() << '\n';
        return 1;
    }
}
