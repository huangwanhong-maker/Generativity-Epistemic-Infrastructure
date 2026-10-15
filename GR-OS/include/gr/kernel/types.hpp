#pragma once
#include "gr/common.hpp"
#include "gr/memory/buffer.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>

namespace gr::kernel {
using Bytes = std::vector<std::uint8_t>;
enum class Representation { Json, Bytes, Buffers };
std::string to_string(Representation representation);
// Exact namespace.name@version identities; no inferred semver compatibility.
void validate_qualified_id(std::string_view id);
struct Value {
    std::string type;
    Value() = default;
    static Value json(std::string type, Json data);
    static Value bytes(std::string type, Bytes data);
    static Value buffers(std::string type, memory::BufferSet data);
    Representation representation() const;
    const Json &as_json() const;
    const Bytes &as_bytes() const;
    const memory::BufferSet &as_buffers() const;
    std::size_t byte_size() const;
    std::size_t logical_bytes() const;
    std::size_t retained_bytes() const;
    std::string content_digest() const;
    // Process-local immutable payload identity, with weak lifetime. Never a
    // portable digest, type certificate, or authority to dereference raw memory.
    std::weak_ptr<const void> owner_token() const noexcept;
    bool operator==(const Value &) const;

  private:
    using Payload = std::variant<Json, Bytes, memory::BufferSet>;
    Value(std::string, Payload);
    std::shared_ptr<const Payload> data_;
};
using Invocation = std::vector<Value>;
// Portable descriptions do not contain or install executable code.
struct ValueTypeDescriptor {
    std::string id;
    Representation representation = Representation::Json;
    std::string meaning;
    Json constraints = Json::object();
    Json to_json() const;
    std::string fingerprint() const;
};
class PreparedValueType {
  public:
    const ValueTypeDescriptor &descriptor() const;
    const std::string &fingerprint() const;
    void validate(const Value &) const;

  private:
    friend class ValueTypeRegistry;
    ValueTypeDescriptor descriptor_;
    std::string fingerprint_;
    std::function<void(const Value &)> validator_;
};
// Operand representations differ from retained record envelopes/profiles.
// Additional validators are separately installed trusted, thread-safe pure code. Descriptions are
// immutable after registration; changed meaning requires a new versioned ID.
class ValueTypeRegistry {
  public:
    using Validator = std::function<void(const Value &)>;
    explicit ValueTypeRegistry(bool builtins = true);
    void register_type(ValueTypeDescriptor descriptor, Validator validator = {});
    ValueTypeDescriptor describe(std::string_view id) const;
    PreparedValueType prepare(std::string_view id) const;
    void validate(const Value &value) const;
    Json capabilities() const;

  private:
    struct Entry {
        ValueTypeDescriptor descriptor;
        Validator validator;
    };
    Entry resolve(std::string_view id) const;
    mutable std::mutex mutex_;
    std::map<std::string, Entry, std::less<>> entries_;
};
} // namespace gr::kernel
