#include "gr/kernel/types.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace gr::kernel {
namespace {
void require(bool condition, std::string_view code, std::string_view message) {
    if (!condition)
        throw Error(std::string(code), std::string(message));
}
bool name_character(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}
void validate_constraints(const ValueTypeDescriptor &descriptor) {
    const auto &c = descriptor.constraints;
    require(c.is_object(), "type_constraints", "Type constraints must be an object.");
    if (descriptor.representation == Representation::Buffers) {
        const std::set<std::string> allowed{"min_buffers", "max_buffers", "required_buffers",
                                            "max_logical_bytes", "max_retained_bytes"};
        for (auto it = c.begin(); it != c.end(); ++it) {
            require(allowed.contains(it.key()), "type_constraints", "Unsupported buffer-set constraint.");
            if (it.key() == "required_buffers") {
                require(it->is_array(), "type_constraints", "required_buffers must be an array.");
                std::set<std::string> names;
                for (const auto &name : *it)
                    require(name.is_string() && !name.get_ref<const std::string &>().empty() &&
                                names.insert(name.get<std::string>()).second,
                            "type_constraints", "Required buffer names must be distinct nonempty strings.");
            } else {
                require(it->is_number_integer() && *it >= 0 &&
                            it->get<std::uint64_t>() <= std::numeric_limits<std::size_t>::max(),
                        "type_constraints", "Buffer limits require addressable nonnegative integers.");
            }
        }
        if (c.contains("min_buffers") && c.contains("max_buffers"))
            require(c["min_buffers"] <= c["max_buffers"], "type_constraints",
                    "Buffer minimum exceeds maximum.");
        if (c.contains("required_buffers") && c.contains("max_buffers"))
            require(c["required_buffers"].size() <= c["max_buffers"].get<std::size_t>(), "type_constraints",
                    "Required buffers exceed the declared maximum.");
        return;
    }
    const std::set<std::string> byte_keys{"min_bytes", "max_bytes", "byte_length", "alphabet"};
    const std::set<std::string> json_keys{"kind",      "required_keys", "min_items",
                                          "max_items", "min_value",     "max_value"};
    const auto &allowed = descriptor.representation == Representation::Bytes ? byte_keys : json_keys;
    for (auto it = c.begin(); it != c.end(); ++it) {
        require(allowed.contains(it.key()), "type_constraints", "Unsupported declarative type constraint.");
        if (it.key() == "alphabet")
            require(it->is_string(), "type_constraints", "alphabet must be a UTF-8 byte string.");
        else if (it.key() == "kind") {
            const std::set<std::string> kinds{"object", "array", "string", "integer", "boolean", "null"};
            require(it->is_string() && kinds.contains(it->get<std::string>()), "type_constraints",
                    "Unsupported JSON kind.");
        } else if (it.key() == "required_keys") {
            require(it->is_array(), "type_constraints", "required_keys must be an array.");
            std::set<std::string> keys;
            for (const auto &key : *it) {
                require(key.is_string() && keys.insert(key.get<std::string>()).second, "type_constraints",
                        "Required keys must be distinct strings.");
            }
        } else {
            require(it->is_number_integer(), "type_constraints",
                    "Numeric constraints require exact integers.");
            if (it.key() != "min_value" && it.key() != "max_value") {
                require(*it >= 0, "type_constraints", "Size constraints must be nonnegative.");
                require(it->get<std::uint64_t>() <= std::numeric_limits<std::size_t>::max(),
                        "type_constraints", "Size constraint exceeds this platform's addressable range.");
            }
        }
    }
    for (const auto &[lower, upper] :
         {std::pair{"min_bytes", "max_bytes"}, std::pair{"min_items", "max_items"},
          std::pair{"min_value", "max_value"}}) {
        if (c.contains(lower) && c.contains(upper))
            require(c[lower] <= c[upper], "type_constraints", "Constraint minimum exceeds maximum.");
    }
    if (c.contains("byte_length")) {
        if (c.contains("min_bytes"))
            require(c["byte_length"] >= c["min_bytes"], "type_constraints",
                    "Conflicting byte length constraints.");
        if (c.contains("max_bytes"))
            require(c["byte_length"] <= c["max_bytes"], "type_constraints",
                    "Conflicting byte length constraints.");
    }
    if (c.contains("kind")) {
        if (c.contains("required_keys"))
            require(c["kind"] == "object", "type_constraints", "required_keys needs object values.");
        if (c.contains("min_items") || c.contains("max_items"))
            require(c["kind"] == "array", "type_constraints", "Item constraints need arrays.");
        if (c.contains("min_value") || c.contains("max_value"))
            require(c["kind"] == "integer", "type_constraints", "Value constraints need integers.");
    }
    const int implied_kinds = int(c.contains("required_keys")) +
                              int(c.contains("min_items") || c.contains("max_items")) +
                              int(c.contains("min_value") || c.contains("max_value"));
    require(implied_kinds <= 1, "type_constraints", "Constraints require mutually incompatible JSON kinds.");
}
void check_constraints(const Value &value, const Json &c) {
    if (value.representation() == Representation::Buffers) {
        const auto &buffers = value.as_buffers();
        if (c.contains("min_buffers"))
            require(buffers.buffers().size() >= c["min_buffers"].get<std::size_t>(), "value_constraint",
                    "Too few named buffers.");
        if (c.contains("max_buffers"))
            require(buffers.buffers().size() <= c["max_buffers"].get<std::size_t>(), "value_constraint",
                    "Too many named buffers.");
        if (c.contains("required_buffers"))
            for (const auto &name : c["required_buffers"])
                buffers.at(name.get<std::string>());
        if (c.contains("max_logical_bytes"))
            require(buffers.logical_bytes() <= c["max_logical_bytes"].get<std::size_t>(), "value_constraint",
                    "Logical buffer volume exceeds its constraint.");
        if (c.contains("max_retained_bytes"))
            require(buffers.retained_bytes() <= c["max_retained_bytes"].get<std::size_t>(),
                    "value_constraint", "Retained owner allocations exceed their constraint.");
        return;
    }
    if (value.representation() == Representation::Bytes) {
        const auto &bytes = value.as_bytes();
        if (c.contains("min_bytes"))
            require(bytes.size() >= c["min_bytes"].get<std::size_t>(), "value_constraint",
                    "Byte value is shorter than permitted.");
        if (c.contains("max_bytes"))
            require(bytes.size() <= c["max_bytes"].get<std::size_t>(), "value_constraint",
                    "Byte value is longer than permitted.");
        if (c.contains("byte_length"))
            require(bytes.size() == c["byte_length"].get<std::size_t>(), "value_constraint",
                    "Byte value has the wrong length.");
        if (c.contains("alphabet")) {
            const auto alphabet = c["alphabet"].get<std::string>();
            for (auto byte : bytes)
                require(alphabet.find(static_cast<char>(byte)) != std::string::npos, "value_constraint",
                        "Byte value contains a disallowed byte.");
        }
        return;
    }
    const auto &data = value.as_json();
    if (c.contains("kind")) {
        const auto kind = c["kind"].get<std::string>();
        const bool matches = (kind == "object" && data.is_object()) || (kind == "array" && data.is_array()) ||
                             (kind == "string" && data.is_string()) ||
                             (kind == "integer" && data.is_number_integer()) ||
                             (kind == "boolean" && data.is_boolean()) || (kind == "null" && data.is_null());
        require(matches, "value_constraint", "JSON value has the wrong kind.");
    }
    if (c.contains("required_keys")) {
        require(data.is_object(), "value_constraint", "Required-key constraints need an object.");
        for (const auto &key : c["required_keys"])
            require(data.contains(key.get<std::string>()), "value_constraint",
                    "Required JSON key is absent.");
    }
    if (c.contains("min_items") || c.contains("max_items")) {
        require(data.is_array(), "value_constraint", "Item constraints need an array.");
        if (c.contains("min_items"))
            require(data.size() >= c["min_items"].get<std::size_t>(), "value_constraint",
                    "Too few array items.");
        if (c.contains("max_items"))
            require(data.size() <= c["max_items"].get<std::size_t>(), "value_constraint",
                    "Too many array items.");
    }
    if (c.contains("min_value") || c.contains("max_value")) {
        require(data.is_number_integer(), "value_constraint", "Numeric bounds need an integer.");
        if (c.contains("min_value"))
            require(data >= c["min_value"], "value_constraint", "Value is below its minimum.");
        if (c.contains("max_value"))
            require(data <= c["max_value"], "value_constraint", "Value is above its maximum.");
    }
}
} // namespace

std::string to_string(Representation representation) {
    switch (representation) {
    case Representation::Json:
        return "json";
    case Representation::Bytes:
        return "bytes";
    case Representation::Buffers:
        return "buffers";
    }
    throw Error("representation", "Unknown value representation.");
}
void validate_qualified_id(std::string_view id) {
    require(!id.empty() && id.size() <= 256, "qualified_id", "Qualified identity is empty or overlong.");
    const auto at = id.find('@');
    require(at != std::string_view::npos && at != 0 && at + 1 < id.size() &&
                id.find('@', at + 1) == std::string_view::npos,
            "qualified_id", "Identity requires exactly one nonempty @version suffix.");
    const auto name = id.substr(0, at), version = id.substr(at + 1);
    require(name.find('.') != std::string_view::npos && name.front() != '.' && name.back() != '.' &&
                name.find("..") == std::string_view::npos,
            "qualified_id", "Identity requires nonempty namespace segments.");
    for (auto c : name)
        require(name_character(c) || c == '.', "qualified_id", "Invalid namespace character.");
    require(version.size() <= 64 && version.front() != '.' && version.back() != '.' &&
                version.find("..") == std::string_view::npos,
            "qualified_id", "Invalid version suffix.");
    for (auto c : version)
        require(name_character(c) || c == '.', "qualified_id", "Invalid version character.");
}
Value::Value(std::string value_type, Payload data)
    : type(std::move(value_type)), data_(std::make_shared<const Payload>(std::move(data))) {}
Value Value::json(std::string type, Json data) {
    return {std::move(type), std::move(data)};
}
Value Value::bytes(std::string type, Bytes data) {
    return {std::move(type), std::move(data)};
}
Value Value::buffers(std::string type, memory::BufferSet data) {
    return {std::move(type), std::move(data)};
}
std::weak_ptr<const void> Value::owner_token() const noexcept {
    return data_;
}
Representation Value::representation() const {
    require(bool(data_), "representation", "Value has no payload.");
    if (std::holds_alternative<Json>(*data_))
        return Representation::Json;
    return std::holds_alternative<Bytes>(*data_) ? Representation::Bytes : Representation::Buffers;
}
const Json &Value::as_json() const {
    require(data_ && std::holds_alternative<Json>(*data_), "representation", "Expected JSON representation.");
    return std::get<Json>(*data_);
}
const Bytes &Value::as_bytes() const {
    require(data_ && std::holds_alternative<Bytes>(*data_), "representation",
            "Expected byte representation.");
    return std::get<Bytes>(*data_);
}
const memory::BufferSet &Value::as_buffers() const {
    require(data_ && std::holds_alternative<memory::BufferSet>(*data_), "representation",
            "Expected named buffer representation.");
    return std::get<memory::BufferSet>(*data_);
}
bool Value::operator==(const Value &other) const {
    return type == other.type && (data_ == other.data_ || (data_ && other.data_ && *data_ == *other.data_));
}
std::size_t Value::byte_size() const {
    switch (representation()) {
    case Representation::Bytes:
        return as_bytes().size();
    case Representation::Json:
        return canonical(as_json()).size();
    case Representation::Buffers:
        return as_buffers().accounted_bytes();
    }
    throw Error("representation", "Unknown value representation.");
}
std::size_t Value::logical_bytes() const {
    return representation() == Representation::Buffers ? as_buffers().logical_bytes() : byte_size();
}
std::size_t Value::retained_bytes() const {
    return representation() == Representation::Buffers ? as_buffers().retained_bytes() : byte_size();
}
std::string Value::content_digest() const {
    switch (representation()) {
    case Representation::Bytes:
        return "sha256:" + sha256(std::span<const std::uint8_t>(as_bytes()));
    case Representation::Json:
        return "sha256:" + sha256(canonical(as_json()));
    case Representation::Buffers:
        return as_buffers().content_digest();
    }
    throw Error("representation", "Unknown value representation.");
}
Json ValueTypeDescriptor::to_json() const {
    return {{"id", id},
            {"representation", to_string(representation)},
            {"meaning", meaning},
            {"constraints", constraints}};
}
std::string ValueTypeDescriptor::fingerprint() const {
    return "sha256:" + sha256(canonical(to_json()));
}
ValueTypeRegistry::ValueTypeRegistry(bool builtins) {
    if (!builtins)
        return;
    register_type(
        {"gr.bytes@1", Representation::Bytes, "An ordered finite sequence of octets.", Json::object()});
    register_type({"gr.json@1", Representation::Json,
                   "A JSON value in the bounded gr.canonical-json/1 domain.", Json::object()});
    register_type({"gr.sha256-hex@1",
                   Representation::Bytes,
                   "The 64 lowercase ASCII hexadecimal digits of a SHA-256 digest.",
                   {{"byte_length", 64}, {"alphabet", "0123456789abcdef"}}});
}
void ValueTypeRegistry::register_type(ValueTypeDescriptor descriptor, Validator validator) {
    validate_qualified_id(descriptor.id);
    require(!descriptor.meaning.empty(), "type_descriptor", "A type descriptor requires its meaning.");
    canonical(descriptor.to_json());
    validate_constraints(descriptor);
    std::lock_guard lock(mutex_);
    const auto id = descriptor.id;
    require(!entries_.contains(id), "duplicate_type",
            "A value type with that identity is already registered.");
    entries_.emplace(id, Entry{std::move(descriptor), std::move(validator)});
}
ValueTypeRegistry::Entry ValueTypeRegistry::resolve(std::string_view id) const {
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(id);
    if (found == entries_.end())
        throw Error("unknown_value_type", "Value type is unavailable.", {{"type", id}});
    return found->second;
}
ValueTypeDescriptor ValueTypeRegistry::describe(std::string_view id) const {
    return resolve(id).descriptor;
}
PreparedValueType ValueTypeRegistry::prepare(std::string_view id) const {
    const auto entry = resolve(id);
    PreparedValueType prepared;
    prepared.descriptor_ = entry.descriptor;
    prepared.fingerprint_ = entry.descriptor.fingerprint();
    prepared.validator_ = entry.validator;
    return prepared;
}
const ValueTypeDescriptor &PreparedValueType::descriptor() const {
    return descriptor_;
}
const std::string &PreparedValueType::fingerprint() const {
    return fingerprint_;
}
void PreparedValueType::validate(const Value &value) const {
    require(value.type == descriptor_.id, "input_type", "Value does not match its prepared type.");
    require(value.representation() == descriptor_.representation, "representation",
            "Value representation differs from its declared type.");
    if (value.representation() == Representation::Json)
        canonical(value.as_json());
    check_constraints(value, descriptor_.constraints);
    if (validator_)
        validator_(value);
}
void ValueTypeRegistry::validate(const Value &value) const {
    validate_qualified_id(value.type);
    prepare(value.type).validate(value);
}
Json ValueTypeRegistry::capabilities() const {
    std::lock_guard lock(mutex_);
    auto result = Json::array();
    for (const auto &[id, entry] : entries_) {
        (void)id;
        auto descriptor = entry.descriptor.to_json();
        descriptor["fingerprint"] = entry.descriptor.fingerprint();
        descriptor["native_validator"] = static_cast<bool>(entry.validator);
        result.push_back(std::move(descriptor));
    }
    return result;
}
} // namespace gr::kernel
