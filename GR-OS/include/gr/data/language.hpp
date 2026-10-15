#pragma once
#include "gr/runtime/scheduler.hpp"
#include <functional>
#include <map>
#include <memory>

namespace gr::data {
// Structural logical identity; physical representation is deliberately separate.
struct Type {
    std::string family;
    std::vector<Type> arguments;
    Json parameters = Json::object();
    Json to_json() const;
    static Type from_json(const Json &);
    std::string fingerprint() const;
    bool operator==(const Type &) const = default;
};
struct ValueSpec {
    Type type;
    std::string representation;
    Json to_json() const;
    static ValueSpec from_json(const Json &);
    bool operator==(const ValueSpec &) const = default;
};
struct DataValue {
    ValueSpec spec;
    kernel::Value payload;
    Json provenance = Json::object();
};
struct TypeFamily {
    std::string id;
    std::string meaning;
    std::size_t min_arguments = 0;
    std::size_t max_arguments = 0;
    Json traits = Json::object();
    Json metadata = Json::object();
    std::function<void(const Type &)> validate;
    Json describe() const;
};
struct Representation {
    std::string id;
    kernel::Representation carrier = kernel::Representation::Buffers;
    std::string meaning;
    Json metadata = Json::object();
    std::function<bool(const Type &)> supports;
    std::function<void(const Type &, const kernel::Value &)> validate;
    Json describe() const;
};
struct Provider {
    // Versioned provider base ID; compilation qualifies it by the bound contract.
    execution::ImplementationDescriptor descriptor;
    // Inputs/outputs use payload carriers. The adapter assigns concrete type tags;
    // it validates the full registered representation before accepting results.
    execution::ImplementationFunction function;
};
struct Specialization {
    // Core fills id, input/output IDs and type fingerprints. Modules supply meaning,
    // numerical contract, preconditions and invariants consistent with Operator.
    kernel::OperationDescriptor contract;
    kernel::OperationRegistry::InputValidator validate_inputs;
    kernel::OperationRegistry::OutputValidator validate_outputs;
    std::vector<Provider> providers;
};
struct Operator {
    std::string id;
    std::string meaning;
    kernel::Effect effect = kernel::Effect::Pure;
    kernel::Determinism determinism = kernel::Determinism::Exact;
    Json metadata = Json::object();
    // Explicit descriptions of possible losses/assumptions, not authority claims.
    Json qualifications = Json::array();
    std::function<std::vector<ValueSpec>(const std::vector<ValueSpec> &, const Json &)> infer;
    std::function<Specialization(const std::vector<ValueSpec> &, const std::vector<ValueSpec> &,
                                 const Json &)>
        specialize;
    Json describe() const;
};
struct ValidationCacheLimits {
    std::size_t max_entries = 64;
    std::size_t max_bytes = 16 * 1024 * 1024;
    bool profile_timing = false;
};
class Library {
  public:
    explicit Library(ValidationCacheLimits = {});
    ~Library();
    Library(const Library &) = delete;
    Library &operator=(const Library &) = delete;
    void register_type(TypeFamily);
    void register_representation(Representation);
    void register_operator(Operator);
    void register_backend(std::shared_ptr<const execution::ExecutionBackend>);
    void seal();
    bool sealed() const;
    void validate(const Type &) const;
    void validate(const ValueSpec &) const;
    void validate(const DataValue &) const;
    std::string binding_fingerprint(const ValueSpec &) const;
    Json capabilities() const;
    // Weak-owner cache: never extends immutable payload/allocation lifetime.
    // Limits charge source payload + specification bytes, not process RSS.
    Json validation_statistics() const;
    void clear_validation_cache() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend struct LanguageAccess;
};
struct CallOptions {
    runtime::Selection selection; // Provider base IDs translated to qualified IDs.
    std::size_t max_output_bytes = 64 * 1024 * 1024;
    std::size_t max_input_bytes = 64 * 1024 * 1024;
};
class Expr {
  public:
    Expr() = default;
    const ValueSpec &spec() const;
    Json describe() const;
    explicit operator bool() const noexcept;

  private:
    struct Node;
    std::shared_ptr<const Node> node_;
    std::size_t output_ = 0;
    Expr(std::shared_ptr<const Node>, std::size_t);
    friend struct LanguageAccess;
};
// Sealing is explicit. Literals freeze value/qualification copies at construction.
Expr literal(std::shared_ptr<const Library>, DataValue);
Expr input(std::shared_ptr<const Library>, std::string name, ValueSpec);
std::vector<Expr> call(std::shared_ptr<const Library>, std::string_view operation, std::vector<Expr> inputs,
                       Json attributes = Json::object(), CallOptions = {});
Expr apply(std::shared_ptr<const Library>, std::string_view operation, std::vector<Expr> inputs,
           Json attributes = Json::object(), CallOptions = {});
using Bindings = std::map<std::string, DataValue, std::less<>>;
class Program {
  public:
    const std::shared_ptr<const runtime::PreparedPlan> &plan() const;
    const std::vector<ValueSpec> &outputs() const;
    Json describe() const;
    // Checks successful result and exact plan identity before attaching lineage.
    std::vector<DataValue> materialize(const runtime::JobResult &) const;

  private:
    std::shared_ptr<const Library> library_;
    std::shared_ptr<const runtime::PreparedPlan> plan_;
    std::vector<ValueSpec> outputs_;
    Json description_;
    friend Program compile(const std::vector<Expr> &, const Bindings &, runtime::PlannerLimits);
};
// Only reachable nodes lower. Shared Expr objects lower once. Input bindings pin
// values at compilation; reuse the expression template to bind another chunk.
// Aggregate serialized output lineage is capped by PlannerLimits.max_plan_bytes,
// separately from the runtime plan charge; neither is a total process RSS bound.
Program compile(const std::vector<Expr> &outputs, const Bindings & = {}, runtime::PlannerLimits = {});
// Immutable named-input expression template. Substitution preserves explicit
// sharing, call options and the sealed library; no hidden execution or effects.
class Function {
  public:
    explicit Function(std::vector<Expr> outputs);
    const std::map<std::string, ValueSpec, std::less<>> &inputs() const;
    Json describe() const;
    Program bind(const Bindings & = {}, runtime::PlannerLimits = {}) const;
    std::vector<Expr> instantiate(const std::map<std::string, Expr, std::less<>> &) const;

  private:
    std::vector<Expr> roots_;
    std::vector<Expr> order_;
    std::map<std::string, ValueSpec, std::less<>> inputs_;
};
} // namespace gr::data
