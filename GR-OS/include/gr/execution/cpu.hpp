#pragma once
#include "gr/execution/backend.hpp"
namespace gr::execution {
Json discover_devices();
void install_cpu_implementations(ImplementationRegistry &implementations,
                                 const kernel::OperationRegistry &operations);
// Single invocation on the calling thread. The runtime, if present, owns its
// worker and deadline monitor. Reusable by separately registered host backends.
std::shared_ptr<const PreparedExecutor>
prepare_host_execution(const kernel::ValueTypeRegistry &, const kernel::OperationRegistry &,
                       const ImplementationRegistry &, std::string_view operation,
                       std::string_view implementation, std::string_view backend, const ExecutionTarget &);
class SequentialBackend final : public ExecutionBackend {
  public:
    std::string id() const override;
    Json capabilities() const override;
    std::vector<ExecutionTarget> targets() const override;
    std::shared_ptr<const PreparedExecutor> prepare(const kernel::ValueTypeRegistry &,
                                                    const kernel::OperationRegistry &,
                                                    const ImplementationRegistry &, std::string_view,
                                                    std::string_view, const ExecutionTarget &) const override;
    BatchResult execute(const kernel::ValueTypeRegistry &, const kernel::OperationRegistry &,
                        const ImplementationRegistry &, std::string_view operation,
                        std::string_view implementation, const Batch &,
                        const ExecutionOptions & = {}) const override;
};
class ThreadedCpuBackend final : public ExecutionBackend {
  public:
    std::string id() const override;
    Json capabilities() const override;
    std::vector<ExecutionTarget> targets() const override;
    std::shared_ptr<const PreparedExecutor> prepare(const kernel::ValueTypeRegistry &,
                                                    const kernel::OperationRegistry &,
                                                    const ImplementationRegistry &, std::string_view,
                                                    std::string_view, const ExecutionTarget &) const override;
    BatchResult execute(const kernel::ValueTypeRegistry &, const kernel::OperationRegistry &,
                        const ImplementationRegistry &, std::string_view operation,
                        std::string_view implementation, const Batch &,
                        const ExecutionOptions & = {}) const override;
};
} // namespace gr::execution
