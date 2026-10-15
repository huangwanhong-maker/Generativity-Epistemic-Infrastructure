#include "gr/runtime/catalog.hpp"
#include "gr/execution/cpu.hpp"
#include "gr/kernel/record_operations.hpp"
#include "gr/kernel/records.hpp"
#include <algorithm>
#include <mutex>
#include <set>
#include <tuple>

namespace gr::runtime {
namespace {
void need(bool condition, const char *code, const char *message) {
    if (!condition)
        throw Error(code, message);
}
bool has(const std::vector<std::string> &values, const std::string &value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
Json module_json(const ModuleDescriptor &module) {
    return {{"id", module.id}, {"dependencies", module.dependencies}, {"metadata", module.metadata}};
}
class RetainedExecutor final : public execution::PreparedExecutor {
    std::shared_ptr<const execution::ExecutionBackend> backend_;
    std::shared_ptr<const execution::PreparedExecutor> executor_;

  public:
    RetainedExecutor(std::shared_ptr<const execution::ExecutionBackend> backend,
                     std::shared_ptr<const execution::PreparedExecutor> executor)
        : backend_(std::move(backend)), executor_(std::move(executor)) {}
    kernel::Invocation invoke(const kernel::Invocation &inputs, std::stop_token stop, std::size_t input_limit,
                              std::size_t output_limit) const override {
        return executor_->invoke(inputs, stop, input_limit, output_limit);
    }
    std::size_t cpu_slots() const override { return executor_->cpu_slots(); }
    Json description() const override { return executor_->description(); }
};
} // namespace
struct Catalog::State {
    mutable std::mutex mutex;
    bool sealed = false;
    kernel::ValueTypeRegistry types;
    kernel::OperationRegistry operations;
    execution::ImplementationRegistry implementations;
    struct Backend {
        std::shared_ptr<const execution::ExecutionBackend> object;
        Json capabilities;
        std::vector<execution::ExecutionTarget> targets;
    };
    std::map<std::string, Backend> backends;
    std::map<std::string, ModuleDescriptor> modules;
    explicit State(bool builtins) : types(builtins), operations(builtins) {}
    void writable() const { need(!sealed, "catalog_sealed", "Catalog is permanently sealed."); }
    void readable() const {
        need(sealed, "catalog_unsealed", "Seal the catalog before planning or execution.");
    }
};
Catalog::Catalog(bool builtins) : state_(std::make_unique<State>(builtins)) {
    if (!builtins)
        return;
    execution::install_cpu_implementations(state_->implementations, state_->operations);
    kernel::install_record_operations(state_->types, state_->operations);
    const auto operation = state_->operations.describe("gr.record.verify@1");
    // Prepared callbacks own their semantic context; process-global registry
    // destruction must not invalidate a still-retained runtime plan.
    const auto record_types = std::make_shared<const core::TypeRegistry>();
    state_->implementations.register_implementation(
        {"gr.cpu.record.verify@1",
         operation.id,
         operation.fingerprint(),
         {"cpu.sequential", "cpu.threaded"},
         {"cpu"},
         {"host"},
         kernel::Determinism::Exact,
         true,
         {{"reference_behavior", "gr::core::verify"}}},
        [record_types](const kernel::Invocation &inputs, std::stop_token stop) {
            if (stop.stop_requested())
                throw Error("execution_cancelled", "Record verification was cancelled.");
            return kernel::Invocation{kernel::Value::json("gr.record-verification@1",
                                                          core::verify(inputs[0].as_json(), *record_types))};
        });
    register_backend(std::make_shared<execution::SequentialBackend>());
    register_backend(std::make_shared<execution::ThreadedCpuBackend>());
    register_module({"gr.foundation@0.3", {}, {{"loading", "statically linked trusted native code"}}});
}
Catalog::~Catalog() = default;
void Catalog::register_type(kernel::ValueTypeDescriptor descriptor,
                            kernel::ValueTypeRegistry::Validator validator) {
    std::lock_guard lock(state_->mutex);
    state_->writable();
    state_->types.register_type(std::move(descriptor), std::move(validator));
}
void Catalog::register_operation(kernel::OperationDescriptor descriptor,
                                 kernel::OperationRegistry::InputValidator input,
                                 kernel::OperationRegistry::OutputValidator output) {
    std::lock_guard lock(state_->mutex);
    state_->writable();
    state_->operations.register_operation(std::move(descriptor), std::move(input), std::move(output));
}
void Catalog::register_implementation(execution::ImplementationDescriptor descriptor,
                                      execution::ImplementationFunction function) {
    std::lock_guard lock(state_->mutex);
    state_->writable();
    state_->implementations.register_implementation(std::move(descriptor), std::move(function));
}
void Catalog::register_backend(std::shared_ptr<const execution::ExecutionBackend> backend) {
    need(bool(backend), "backend_descriptor", "Backend cannot be null.");
    const auto id = backend->id();
    need(!id.empty() && id.size() <= 256 && id.find('\0') == std::string::npos, "backend_descriptor",
         "Invalid backend identity.");
    const auto capabilities = backend->capabilities();
    canonical(capabilities);
    std::set<std::string> devices;
    const auto targets = backend->targets();
    need(!targets.empty() && targets.size() <= 256, "backend_descriptor",
         "Backend must expose bounded prepared targets.");
    for (const auto &target : targets) {
        for (const auto *field : {&target.device, &target.kind, &target.memory, &target.binding})
            need(!field->empty() && field->size() <= 256 && field->find('\0') == std::string::npos,
                 "backend_descriptor", "Invalid execution target description.");
        need(devices.insert(target.device).second, "backend_descriptor",
             "Backend repeats a device identity.");
    }
    std::lock_guard lock(state_->mutex);
    state_->writable();
    need(state_->backends.size() < 256, "catalog_limit", "Too many backends.");
    need(state_->backends.emplace(id, State::Backend{std::move(backend), capabilities, targets}).second,
         "duplicate_backend", "Backend is already registered.");
}
void Catalog::register_module(ModuleDescriptor module) {
    kernel::validate_qualified_id(module.id);
    need(module.dependencies.size() <= 256 && module.metadata.is_object(), "module_descriptor",
         "Invalid module descriptor.");
    std::set<std::string> distinct;
    for (const auto &dependency : module.dependencies) {
        kernel::validate_qualified_id(dependency);
        need(dependency != module.id && distinct.insert(dependency).second, "module_descriptor",
             "Repeated or self module dependency.");
    }
    canonical(module_json(module));
    std::lock_guard lock(state_->mutex);
    state_->writable();
    need(state_->modules.size() < 256, "catalog_limit", "Too many modules.");
    const auto id = module.id;
    need(state_->modules.emplace(id, std::move(module)).second, "duplicate_module",
         "Module is already registered.");
}
void Catalog::pin_types(kernel::OperationDescriptor &descriptor) const {
    std::lock_guard lock(state_->mutex);
    kernel::pin_types(descriptor, state_->types);
}
void Catalog::seal() {
    std::lock_guard lock(state_->mutex);
    if (state_->sealed)
        return;
    std::map<std::string, unsigned> color;
    std::function<void(const std::string &)> visit = [&](const std::string &id) {
        need(state_->modules.contains(id), "module_dependency", "Exact module dependency is unavailable.");
        need(color[id] != 1, "module_cycle", "Module dependency cycle.");
        if (color[id] == 2)
            return;
        color[id] = 1;
        for (const auto &dependency : state_->modules.at(id).dependencies)
            visit(dependency);
        color[id] = 2;
    };
    for (const auto &[id, module] : state_->modules) {
        (void)module;
        visit(id);
    }
    for (const auto &item : state_->operations.capabilities())
        state_->operations.validate_types(item.at("id").get<std::string>(), state_->types);
    state_->sealed = true;
}
bool Catalog::sealed() const {
    std::lock_guard lock(state_->mutex);
    return state_->sealed;
}
Json Catalog::capabilities() const {
    std::lock_guard lock(state_->mutex);
    Json modules = Json::array(), backends = Json::array();
    for (const auto &[id, module] : state_->modules) {
        (void)id;
        modules.push_back(module_json(module));
    }
    for (const auto &[id, backend] : state_->backends) {
        (void)id;
        backends.push_back(backend.capabilities);
    }
    return {{"sealed", state_->sealed},
            {"types", state_->types.capabilities()},
            {"operations", state_->operations.capabilities()},
            {"implementations", state_->implementations.capabilities()},
            {"backends", backends},
            {"modules", modules},
            {"extension_trust", "trusted static native code; no dynamic loading or authority grant"}};
}
kernel::OperationDescriptor Catalog::operation(std::string_view id) const {
    {
        std::lock_guard lock(state_->mutex);
        state_->readable();
    }
    return state_->operations.describe(id);
}
void Catalog::validate(const kernel::Value &value) const {
    {
        std::lock_guard lock(state_->mutex);
        state_->readable();
    }
    state_->types.validate(value);
}
SelectedExecution Catalog::prepare(std::string_view id, const Selection &selection) const {
    {
        std::lock_guard lock(state_->mutex);
        state_->readable();
    }
    const auto operation = state_->operations.prepare(id, state_->types);
    need(operation.descriptor().effect == kernel::Effect::Pure, "unsupported_effect",
         "Runtime graph nodes must be pure; publication uses the transaction coordinator.");
    need(operation.descriptor().determinism == kernel::Determinism::Exact, "unsupported_determinism",
         "Runtime graph requires exact operations.");
    need(selection.prefer_implementations.size() <= 256, "selection", "Too many provider preferences.");
    struct Candidate {
        execution::ImplementationDescriptor provider;
        std::shared_ptr<const execution::ExecutionBackend> backend;
        std::string backend_id;
        Json capabilities;
        execution::ExecutionTarget target;
        std::size_t rank;
    };
    std::vector<Candidate> candidates;
    Json rejected = Json::array();
    for (const auto &provider : state_->implementations.descriptors()) {
        if (provider.operation != id)
            continue;
        if (!selection.implementation.empty() && selection.implementation != provider.id)
            continue;
        if (provider.contract_fingerprint != operation.fingerprint() ||
            provider.determinism != operation.descriptor().determinism) {
            rejected.push_back({{"implementation", provider.id}, {"reason", "semantic_contract"}});
            continue;
        }
        for (const auto &[backend_id, backend] : state_->backends) {
            if (!selection.backend.empty() && backend_id != selection.backend)
                continue;
            if (!has(provider.backend_ids, backend_id))
                continue;
            for (const auto &target : backend.targets) {
                if ((!selection.device.empty() && target.device != selection.device) ||
                    !has(provider.device_kinds, target.kind) ||
                    !has(provider.memory_domains, target.memory) || target.binding != "gr.host-values/1")
                    continue;
                const auto preferred = std::find(selection.prefer_implementations.begin(),
                                                 selection.prefer_implementations.end(), provider.id);
                candidates.push_back(
                    {provider, backend.object, backend_id, backend.capabilities, target,
                     static_cast<std::size_t>(preferred - selection.prefer_implementations.begin())});
            }
        }
    }
    if (candidates.empty())
        throw Error("no_compatible_implementation",
                    "No provider/backend/target satisfies the exact requested contract.",
                    {{"operation", id}, {"rejected", rejected}});
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return std::tuple(a.rank, a.provider.id, a.backend_id, a.target.device) <
               std::tuple(b.rank, b.provider.id, b.backend_id, b.target.device);
    });
    const auto &chosen = candidates.front();
    auto executor = chosen.backend->prepare(state_->types, state_->operations, state_->implementations, id,
                                            chosen.provider.id, chosen.target);
    need(bool(executor) && executor->cpu_slots() > 0, "backend_contract",
         "Prepared executor requires a positive declared CPU slot reservation.");
    Json description = executor->description();
    need(description.is_object(), "backend_contract", "Prepared description must be an object.");
    description["operation"] = operation.descriptor().id;
    description["contract_fingerprint"] = operation.fingerprint();
    description["implementation_id"] = chosen.provider.id;
    description["implementation"] = chosen.provider.to_json();
    description["backend"] = chosen.backend_id;
    description["backend_capabilities"] = chosen.capabilities;
    description["device"] = chosen.target.device;
    description["cpu_slots"] = executor->cpu_slots();
    canonical(description);
    description["selection_policy"] =
        "explicit constraints, ordered provider preference, stable lexical tie-break; no execution fallback";
    description["compatible_candidates"] = candidates.size();
    return {std::make_shared<RetainedExecutor>(chosen.backend, std::move(executor)), std::move(description)};
}
} // namespace gr::runtime
