#pragma once
#include "gr/common.hpp"
#include <functional>
#include <map>
#include <set>

namespace gr::core {
// Record profiles interpret retained documents, distinct from operand value types.
struct RecordTypeDescriptor {
    std::string id;
    std::string description;
    std::set<std::string> capabilities;
};
class TypeRegistry {
  public:
    using Validator = std::function<void(const Json &)>;
    explicit TypeRegistry(bool install_defaults = true);
    void register_type(std::string type, Validator validator);
    void register_type(RecordTypeDescriptor descriptor, Validator validator);
    void register_extension(std::string type, Validator validator);
    bool known(std::string_view type) const;
    bool known_extension(std::string_view type) const;
    bool supports(std::string_view type, std::string_view capability) const;
    Json descriptors() const;
    void validate(const Json &document) const;

  private:
    struct Definition {
        RecordTypeDescriptor descriptor;
        Validator validate;
    };
    std::map<std::string, Definition, std::less<>> validators_;
    std::map<std::string, Validator, std::less<>> extension_validators_;
};
// Register custom registries before concurrent reads; callbacks are trusted/pure.
const TypeRegistry &default_types();
class Record {
  public:
    static Record from_document(Json document, const TypeRegistry & = default_types());
    static Record from_wire(const Json &record, const TypeRegistry & = default_types());
    const Json &document() const noexcept { return document_; }
    const std::string &version() const noexcept { return version_; }
    const std::string &id() const { return document_.at("id").get_ref<const std::string &>(); }
    Json wire() const { return {{"document", document_}, {"version", version_}}; }

  private:
    Record(Json document, std::string version)
        : document_(std::move(document)), version_(std::move(version)) {}
    Json document_;
    std::string version_;
};
Json construct(Json document, const TypeRegistry &registry = default_types());
Json verify(const Json &record, const TypeRegistry &registry = default_types());
// Archive supplies pinned historical versions; historical dependencies are not rebound.
Json validate_snapshot(const Json &snapshot, bool for_mutation = false,
                       const TypeRegistry &registry = default_types());
Json apply(const Json &snapshot, const Json &updates, const TypeRegistry &registry = default_types());

// Bounded, transaction-local reuse of verified immutable wrappers. The registry
// is copied; callbacks must remain pure and stable for the session lifetime.
// This object is not thread-safe. Candidate-wide dependencies are never cached.
class ValidationSession {
  public:
    explicit ValidationSession(const TypeRegistry &registry = default_types());
    Json verify(const Json &record);
    Json validate_snapshot(const Json &snapshot, bool for_mutation = false);
    Json apply(const Json &snapshot, const Json &updates);
    std::size_t canonical_bytes(const Json &record);
    Json statistics() const;

  private:
    struct Entry {
        Json record, report;
        std::size_t bytes;
    };
    TypeRegistry registry_;
    std::map<std::string, Entry, std::less<>> verified_;
    std::size_t bytes_ = 0, checks_ = 0, hits_ = 0;
};
} // namespace gr::core
