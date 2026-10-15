#pragma once
#include "gr/kernel/operations.hpp"
#include <memory>
#include <stop_token>
namespace gr::execution {
// Synchronous gr.host-values/1 binding; no device-buffer/zero-copy promise.
struct ImplementationDescriptor {
    std::string id;
    std::string operation;
    std::string contract_fingerprint;
    std::vector<std::string> backend_ids;
    std::vector<std::string> device_kinds;
    std::vector<std::string> memory_domains;
    kernel::Determinism determinism = kernel::Determinism::Exact;
    bool thread_safe = false;
    Json metadata = Json::object();
    Json to_json() const;
};
using ImplementationFunction = std::function<kernel::Invocation(const kernel::Invocation &, std::stop_token)>;
struct RegisteredImplementation {
    ImplementationDescriptor descriptor;
    ImplementationFunction function;
    // Shared by registry resolutions; serializes a non-thread-safe provider even
    // when two independent callers use the sequential backend concurrently.
    std::shared_ptr<std::mutex> serialization_mutex;
};
// Many providers can target one operation; no automatic selection or fallback.
class ImplementationRegistry {
  public:
    void register_implementation(ImplementationDescriptor descriptor, ImplementationFunction function);
    ImplementationDescriptor describe(std::string_view id) const;
    RegisteredImplementation resolve(std::string_view id) const;
    Json capabilities() const;
    std::vector<ImplementationDescriptor> descriptors() const;
    // Semantic match only. Backend/device/binding checks still apply.
    Json compatible(const kernel::OperationDescriptor &operation) const;

  private:
    mutable std::mutex mutex_;
    std::map<std::string, RegisteredImplementation, std::less<>> entries_;
};
} // namespace gr::execution
