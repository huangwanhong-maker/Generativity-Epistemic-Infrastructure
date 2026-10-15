#include "gr/kernel/operations.hpp"
#include <algorithm>
#include <set>

namespace gr::kernel {
namespace {
void require(bool condition, const char *code, const char *message) {
    if (!condition)
        throw Error(code, message);
}
void validate_signature(const Invocation &values, const std::vector<std::string> &signature,
                        const ValueTypeRegistry &types, const char *code) {
    require(values.size() == signature.size(), code,
            "Operation argument/result arity does not match its signature.");
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i].type != signature[i])
            throw Error(code, "Operation value type does not exactly match its signature.",
                        {{"index", i}, {"expected", signature[i]}, {"actual", values[i].type}});
        types.validate(values[i]);
    }
}
void check_types(const OperationDescriptor &descriptor, const ValueTypeRegistry &types) {
    for (const auto &[id, fingerprint] : descriptor.type_fingerprints) {
        const auto actual = types.describe(id).fingerprint();
        if (actual != fingerprint)
            throw Error("operation_type_contract",
                        "Value type differs from the operation's exact type contract.",
                        {{"type", id}, {"expected", fingerprint}, {"actual", actual}});
    }
}
} // namespace
std::string to_string(Effect effect) {
    switch (effect) {
    case Effect::Pure:
        return "pure";
    case Effect::SnapshotRead:
        return "snapshot_read";
    case Effect::HistoryMutation:
        return "history_mutation";
    case Effect::External:
        return "external";
    }
    throw Error("effect", "Unknown operation effect.");
}
std::string to_string(Determinism determinism) {
    switch (determinism) {
    case Determinism::Exact:
        return "exact";
    case Determinism::DeclaredApproximate:
        return "declared_approximate";
    case Determinism::Nondeterministic:
        return "nondeterministic";
    }
    throw Error("determinism", "Unknown determinism contract.");
}
Json OperationDescriptor::to_json() const {
    Json result{{"id", id},
                {"inputs", inputs},
                {"outputs", outputs},
                {"effect", to_string(effect)},
                {"determinism", to_string(determinism)},
                {"meaning", meaning},
                {"preconditions", preconditions},
                {"invariants", invariants},
                {"metadata", metadata},
                {"type_fingerprints", type_fingerprints}};
    if (numerical)
        result["numerical"] = numerical->to_json();
    return result;
}
std::string OperationDescriptor::fingerprint() const {
    return "sha256:" + sha256(canonical(to_json()));
}
void pin_types(OperationDescriptor &descriptor, const ValueTypeRegistry &types) {
    std::map<std::string, std::string> fingerprints;
    for (const auto &id : descriptor.inputs)
        fingerprints[id] = types.describe(id).fingerprint();
    for (const auto &id : descriptor.outputs)
        fingerprints[id] = types.describe(id).fingerprint();
    descriptor.type_fingerprints = std::move(fingerprints);
}
OperationRegistry::OperationRegistry(bool builtins) {
    if (!builtins)
        return;
    const ValueTypeRegistry types;
    auto pinned = [&](OperationDescriptor descriptor) {
        pin_types(descriptor, types);
        return descriptor;
    };
    register_operation(pinned({"gr.bytes.identity@1",
                               {"gr.bytes@1"},
                               {"gr.bytes@1"},
                               Effect::Pure,
                               Determinism::Exact,
                               "Return exactly the supplied octets in their original order.",
                               {},
                               {"Output bytes equal input bytes; input is unchanged."},
                               {{"runtime_postcondition", "exact byte equality"}}}),
                       {}, [](const Invocation &input, const Invocation &output) {
                           require(input[0].as_bytes() == output[0].as_bytes(), "postcondition",
                                   "Identity implementation changed the bytes.");
                       });
    register_operation(pinned(
        {"gr.bytes.sha256@1",
         {"gr.bytes@1"},
         {"gr.sha256-hex@1"},
         Effect::Pure,
         Determinism::Exact,
         "Compute SHA-256 over the exact input octets and return 64 lowercase ASCII hexadecimal digits.",
         {},
         {"No text decoding or normalization; identical inputs yield identical outputs."},
         {{"runtime_postcondition",
           "output representation only; algorithm checked by conformance vectors"}}}));
    register_operation(pinned({"gr.bytes.concat@1",
                               {"gr.bytes@1", "gr.bytes@1"},
                               {"gr.bytes@1"},
                               Effect::Pure,
                               Determinism::Exact,
                               "Concatenate the first octet sequence followed by the second.",
                               {},
                               {"Argument order is significant; empty sequences are identities."},
                               {{"runtime_postcondition", "exact ordered concatenation"}}}),
                       {}, [](const Invocation &input, const Invocation &output) {
                           const auto &a = input[0].as_bytes();
                           const auto &b = input[1].as_bytes();
                           const auto &c = output[0].as_bytes();
                           require(a.size() <= c.size() && b.size() == c.size() - a.size() &&
                                       std::equal(a.begin(), a.end(), c.begin()) &&
                                       std::equal(b.begin(), b.end(), c.begin() + a.size()),
                                   "postcondition",
                                   "Concatenation implementation violated ordered concatenation.");
                       });
}
void OperationRegistry::register_operation(OperationDescriptor descriptor, InputValidator input_validator,
                                           OutputValidator output_validator) {
    validate_qualified_id(descriptor.id);
    require(!descriptor.meaning.empty(), "operation_descriptor", "Operation meaning must be stated.");
    require(descriptor.inputs.size() <= 64 && descriptor.outputs.size() <= 64, "operation_descriptor",
            "This contract edition supports at most 64 inputs and outputs.");
    for (const auto &type : descriptor.inputs)
        validate_qualified_id(type);
    for (const auto &type : descriptor.outputs)
        validate_qualified_id(type);
    std::set<std::string> required_types(descriptor.inputs.begin(), descriptor.inputs.end());
    required_types.insert(descriptor.outputs.begin(), descriptor.outputs.end());
    require(descriptor.type_fingerprints.size() == required_types.size(), "operation_type_contract",
            "Operation requires one exact fingerprint for every signature type and no extra bindings.");
    for (const auto &[id, fingerprint] : descriptor.type_fingerprints) {
        require(required_types.contains(id), "operation_type_contract",
                "Operation type binding is outside its signature.");
        require(fingerprint.size() == 71 && fingerprint.starts_with("sha256:") &&
                    std::all_of(fingerprint.begin() + 7, fingerprint.end(),
                                [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
                "operation_type_contract", "Operation type fingerprint must be a lowercase SHA-256 digest.");
    }
    require(descriptor.metadata.is_object(), "operation_descriptor", "Operation metadata must be an object.");
    if (descriptor.numerical) {
        descriptor.numerical->validate();
        require(descriptor.determinism != Determinism::Exact ||
                    descriptor.numerical->repeatability == Repeatability::AcrossProviders,
                "numerical_contract", "Exact contracts require repeatability across compatible providers.");
        require(descriptor.determinism != Determinism::DeclaredApproximate ||
                    descriptor.numerical->tolerance.has_value(),
                "numerical_contract", "Declared numerical approximation requires a reference and tolerance.");
    }
    for (const auto &text : descriptor.preconditions)
        require(!text.empty(), "operation_descriptor", "Precondition descriptions cannot be empty.");
    for (const auto &text : descriptor.invariants)
        require(!text.empty(), "operation_descriptor", "Invariant descriptions cannot be empty.");
    canonical(descriptor.to_json());
    std::lock_guard lock(mutex_);
    const auto id = descriptor.id;
    require(!entries_.contains(id), "duplicate_operation", "Operation identity is already registered.");
    entries_.emplace(id,
                     Entry{std::move(descriptor), std::move(input_validator), std::move(output_validator)});
}
OperationRegistry::Entry OperationRegistry::resolve(std::string_view id) const {
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(id);
    if (found == entries_.end())
        throw Error("unsupported_operation", "Semantic operation is unavailable.", {{"operation", id}});
    return found->second;
}
OperationDescriptor OperationRegistry::describe(std::string_view id) const {
    return resolve(id).descriptor;
}
PreparedOperation OperationRegistry::prepare(std::string_view id, const ValueTypeRegistry &types) const {
    const auto entry = resolve(id);
    PreparedOperation prepared;
    prepared.descriptor_ = entry.descriptor;
    prepared.fingerprint_ = entry.descriptor.fingerprint();
    prepared.input_validator_ = entry.input_validator;
    prepared.output_validator_ = entry.output_validator;
    std::map<std::string, PreparedValueType> bound;
    for (const auto &[type, expected] : entry.descriptor.type_fingerprints) {
        auto actual = types.prepare(type);
        require(actual.fingerprint() == expected, "operation_type_contract",
                "Value type differs from the operation's exact type contract.");
        bound.emplace(type, std::move(actual));
    }
    for (const auto &type : entry.descriptor.inputs)
        prepared.inputs_.push_back(bound.at(type));
    for (const auto &type : entry.descriptor.outputs)
        prepared.outputs_.push_back(bound.at(type));
    return prepared;
}
const OperationDescriptor &PreparedOperation::descriptor() const {
    return descriptor_;
}
const std::string &PreparedOperation::fingerprint() const {
    return fingerprint_;
}
namespace {
void prepared_signature(const Invocation &values, const std::vector<PreparedValueType> &types,
                        const char *code) {
    require(values.size() == types.size(), code,
            "Operation argument/result arity differs from its signature.");
    for (std::size_t i = 0; i < values.size(); ++i) {
        require(values[i].type == types[i].descriptor().id, code, "Value type differs from its signature.");
        types[i].validate(values[i]);
    }
}
} // namespace
void PreparedOperation::validate_inputs(const Invocation &inputs) const {
    prepared_signature(inputs, inputs_, "input_type");
    if (input_validator_)
        input_validator_(inputs);
}
void PreparedOperation::validate_outputs(const Invocation &inputs, const Invocation &outputs) const {
    prepared_signature(inputs, inputs_, "input_type");
    prepared_signature(outputs, outputs_, "output_type");
    if (output_validator_)
        output_validator_(inputs, outputs);
}
void OperationRegistry::validate_types(std::string_view operation, const ValueTypeRegistry &types) const {
    check_types(resolve(operation).descriptor, types);
}
void OperationRegistry::validate_inputs(std::string_view operation, const Invocation &inputs,
                                        const ValueTypeRegistry &types) const {
    const auto entry = resolve(operation);
    check_types(entry.descriptor, types);
    validate_signature(inputs, entry.descriptor.inputs, types, "input_type");
    if (entry.input_validator)
        entry.input_validator(inputs);
}
void OperationRegistry::validate_outputs(std::string_view operation, const Invocation &inputs,
                                         const Invocation &outputs, const ValueTypeRegistry &types) const {
    const auto entry = resolve(operation);
    check_types(entry.descriptor, types);
    validate_signature(inputs, entry.descriptor.inputs, types, "input_type");
    validate_signature(outputs, entry.descriptor.outputs, types, "output_type");
    if (entry.output_validator)
        entry.output_validator(inputs, outputs);
}
Json OperationRegistry::capabilities() const {
    std::lock_guard lock(mutex_);
    auto result = Json::array();
    for (const auto &[id, entry] : entries_) {
        (void)id;
        auto descriptor = entry.descriptor.to_json();
        descriptor["fingerprint"] = entry.descriptor.fingerprint();
        descriptor["native_input_validator"] = static_cast<bool>(entry.input_validator);
        descriptor["native_output_validator"] = static_cast<bool>(entry.output_validator);
        result.push_back(std::move(descriptor));
    }
    return result;
}
} // namespace gr::kernel
