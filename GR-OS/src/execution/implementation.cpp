#include "gr/execution/implementation.hpp"
#include <set>

namespace gr::execution {
namespace {
void list(const std::vector<std::string> &values) {
    if (values.empty() || values.size() > 64)
        throw Error("implementation_descriptor", "Capability lists need between one and 64 entries.");
    std::set<std::string> seen;
    for (const auto &value : values)
        if (value.empty() || value.size() > 256 || value.find('\0') != std::string::npos ||
            !seen.insert(value).second)
            throw Error("implementation_descriptor",
                        "Capability entries must be distinct, bounded nonempty identifiers.");
}
} // namespace
Json ImplementationDescriptor::to_json() const {
    return {{"id", id},
            {"operation", operation},
            {"contract_fingerprint", contract_fingerprint},
            {"backend_ids", backend_ids},
            {"device_kinds", device_kinds},
            {"memory_domains", memory_domains},
            {"determinism", kernel::to_string(determinism)},
            {"thread_safe", thread_safe},
            {"binding", "gr.host-values/1"},
            {"metadata", metadata}};
}
void ImplementationRegistry::register_implementation(ImplementationDescriptor descriptor,
                                                     ImplementationFunction function) {
    kernel::validate_qualified_id(descriptor.id);
    kernel::validate_qualified_id(descriptor.operation);
    const auto &hash = descriptor.contract_fingerprint;
    if (hash.size() != 71 || !hash.starts_with("sha256:") ||
        hash.find_first_not_of("0123456789abcdef", 7) != std::string::npos)
        throw Error("implementation_descriptor",
                    "Implementation must pin an exact semantic contract fingerprint.");
    list(descriptor.backend_ids);
    list(descriptor.device_kinds);
    list(descriptor.memory_domains);
    if (!descriptor.metadata.is_object() || !function)
        throw Error("implementation_descriptor",
                    "Implementation requires object metadata and a native callback.");
    canonical(descriptor.to_json());
    std::lock_guard lock(mutex_);
    const auto id = descriptor.id;
    if (entries_.contains(id))
        throw Error("duplicate_implementation", "Physical implementation identity is already registered.");
    entries_.emplace(id, RegisteredImplementation{std::move(descriptor), std::move(function),
                                                  std::make_shared<std::mutex>()});
}
RegisteredImplementation ImplementationRegistry::resolve(std::string_view id) const {
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(id);
    if (found == entries_.end())
        throw Error("unsupported_implementation", "Physical implementation is unavailable.",
                    {{"implementation", id}});
    return found->second;
}
ImplementationDescriptor ImplementationRegistry::describe(std::string_view id) const {
    return resolve(id).descriptor;
}
Json ImplementationRegistry::capabilities() const {
    std::lock_guard lock(mutex_);
    auto result = Json::array();
    for (const auto &[id, entry] : entries_) {
        (void)id;
        result.push_back(entry.descriptor.to_json());
    }
    return result;
}
std::vector<ImplementationDescriptor> ImplementationRegistry::descriptors() const {
    std::lock_guard lock(mutex_);
    std::vector<ImplementationDescriptor> result;
    for (const auto &[id, entry] : entries_) {
        (void)id;
        result.push_back(entry.descriptor);
    }
    return result;
}
Json ImplementationRegistry::compatible(const kernel::OperationDescriptor &operation) const {
    const auto fingerprint = operation.fingerprint();
    std::lock_guard lock(mutex_);
    auto result = Json::array();
    for (const auto &[id, entry] : entries_) {
        (void)id;
        const auto &d = entry.descriptor;
        if (d.operation == operation.id && d.contract_fingerprint == fingerprint &&
            d.determinism == operation.determinism)
            result.push_back(d.to_json());
    }
    return result;
}
} // namespace gr::execution
