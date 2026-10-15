#include "gr/data/language.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <list>
#include <set>
#include <unordered_map>

namespace gr::data {
namespace {
void need(bool ok, const char *code, const char *message) {
    if (!ok)
        throw Error(code, message);
}
void fields(const Json &j, std::initializer_list<const char *> names) {
    need(j.is_object(), "data_descriptor", "Expected an object.");
    for (auto it = j.begin(); it != j.end(); ++it)
        need(std::any_of(names.begin(), names.end(), [&](const char *name) { return it.key() == name; }),
             "data_descriptor", "Unknown descriptor field.");
}
void metadata(const Json &j) {
    need(j.is_object(), "data_descriptor", "Metadata and parameters must be objects.");
    canonical(j);
}
Json encode_type(const Type &t, std::size_t depth, std::size_t &count) {
    need(depth <= 32 && ++count <= 4096 && t.arguments.size() <= 256, "data_type_limit",
         "Logical type exceeds nesting/argument limits.");
    kernel::validate_qualified_id(t.family);
    metadata(t.parameters);
    Json args = Json::array();
    for (const auto &a : t.arguments)
        args.push_back(encode_type(a, depth + 1, count));
    return {{"family", t.family}, {"arguments", std::move(args)}, {"parameters", t.parameters}};
}
Type decode_type(const Json &j, std::size_t depth, std::size_t &count) {
    need(depth <= 32 && ++count <= 4096, "data_type_limit", "Logical type is too complex.");
    fields(j, {"family", "arguments", "parameters"});
    need(j.contains("family") && j.at("family").is_string(), "data_descriptor", "Type needs a family.");
    Type t{j.at("family").get<std::string>()};
    if (j.contains("parameters"))
        t.parameters = j.at("parameters");
    if (j.contains("arguments")) {
        need(j.at("arguments").is_array() && j.at("arguments").size() <= 256, "data_type_limit",
             "Type arguments must be a bounded array.");
        for (const auto &a : j.at("arguments"))
            t.arguments.push_back(decode_type(a, depth + 1, count));
    }
    return t;
}
void name(std::string_view value) {
    need(!value.empty() && value.size() <= 256 && value.find('\0') == std::string_view::npos, "data_name",
         "Name must be nonempty and bounded, without NUL.");
}
Json selection_json(const runtime::Selection &s) {
    return {{"implementation", s.implementation},
            {"backend", s.backend},
            {"device", s.device},
            {"prefer_implementations", s.prefer_implementations}};
}
Json value_description(const DataValue &v, const Library &library) {
    return {{"spec", v.spec.to_json()},
            {"binding", library.binding_fingerprint(v.spec)},
            {"content", v.payload.content_digest()},
            {"provenance", v.provenance}};
}
Json result_provenance(const Json &recipe_id, const Json &recipe, const std::string &plan,
                       std::size_t output) {
    return {{"kind", "gr.data.derived@1"},
            {"recipe_id", recipe_id},
            {"plan", plan},
            {"output", output},
            {"recipe", recipe},
            {"authority", "computational lineage only"}};
}
} // namespace
Json Type::to_json() const {
    std::size_t count = 0;
    auto j = encode_type(*this, 0, count);
    canonical(j);
    return j;
}
Type Type::from_json(const Json &j) {
    canonical(j);
    std::size_t count = 0;
    auto t = decode_type(j, 0, count);
    t.to_json();
    return t;
}
std::string Type::fingerprint() const {
    return sha256(canonical(to_json()));
}
Json ValueSpec::to_json() const {
    kernel::validate_qualified_id(representation);
    return {{"type", type.to_json()}, {"representation", representation}};
}
ValueSpec ValueSpec::from_json(const Json &j) {
    fields(j, {"type", "representation"});
    need(j.contains("type") && j.contains("representation") && j.at("representation").is_string(),
         "data_descriptor", "Value specification needs type and representation.");
    ValueSpec s{Type::from_json(j.at("type")), j.at("representation").get<std::string>()};
    s.to_json();
    return s;
}
Json TypeFamily::describe() const {
    return {{"id", id},
            {"meaning", meaning},
            {"min_arguments", min_arguments},
            {"max_arguments", max_arguments},
            {"traits", traits},
            {"metadata", metadata}};
}
Json Representation::describe() const {
    return {
        {"id", id}, {"carrier", kernel::to_string(carrier)}, {"meaning", meaning}, {"metadata", metadata}};
}
Json Operator::describe() const {
    return {{"id", id},
            {"meaning", meaning},
            {"effect", kernel::to_string(effect)},
            {"determinism", kernel::to_string(determinism)},
            {"metadata", metadata},
            {"qualifications", qualifications}};
}
struct Library::Impl {
    struct ValidationEntry {
        std::weak_ptr<const void> owner;
        ValueSpec spec;
        std::string payload_tag;
        std::size_t charge;
    };
    ValidationCacheLimits cache_limits;
    mutable std::mutex cache_mutex;
    mutable std::list<ValidationEntry> validated;
    mutable std::size_t cache_bytes = 0;
    mutable std::uint64_t cache_hits = 0, cache_misses = 0, validation_failures = 0, cache_evictions = 0,
                          cache_expired = 0, validation_ns = 0;
    void prune() const {
        for (auto i = validated.begin(); i != validated.end();) {
            if (i->owner.expired()) {
                cache_bytes -= i->charge;
                i = validated.erase(i);
                ++cache_expired;
            } else
                ++i;
        }
    }
    mutable std::mutex mutex;
    std::atomic<bool> sealed = false;
    std::map<std::string, TypeFamily, std::less<>> families;
    std::map<std::string, Representation, std::less<>> representations;
    std::map<std::string, Operator, std::less<>> operators;
    std::vector<std::shared_ptr<const execution::ExecutionBackend>> backends;
    mutable std::map<std::string, std::shared_ptr<std::mutex>> provider_mutexes;
    void readable() const { need(sealed.load(), "data_library_unsealed", "Seal the library before use."); }
    void writable() const { need(!sealed.load(), "data_library_sealed", "Library is permanently sealed."); }
    const TypeFamily &family(std::string_view id) const {
        auto it = families.find(id);
        if (it == families.end())
            throw Error("data_unknown_type", "Unregistered logical type family.", {{"id", id}});
        return it->second;
    }
    const Representation &representation(std::string_view id) const {
        auto it = representations.find(id);
        if (it == representations.end())
            throw Error("data_unknown_representation", "Unregistered representation.", {{"id", id}});
        return it->second;
    }
    const Operator &operation(std::string_view id) const {
        auto it = operators.find(id);
        if (it == operators.end())
            throw Error("data_unknown_operator", "Unregistered logical operator.", {{"id", id}});
        return it->second;
    }
    void validate_type(const Type &t) const {
        const auto &f = family(t.family);
        need(t.arguments.size() >= f.min_arguments && t.arguments.size() <= f.max_arguments,
             "data_type_arity", "Logical type has the wrong argument count.");
        for (const auto &a : t.arguments)
            validate_type(a);
        f.validate(t);
    }
    Json pinned_type(const Type &t) const {
        Json args = Json::array();
        for (const auto &a : t.arguments)
            args.push_back(pinned_type(a));
        return {{"family", family(t.family).describe()}, {"parameters", t.parameters}, {"arguments", args}};
    }
};
Library::Library(ValidationCacheLimits limits) : impl_(std::make_unique<Impl>()) {
    need(limits.max_entries <= 65536, "data_cache_limits", "Validation cache exceeds 65536 entries.");
    impl_->cache_limits = limits;
}
Library::~Library() = default;
void Library::register_type(TypeFamily f) {
    kernel::validate_qualified_id(f.id);
    need(!f.meaning.empty() && bool(f.validate) && f.min_arguments <= f.max_arguments &&
             f.max_arguments <= 256,
         "data_descriptor", "Type family needs a meaning, bounded arity and validator.");
    metadata(f.traits);
    metadata(f.metadata);
    canonical(f.describe());
    std::lock_guard lock(impl_->mutex);
    impl_->writable();
    need(impl_->families.size() < 4096, "data_registry_limit", "Too many type families.");
    const auto id = f.id;
    need(impl_->families.emplace(id, std::move(f)).second, "data_duplicate_type", "Duplicate type family.");
}
void Library::register_representation(Representation r) {
    kernel::validate_qualified_id(r.id);
    need(!r.meaning.empty() && bool(r.supports) && bool(r.validate), "data_descriptor",
         "Representation needs meaning, compatibility predicate and validator.");
    metadata(r.metadata);
    canonical(r.describe());
    std::lock_guard lock(impl_->mutex);
    impl_->writable();
    need(impl_->representations.size() < 4096, "data_registry_limit", "Too many representations.");
    const auto id = r.id;
    need(impl_->representations.emplace(id, std::move(r)).second, "data_duplicate_representation",
         "Duplicate representation.");
}
void Library::register_operator(Operator op) {
    kernel::validate_qualified_id(op.id);
    need(!op.meaning.empty() && bool(op.infer) && bool(op.specialize) && op.qualifications.is_array(),
         "data_descriptor", "Operator needs meaning, inference, specialization and qualifications array.");
    metadata(op.metadata);
    canonical(op.describe());
    std::lock_guard lock(impl_->mutex);
    impl_->writable();
    need(impl_->operators.size() < 4096, "data_registry_limit", "Too many operators.");
    const auto id = op.id;
    need(impl_->operators.emplace(id, std::move(op)).second, "data_duplicate_operator",
         "Duplicate operator.");
}
void Library::register_backend(std::shared_ptr<const execution::ExecutionBackend> b) {
    need(bool(b), "data_descriptor", "Backend cannot be null.");
    need(b->id() != "cpu.sequential" && b->id() != "cpu.threaded", "duplicate_backend",
         "Standard CPU backends are already available in every language library.");
    std::lock_guard lock(impl_->mutex);
    impl_->writable();
    need(impl_->backends.size() < 256, "data_registry_limit", "Too many backends.");
    for (const auto &existing : impl_->backends)
        need(existing->id() != b->id(), "duplicate_backend", "Backend identity is already registered.");
    impl_->backends.push_back(std::move(b));
}
void Library::seal() {
    std::lock_guard lock(impl_->mutex);
    impl_->sealed.store(true);
}
bool Library::sealed() const {
    return impl_->sealed.load();
}
void Library::validate(const Type &t) const {
    impl_->readable();
    t.to_json();
    impl_->validate_type(t);
}
void Library::validate(const ValueSpec &s) const {
    validate(s.type);
    kernel::validate_qualified_id(s.representation);
    need(impl_->representation(s.representation).supports(s.type), "data_representation_type",
         "Representation does not support this logical type.");
}
void Library::validate(const DataValue &v) const {
    validate(v.spec);
    metadata(v.provenance);
    const auto &r = impl_->representation(v.spec.representation);
    need(v.payload.representation() == r.carrier, "data_carrier", "Payload uses the wrong carrier.");
    // This key retains a control block during the lookup. Cached weak tokens can
    // never turn address reuse into a hit, and never extend buffer-budget lifetime.
    const auto owner = v.payload.owner_token().lock();
    {
        std::lock_guard lock(impl_->cache_mutex);
        impl_->prune();
        for (auto i = impl_->validated.begin(); i != impl_->validated.end(); ++i) {
            if (i->owner.lock() == owner && i->spec == v.spec && i->payload_tag == v.payload.type) {
                ++impl_->cache_hits;
                impl_->validated.splice(impl_->validated.begin(), impl_->validated, i);
                return;
            }
        }
        ++impl_->cache_misses;
    }
    const auto start = impl_->cache_limits.profile_timing ? std::chrono::steady_clock::now()
                                                          : std::chrono::steady_clock::time_point{};
    std::size_t charge = 0;
    try {
        r.validate(v.spec.type, v.payload);
        const auto payload_size = v.payload.byte_size();
        if (impl_->cache_limits.max_entries && impl_->cache_limits.max_bytes) {
            const auto payload_bytes =
                r.carrier == kernel::Representation::Buffers ? v.payload.retained_bytes() : payload_size;
            const auto spec_bytes = canonical(v.spec.to_json()).size() + v.payload.type.size();
            if (payload_bytes <= impl_->cache_limits.max_bytes &&
                spec_bytes <= impl_->cache_limits.max_bytes - payload_bytes)
                charge = payload_bytes + spec_bytes;
        }
    } catch (...) {
        std::lock_guard lock(impl_->cache_mutex);
        ++impl_->validation_failures;
        throw;
    }
    std::lock_guard lock(impl_->cache_mutex);
    if (impl_->cache_limits.profile_timing)
        impl_->validation_ns +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start)
                .count();
    if (!impl_->cache_limits.max_entries || !charge)
        return;
    impl_->prune();
    for (const auto &e : impl_->validated)
        if (e.owner.lock() == owner && e.spec == v.spec && e.payload_tag == v.payload.type)
            return; // Concurrent successful validation already installed this entry.
    while (!impl_->validated.empty() && (impl_->validated.size() >= impl_->cache_limits.max_entries ||
                                         charge > impl_->cache_limits.max_bytes - impl_->cache_bytes)) {
        impl_->cache_bytes -= impl_->validated.back().charge;
        impl_->validated.pop_back();
        ++impl_->cache_evictions;
    }
    impl_->validated.push_front({owner, v.spec, v.payload.type, charge});
    impl_->cache_bytes += charge;
}
Json Library::validation_statistics() const {
    std::lock_guard lock(impl_->cache_mutex);
    impl_->prune();
    return {{"hits", impl_->cache_hits},
            {"misses", impl_->cache_misses},
            {"failures", impl_->validation_failures},
            {"evictions", impl_->cache_evictions},
            {"expired", impl_->cache_expired},
            {"entries", impl_->validated.size()},
            {"charged_bytes", impl_->cache_bytes},
            {"max_entries", impl_->cache_limits.max_entries},
            {"max_bytes", impl_->cache_limits.max_bytes},
            {"profile_timing", impl_->cache_limits.profile_timing},
            {"successful_validation_ns", impl_->validation_ns},
            {"ownership", "weak immutable payload; no payload retention"}};
}
void Library::clear_validation_cache() const {
    std::lock_guard lock(impl_->cache_mutex);
    impl_->validated.clear();
    impl_->cache_bytes = 0;
}
std::string Library::binding_fingerprint(const ValueSpec &s) const {
    validate(s);
    return sha256(canonical(Json{{"logical", impl_->pinned_type(s.type)},
                                 {"representation", impl_->representation(s.representation).describe()}}));
}
Json Library::capabilities() const {
    impl_->readable();
    Json j{{"format", "gr.data.library@1"},    {"types", Json::array()},
           {"representations", Json::array()}, {"operators", Json::array()},
           {"backends", Json::array()},        {"builtin_backends", {"cpu.sequential", "cpu.threaded"}}};
    for (const auto &[id, f] : impl_->families)
        j["types"].push_back(f.describe());
    for (const auto &[id, r] : impl_->representations)
        j["representations"].push_back(r.describe());
    for (const auto &[id, op] : impl_->operators)
        j["operators"].push_back(op.describe());
    for (const auto &b : impl_->backends)
        j["backends"].push_back(b->capabilities());
    return j;
}
struct Expr::Node {
    enum class Kind { Literal, Input, Call } kind;
    std::shared_ptr<const Library> library;
    std::vector<ValueSpec> specs;
    std::vector<Expr> inputs;
    std::optional<DataValue> value;
    std::string name;
    Json attributes = Json::object();
    Json literal_description;
    CallOptions options;
    mutable std::once_flag specialization_once;
    mutable Specialization specialization;
};
struct LanguageAccess {
    static const auto &node(const Expr &e) { return e.node_; }
    static auto output(const Expr &e) { return e.output_; }
    static Expr expression(std::shared_ptr<const Expr::Node> n, std::size_t i = 0) {
        return Expr(std::move(n), i);
    }
    static auto make_node() { return std::make_shared<Expr::Node>(); }
    static const auto &impl(const Library &l) { return *l.impl_; }
    using Node = Expr::Node;
};
Expr::Expr(std::shared_ptr<const Node> n, std::size_t i) : node_(std::move(n)), output_(i) {}
Expr::operator bool() const noexcept {
    return bool(node_);
}
const ValueSpec &Expr::spec() const {
    need(bool(node_), "data_expression", "Expression is empty.");
    return node_->specs.at(output_);
}
Json Expr::describe() const {
    const auto &s = spec();
    Json j{{"spec", s.to_json()}, {"output", output_}};
    if (node_->kind == Node::Kind::Literal) {
        j["kind"] = "literal";
        j["value"] = node_->literal_description;
    } else if (node_->kind == Node::Kind::Input) {
        j["kind"] = "input";
        j["name"] = node_->name;
    } else {
        j["kind"] = "call";
        j["operator"] = node_->name;
        j["attributes"] = node_->attributes;
    }
    return j;
}
Expr literal(std::shared_ptr<const Library> l, DataValue v) {
    need(bool(l), "data_library", "Library cannot be null.");
    l->validate(v);
    auto n = LanguageAccess::make_node();
    n->kind = LanguageAccess::Node::Kind::Literal;
    n->specs = {v.spec};
    n->literal_description = value_description(v, *l);
    n->library = std::move(l);
    n->value = std::move(v);
    return LanguageAccess::expression(std::move(n));
}
Expr input(std::shared_ptr<const Library> l, std::string id, ValueSpec s) {
    need(bool(l), "data_library", "Library cannot be null.");
    name(id);
    l->validate(s);
    auto n = LanguageAccess::make_node();
    n->kind = LanguageAccess::Node::Kind::Input;
    n->specs = {std::move(s)};
    n->library = std::move(l);
    n->name = std::move(id);
    return LanguageAccess::expression(std::move(n));
}
std::vector<Expr> call(std::shared_ptr<const Library> l, std::string_view id, std::vector<Expr> inputs,
                       Json attrs, CallOptions options) {
    need(bool(l) && l->sealed(), "data_library_unsealed", "A sealed library is required.");
    need(inputs.size() <= 256, "data_expression_limit", "Too many operator arguments.");
    metadata(attrs);
    need(options.selection.prefer_implementations.size() <= 256, "data_selection",
         "Too many preferred providers.");
    canonical(selection_json(options.selection));
    const auto &op = LanguageAccess::impl(*l).operation(id);
    need(op.effect == kernel::Effect::Pure && op.determinism == kernel::Determinism::Exact,
         "data_execution_contract", "This runtime executes only pure exact logical operators.");
    std::vector<ValueSpec> specs;
    for (const auto &e : inputs) {
        need(bool(e) && LanguageAccess::node(e)->library == l, "data_expression_library",
             "Expressions must belong to the same sealed library.");
        specs.push_back(e.spec());
    }
    std::vector<ValueSpec> outputs;
    try {
        outputs = op.infer(specs, attrs);
    } catch (const Error &e) {
        auto d = e.details;
        d["logical_operator"] = std::string(id);
        throw Error(e.code, e.what(), d);
    }
    need(!outputs.empty() && outputs.size() <= 256, "data_expression_limit",
         "Operator output arity is invalid.");
    for (const auto &s : outputs)
        l->validate(s);
    auto n = LanguageAccess::make_node();
    n->kind = LanguageAccess::Node::Kind::Call;
    n->library = std::move(l);
    n->specs = std::move(outputs);
    n->inputs = std::move(inputs);
    n->name = std::string(id);
    n->attributes = std::move(attrs);
    n->options = std::move(options);
    std::vector<Expr> result;
    for (std::size_t i = 0; i < n->specs.size(); ++i)
        result.push_back(LanguageAccess::expression(n, i));
    return result;
}
Expr apply(std::shared_ptr<const Library> l, std::string_view id, std::vector<Expr> inputs, Json attrs,
           CallOptions options) {
    auto outputs = call(std::move(l), id, std::move(inputs), std::move(attrs), std::move(options));
    need(outputs.size() == 1, "data_output_arity", "apply requires a single-output operator; use call.");
    return outputs.front();
}
Program compile(const std::vector<Expr> &roots, const Bindings &bindings, runtime::PlannerLimits limits) {
    using Node = LanguageAccess::Node;
    need(!roots.empty() && roots.size() <= 4096 && bool(roots.front()), "data_expression_limit",
         "Compilation needs bounded nonempty expression roots.");
    const auto library = LanguageAccess::node(roots.front())->library;
    const auto &registry = LanguageAccess::impl(*library);
    std::vector<std::shared_ptr<const Node>> order;
    std::set<const Node *> done, visiting;
    std::vector<std::pair<std::shared_ptr<const Node>, bool>> stack;
    for (const auto &root : roots) {
        need(bool(root) && LanguageAccess::node(root)->library == library, "data_expression_library",
             "All roots must belong to the same library.");
        stack.emplace_back(LanguageAccess::node(root), false);
        while (!stack.empty()) {
            auto [n, expanded] = std::move(stack.back());
            stack.pop_back();
            if (done.contains(n.get()))
                continue;
            if (expanded) {
                visiting.erase(n.get());
                done.insert(n.get());
                order.push_back(n);
                continue;
            }
            need(visiting.insert(n.get()).second, "data_cycle", "Expression dependency cycle.");
            need(done.size() + visiting.size() <= limits.max_nodes, "data_expression_limit",
                 "Too many reachable expressions.");
            stack.emplace_back(n, true);
            for (auto it = n->inputs.rbegin(); it != n->inputs.rend(); ++it)
                stack.emplace_back(LanguageAccess::node(*it), false);
        }
    }
    auto catalog = std::make_shared<runtime::Catalog>();
    for (const auto &b : registry.backends)
        catalog->register_backend(b);
    std::map<std::string, std::string> type_ids;
    auto bind_type = [&](const ValueSpec &spec) {
        const auto hash = library->binding_fingerprint(spec);
        const auto id = "gr.data.bound." + hash + "@1";
        if (!type_ids.contains(hash)) {
            const auto &rep = registry.representation(spec.representation);
            catalog->register_type(
                {id, rep.carrier, "Layer 3 binding " + hash, Json::object()},
                [library, spec](const kernel::Value &value) { library->validate(DataValue{spec, value}); });
            type_ids.emplace(hash, id);
        }
        return id;
    };
    auto bound_value = [&](const DataValue &v) {
        auto payload = v.payload;
        payload.type = bind_type(v.spec);
        return payload;
    };
    struct Lowered {
        std::vector<runtime::Operand> outputs;
        std::string id;
    };
    std::map<const Node *, Lowered> lowered;
    struct BoundOperation {
        std::string id;
        std::map<std::string, std::string> providers;
    };
    std::map<std::string, BoundOperation> operations;
    runtime::Graph graph;
    Json recipe{{"format", "gr.data.recipe@1"}, {"nodes", Json::array()}, {"outputs", Json::array()}};
    std::set<std::string> used_bindings;
    for (const auto &n : order) {
        Lowered current;
        current.id = "n" + std::to_string(lowered.size());
        Json record{{"id", current.id}};
        if (n->kind != Node::Kind::Call) {
            const DataValue *v = nullptr;
            if (n->kind == Node::Kind::Literal) {
                v = &*n->value;
                record["kind"] = "literal";
                record["value"] = n->literal_description;
            } else {
                auto it = bindings.find(n->name);
                if (it == bindings.end())
                    throw Error("data_input_missing", "Named input was not bound.", {{"name", n->name}});
                v = &it->second;
                used_bindings.insert(n->name);
                need(v->spec == n->specs.front(), "data_input_type",
                     "Named input specification differs from its declaration.");
                library->validate(*v);
                record["kind"] = "input";
                record["name"] = n->name;
                record["value"] = value_description(*v, *library);
            }
            current.outputs.push_back(bound_value(*v));
        } else {
            const auto &op = registry.operation(n->name);
            std::vector<ValueSpec> inputs;
            Json input_refs = Json::array();
            runtime::Node runtime_node;
            runtime_node.id = current.id;
            runtime_node.max_input_bytes = n->options.max_input_bytes;
            runtime_node.max_output_bytes = n->options.max_output_bytes;
            for (const auto &e : n->inputs) {
                inputs.push_back(e.spec());
                const auto &source = lowered.at(LanguageAccess::node(e).get());
                runtime_node.inputs.push_back(source.outputs.at(LanguageAccess::output(e)));
                input_refs.push_back({{"node", source.id}, {"output", LanguageAccess::output(e)}});
            }
            Specialization s;
            try {
                // Node inputs/specs/attributes and its sealed library cannot change.
                // Failed callbacks are not cached: call_once retries after exceptions.
                std::call_once(n->specialization_once,
                               [&] { n->specialization = op.specialize(inputs, n->specs, n->attributes); });
                s = n->specialization;
            } catch (const Error &e) {
                auto d = e.details;
                d["logical_operator"] = n->name;
                d["logical_node"] = current.id;
                throw Error(e.code, e.what(), d);
            }
            need(s.contract.effect == op.effect && s.contract.determinism == op.determinism,
                 "data_specialization_contract",
                 "Specialization changes the operator effect or determinism.");
            auto &contract = s.contract;
            contract.id = "gr.data.pending@1";
            contract.inputs.clear();
            contract.outputs.clear();
            if (contract.meaning.empty())
                contract.meaning = op.meaning;
            metadata(contract.metadata);
            for (const auto &spec : inputs)
                contract.inputs.push_back(bind_type(spec));
            for (const auto &spec : n->specs)
                contract.outputs.push_back(bind_type(spec));
            contract.metadata["gr.data.language"] = {{"operator", op.describe()},
                                                     {"attributes", n->attributes}};
            catalog->pin_types(contract);
            const auto hash = sha256(canonical(contract.to_json()));
            if (!operations.contains(hash)) {
                contract.id = "gr.data.operation." + hash + "@1";
                catalog->register_operation(contract, std::move(s.validate_inputs),
                                            std::move(s.validate_outputs));
                need(!s.providers.empty() && s.providers.size() <= 256, "data_provider",
                     "Specialization needs bounded providers.");
                BoundOperation bound{contract.id, {}};
                for (auto &provider : s.providers) {
                    auto descriptor = std::move(provider.descriptor);
                    kernel::validate_qualified_id(descriptor.id);
                    const auto base = descriptor.id;
                    const auto provider_hash =
                        sha256(canonical(Json{{"base", base}, {"contract", contract.fingerprint()}}));
                    descriptor.id = "gr.data.provider." + provider_hash + "@1";
                    descriptor.operation = contract.id;
                    descriptor.contract_fingerprint = contract.fingerprint();
                    need(bool(provider.function) && bound.providers.emplace(base, descriptor.id).second,
                         "data_provider", "Provider is null or has a duplicate base ID.");
                    std::shared_ptr<std::mutex> serialization;
                    if (!descriptor.thread_safe) {
                        std::lock_guard lock(registry.mutex);
                        auto &shared = registry.provider_mutexes[n->name + "/" + base];
                        if (!shared)
                            shared = std::make_shared<std::mutex>();
                        serialization = shared;
                    }
                    const auto output_types = contract.outputs;
                    const auto logical = n->name;
                    catalog->register_implementation(
                        std::move(descriptor),
                        [fn = std::move(provider.function), output_types, serialization,
                         logical](const kernel::Invocation &values, std::stop_token stop) {
                            std::unique_lock<std::mutex> lock;
                            if (serialization)
                                lock = std::unique_lock<std::mutex>(*serialization);
                            try {
                                auto outputs = fn(values, stop);
                                need(outputs.size() == output_types.size(), "data_provider_arity",
                                     "Provider returned wrong output count.");
                                for (std::size_t i = 0; i < outputs.size(); ++i)
                                    outputs[i].type = output_types[i];
                                return outputs;
                            } catch (const Error &e) {
                                auto d = e.details;
                                d["logical_operator"] = logical;
                                throw Error(e.code, e.what(), d);
                            }
                        });
                }
                operations.emplace(hash, std::move(bound));
            }
            const auto &bound = operations.at(hash);
            runtime_node.operation = bound.id;
            runtime_node.selection = n->options.selection;
            auto resolve_provider = [&](std::string &id) {
                if (id.empty())
                    return;
                if (auto it = bound.providers.find(id); it != bound.providers.end()) {
                    id = it->second;
                    return;
                }
                for (const auto &[base, qualified] : bound.providers)
                    if (id == qualified)
                        return;
                throw Error("data_selection", "Requested provider is not registered for this specialization.",
                            {{"provider", id}, {"operator", n->name}});
            };
            resolve_provider(runtime_node.selection.implementation);
            for (auto &id : runtime_node.selection.prefer_implementations)
                resolve_provider(id);
            record["kind"] = "call";
            record["operator"] = op.describe();
            record["attributes"] = n->attributes;
            record["inputs"] = input_refs;
            record["operation"] = bound.id;
            record["selection"] = selection_json(n->options.selection);
            record["outputs"] = Json::array();
            for (std::size_t i = 0; i < n->specs.size(); ++i) {
                current.outputs.push_back(runtime::OutputRef{current.id, i});
                record["outputs"].push_back(n->specs[i].to_json());
            }
            graph.nodes.push_back(std::move(runtime_node));
        }
        recipe["nodes"].push_back(std::move(record));
        lowered.emplace(n.get(), std::move(current));
    }
    need(used_bindings.size() == bindings.size(), "data_input_unused", "Bindings contain unused names.");
    std::map<std::string, std::string> identity_operations;
    std::map<const Node *, runtime::OutputRef> literal_outputs;
    for (const auto &root : roots) {
        const auto *node = LanguageAccess::node(root).get();
        auto &source = lowered.at(node);
        const auto index = LanguageAccess::output(root);
        recipe["outputs"].push_back({{"node", source.id}, {"output", index}});
        const auto &operand = source.outputs.at(index);
        if (std::holds_alternative<runtime::OutputRef>(operand))
            graph.outputs.push_back(std::get<runtime::OutputRef>(operand));
        else {
            if (!literal_outputs.contains(node)) {
                const auto &value = std::get<kernel::Value>(operand);
                if (!identity_operations.contains(value.type)) {
                    kernel::OperationDescriptor identity;
                    identity.id = "gr.data.identity." + sha256(value.type) + "@1";
                    identity.inputs = {value.type};
                    identity.outputs = {value.type};
                    identity.meaning = "Return the exact immutable selected literal/input payload.";
                    catalog->pin_types(identity);
                    catalog->register_operation(identity);
                    catalog->register_implementation({"gr.data.identity.cpu." + sha256(value.type) + "@1",
                                                      identity.id,
                                                      identity.fingerprint(),
                                                      {"cpu.sequential", "cpu.threaded"},
                                                      {"cpu"},
                                                      {"host"},
                                                      kernel::Determinism::Exact,
                                                      true,
                                                      Json::object()},
                                                     [](const kernel::Invocation &v, std::stop_token stop) {
                                                         if (stop.stop_requested())
                                                             throw Error("execution_cancelled",
                                                                         "Literal selection was cancelled.");
                                                         return v;
                                                     });
                    identity_operations.emplace(value.type, identity.id);
                }
                runtime::Node identity;
                identity.id = source.id;
                identity.operation = identity_operations.at(value.type);
                identity.inputs = {value};
                graph.nodes.push_back(std::move(identity));
                literal_outputs.emplace(node, runtime::OutputRef{source.id, 0});
            }
            graph.outputs.push_back(literal_outputs.at(node));
        }
    }
    const auto recipe_id = sha256(canonical(recipe));
    // Check the complete future provenance before dispatch, including wrapper
    // overhead. Plans use fixed-length SHA-256 IDs in the current runtime format.
    const auto lineage_bytes =
        canonical(result_provenance(recipe_id, recipe, "sha256:" + std::string(64, '0'), roots.size() - 1))
            .size();
    need(lineage_bytes <= limits.max_plan_bytes / roots.size(), "data_lineage_limit",
         "Aggregate serialized result lineage exceeds the compilation metadata budget.");
    graph.context = {{"gr.data.recipe_id", recipe_id}};
    catalog->seal();
    runtime::Planner planner(catalog, limits);
    Program program;
    program.library_ = library;
    program.plan_ = planner.compile(graph);
    for (const auto &root : roots)
        program.outputs_.push_back(root.spec());
    program.description_ = {{"format", "gr.data.program@1"},
                            {"recipe_id", recipe_id},
                            {"recipe", std::move(recipe)},
                            {"plan", program.plan_->describe()},
                            {"catalog", catalog->capabilities()}};
    return program;
}
const std::shared_ptr<const runtime::PreparedPlan> &Program::plan() const {
    need(bool(plan_), "data_program", "Program is empty.");
    return plan_;
}
const std::vector<ValueSpec> &Program::outputs() const {
    plan();
    return outputs_;
}
Json Program::describe() const {
    plan();
    return description_;
}
std::vector<DataValue> Program::materialize(const runtime::JobResult &result) const {
    plan();
    need(result.status == runtime::JobStatus::succeeded, "data_result_status",
         "Only successful jobs can materialize typed results.");
    need(result.diagnostics.value("plan", std::string{}) == plan_->id(), "data_result_plan",
         "Result belongs to a different prepared plan.");
    need(result.outputs.size() == outputs_.size(), "data_result_arity",
         "Result output count differs from program.");
    std::vector<DataValue> values;
    for (std::size_t i = 0; i < outputs_.size(); ++i) {
        const auto type_id = "gr.data.bound." + library_->binding_fingerprint(outputs_[i]) + "@1";
        need(result.outputs[i].type == type_id, "data_result_type", "Result has an unexpected bound type.");
        DataValue v{
            outputs_[i], result.outputs[i],
            result_provenance(description_.at("recipe_id"), description_.at("recipe"), plan_->id(), i)};
        library_->validate(v);
        values.push_back(std::move(v));
    }
    return values;
}
Function::Function(std::vector<Expr> roots) : roots_(std::move(roots)) {
    using Node = LanguageAccess::Node;
    need(!roots_.empty() && roots_.size() <= 4096 && bool(roots_.front()), "data_expression_limit",
         "Function needs bounded nonempty roots.");
    const auto library = LanguageAccess::node(roots_.front())->library;
    std::set<const Node *> done, visiting;
    std::vector<std::pair<std::shared_ptr<const Node>, bool>> stack;
    for (const auto &root : roots_) {
        need(bool(root) && LanguageAccess::node(root)->library == library, "data_expression_library",
             "Function roots must use the same sealed library.");
        stack.emplace_back(LanguageAccess::node(root), false);
        while (!stack.empty()) {
            auto [node, expanded] = std::move(stack.back());
            stack.pop_back();
            if (done.contains(node.get()))
                continue;
            if (expanded) {
                visiting.erase(node.get());
                done.insert(node.get());
                order_.push_back(LanguageAccess::expression(node));
                if (node->kind == Node::Kind::Input) {
                    auto [i, added] = inputs_.emplace(node->name, node->specs.front());
                    need(added || i->second == node->specs.front(), "data_input_type",
                         "Function input name has inconsistent specifications.");
                }
                continue;
            }
            need(done.size() + visiting.size() < 65536, "data_expression_limit",
                 "Function exceeds its 65536-node inspection limit.");
            need(visiting.insert(node.get()).second, "data_expression_cycle",
                 "Expression dependencies must be acyclic.");
            stack.emplace_back(node, true);
            for (auto i = node->inputs.rbegin(); i != node->inputs.rend(); ++i)
                if (!done.contains(LanguageAccess::node(*i).get()))
                    stack.emplace_back(LanguageAccess::node(*i), false);
        }
    }
}
const std::map<std::string, ValueSpec, std::less<>> &Function::inputs() const {
    return inputs_;
}
Json Function::describe() const {
    Json parameters = Json::object(), outputs = Json::array();
    for (const auto &[name, spec] : inputs_)
        parameters[name] = spec.to_json();
    for (const auto &root : roots_)
        outputs.push_back(root.spec().to_json());
    return {{"format", "gr.data.function@1"},
            {"inputs", parameters},
            {"outputs", outputs},
            {"nodes", order_.size()},
            {"effects", "pure"},
            {"scope", "in-process immutable expression template; not a portable executable"}};
}
Program Function::bind(const Bindings &bindings, runtime::PlannerLimits limits) const {
    return compile(roots_, bindings, limits);
}
std::vector<Expr> Function::instantiate(const std::map<std::string, Expr, std::less<>> &arguments) const {
    using Node = LanguageAccess::Node;
    const auto library = LanguageAccess::node(roots_.front())->library;
    for (const auto &[name, spec] : inputs_) {
        const auto found = arguments.find(name);
        need(found != arguments.end(), "data_input_missing", "Function argument is absent.");
        const auto &expr = found->second;
        need(bool(expr) && LanguageAccess::node(expr)->library == library, "data_expression_library",
             "Function arguments must use the same sealed library.");
        need(expr.spec() == spec, "data_input_type", "Function argument has the wrong specification.");
    }
    need(arguments.size() == inputs_.size(), "data_input_unused", "Function has unused arguments.");
    std::map<const Node *, std::vector<Expr>> lowered;
    for (const auto &expr : order_) {
        const auto &node = LanguageAccess::node(expr);
        if (node->kind == Node::Kind::Input)
            lowered.emplace(node.get(), std::vector<Expr>{arguments.at(node->name)});
        else if (node->kind == Node::Kind::Literal)
            lowered.emplace(node.get(), std::vector<Expr>{expr});
        else {
            std::vector<Expr> inputs;
            for (const auto &e : node->inputs)
                inputs.push_back(lowered.at(LanguageAccess::node(e).get()).at(LanguageAccess::output(e)));
            lowered.emplace(node.get(),
                            call(library, node->name, std::move(inputs), node->attributes, node->options));
        }
    }
    std::vector<Expr> result;
    for (const auto &root : roots_)
        result.push_back(lowered.at(LanguageAccess::node(root).get()).at(LanguageAccess::output(root)));
    return result;
}
} // namespace gr::data
