#pragma once
#include "gr/kernel/numerical.hpp"
#include "gr/kernel/types.hpp"
namespace gr::kernel {
enum class Effect { Pure, SnapshotRead, HistoryMutation, External };
enum class Determinism { Exact, DeclaredApproximate, Nondeterministic };
std::string to_string(Effect effect);
std::string to_string(Determinism determinism);
// Fingerprints identify the description, not the correctness of executable code.
struct OperationDescriptor {
    std::string id;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    Effect effect = Effect::Pure;
    Determinism determinism = Determinism::Exact;
    std::string meaning;
    std::vector<std::string> preconditions;
    std::vector<std::string> invariants;
    Json metadata = Json::object();
    // Exact value descriptions are part of the semantic contract, not merely
    // names that a separate registry could reinterpret.
    std::map<std::string, std::string> type_fingerprints;
    // Absent is serialized exactly as v0.3; adding one changes the pinned contract.
    std::optional<NumericalContract> numerical;
    Json to_json() const;
    std::string fingerprint() const;
};
// Replace the complete binding set from an explicitly supplied type registry.
void pin_types(OperationDescriptor &descriptor, const ValueTypeRegistry &types);
class PreparedOperation {
  public:
    const OperationDescriptor &descriptor() const;
    const std::string &fingerprint() const;
    void validate_inputs(const Invocation &) const;
    void validate_outputs(const Invocation &, const Invocation &) const;

  private:
    friend class OperationRegistry;
    OperationDescriptor descriptor_;
    std::string fingerprint_;
    std::vector<PreparedValueType> inputs_, outputs_;
    std::function<void(const Invocation &)> input_validator_;
    std::function<void(const Invocation &, const Invocation &)> output_validator_;
};
class OperationRegistry {
  public:
    // Additional checks are trusted, pure and thread-safe. They do not execute
    // the operation or establish that arbitrary provider code implements it.
    using InputValidator = std::function<void(const Invocation &)>;
    using OutputValidator = std::function<void(const Invocation &, const Invocation &)>;
    explicit OperationRegistry(bool builtins = true);
    void register_operation(OperationDescriptor descriptor, InputValidator input_validator = {},
                            OutputValidator output_validator = {});
    OperationDescriptor describe(std::string_view id) const;
    PreparedOperation prepare(std::string_view id, const ValueTypeRegistry &) const;
    void validate_types(std::string_view operation, const ValueTypeRegistry &types) const;
    void validate_inputs(std::string_view operation, const Invocation &inputs,
                         const ValueTypeRegistry &types) const;
    void validate_outputs(std::string_view operation, const Invocation &inputs, const Invocation &outputs,
                          const ValueTypeRegistry &types) const;
    Json capabilities() const;

  private:
    struct Entry {
        OperationDescriptor descriptor;
        InputValidator input_validator;
        OutputValidator output_validator;
    };
    Entry resolve(std::string_view id) const;
    mutable std::mutex mutex_;
    std::map<std::string, Entry, std::less<>> entries_;
};
} // namespace gr::kernel
