#pragma once
#include "gr/execution/implementation.hpp"
#include <chrono>
#include <optional>
namespace gr::execution {
using Batch = std::vector<kernel::Invocation>;
struct ExecutionOptions {
    std::string device_id = "cpu:0";
    // Compute workers; deadlines use at most one additional monitor thread.
    std::size_t max_threads = 1;
    std::size_t max_batch_items = 65536;
    std::size_t max_input_bytes = 64 * 1024 * 1024;
    std::size_t max_output_bytes = 64 * 1024 * 1024;
    std::stop_token stop_token;
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
struct BatchResult {
    Batch outputs;
    Json metadata;
};
struct ExecutionTarget {
    std::string device;
    std::string kind;
    std::string memory;
    std::string binding = "gr.host-values/1";
};
class PreparedExecutor {
  public:
    virtual ~PreparedExecutor() = default;
    virtual kernel::Invocation invoke(const kernel::Invocation &, std::stop_token, std::size_t input_limit,
                                      std::size_t output_limit) const = 0;
    virtual std::size_t cpu_slots() const = 0;
    virtual Json description() const = 0;
};
// Synchronous borrowed input; owned output; all started workers joined on return.
// Failure returns no partial batch. Native callbacks are trusted, must not retain
// inputs or perform irreversible effects, and must cooperate with cancellation.
// Payload caps cannot bound their internal allocations or undo their effects.
class ExecutionBackend {
  public:
    virtual ~ExecutionBackend() = default;
    virtual std::string id() const = 0;
    virtual Json capabilities() const = 0;
    // Old batch-only backends remain usable directly, but require an explicit
    // prepared implementation before a runtime catalog can select them.
    virtual std::vector<ExecutionTarget> targets() const { return {}; }
    virtual std::shared_ptr<const PreparedExecutor>
    prepare(const kernel::ValueTypeRegistry &, const kernel::OperationRegistry &,
            const ImplementationRegistry &, std::string_view operation, std::string_view implementation,
            const ExecutionTarget &) const;
    virtual BatchResult execute(const kernel::ValueTypeRegistry &types,
                                const kernel::OperationRegistry &operations,
                                const ImplementationRegistry &implementations, std::string_view operation,
                                std::string_view implementation, const Batch &inputs,
                                const ExecutionOptions &options = {}) const = 0;
};
} // namespace gr::execution
