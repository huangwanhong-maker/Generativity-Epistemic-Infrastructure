#pragma once
#include "gr/execution/backend.hpp"
#include <memory>
#include <string>
#include <vector>

namespace gr::runtime {
struct Selection {
    std::string implementation;
    std::string backend;
    std::string device;
    std::vector<std::string> prefer_implementations;
};
struct ModuleDescriptor {
    std::string id;
    std::vector<std::string> dependencies;
    Json metadata = Json::object();
};
struct SelectedExecution {
    std::shared_ptr<const execution::PreparedExecutor> executable;
    Json selection;
};
// Trusted native registration followed by a permanent seal. No mutable registry
// escapes; prepared handles retain their code/validation/backend lifetimes.
class Catalog {
  public:
    explicit Catalog(bool builtins = true);
    ~Catalog();
    Catalog(const Catalog &) = delete;
    Catalog &operator=(const Catalog &) = delete;
    void register_type(kernel::ValueTypeDescriptor, kernel::ValueTypeRegistry::Validator = {});
    void register_operation(kernel::OperationDescriptor, kernel::OperationRegistry::InputValidator = {},
                            kernel::OperationRegistry::OutputValidator = {});
    void register_implementation(execution::ImplementationDescriptor, execution::ImplementationFunction);
    void register_backend(std::shared_ptr<const execution::ExecutionBackend>);
    void register_module(ModuleDescriptor);
    void pin_types(kernel::OperationDescriptor &) const;
    void seal();
    bool sealed() const;
    Json capabilities() const;
    kernel::OperationDescriptor operation(std::string_view) const;
    void validate(const kernel::Value &) const;
    SelectedExecution prepare(std::string_view operation, const Selection & = {}) const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace gr::runtime
