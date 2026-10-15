#include "gr/kernel/types.hpp"
#include "internal.hpp"
#include <algorithm>
#include <deque>
#include <set>
#include <string_view>

namespace gr::core {
namespace detail {
void require(bool condition, const char *code, const std::string &message) {
    if (!condition)
        throw Error(code, message);
}
std::string text(const Json &value, const char *key) {
    require(value.is_object() && value.contains(key) && value[key].is_string(), "schema",
            std::string("Expected string: ") + key);
    auto result = value[key].get<std::string>();
    require(!result.empty() && result.size() <= 512 && result.find('\0') == std::string::npos, "schema",
            std::string("Empty, overlong or NUL-containing identifier: ") + key);
    return result;
}
bool digest(const std::string &s) {
    return s.size() == 71 && s.starts_with("sha256:") &&
           s.find_first_not_of("0123456789abcdef", 7) == std::string::npos;
}
void array_field(const Json &j, const char *field) {
    if (j.contains(field))
        require(j[field].is_array(), "schema", std::string(field) + " must be an array");
}
void ref_shape(const Json &ref) {
    text(ref, "id");
    auto scope = text(ref, "scope");
    require(scope == "record" || scope == "subject", "schema", "Reference scope must be record or subject");
    if (ref.contains("version")) {
        require(scope == "record" && digest(text(ref, "version")), "schema",
                "Only record references can pin a content version");
    }
}
void envelope(const Json &d) {
    canonical(d);
    require(text(d, "format") == "gr.record/1", "format", "Unsupported record format");
    text(d, "id");
    // The retained gr.record/1 envelope keeps its original permissive identifier
    // check. Strict new registration/construction must not make old opaque bytes
    // unreadable or change their content identity.
    const auto type = text(d, "type");
    require(type.find('.') != std::string::npos && type.find('@') != std::string::npos, "type",
            "Retained v1 type must include a namespace and version marker");
    require(d.contains("payload") && d["payload"].is_object(), "schema", "payload must be an object");
    require(d.contains("attribution") && d["attribution"].is_object(), "schema",
            "Explicit attribution is required");
    text(d["attribution"], "agent");
    text(d["attribution"], "method");
    if (d.contains("required"))
        require(d["required"].is_boolean(), "schema", "required must be boolean");
    if (d.contains("qualifiers"))
        require(d["qualifiers"].is_object(), "schema", "qualifiers must be an object");
    for (auto key : {"roles", "references", "participants", "artifacts", "extensions"})
        array_field(d, key);
    if (d.contains("roles"))
        for (const auto &role : d["roles"])
            require(role.is_string() && !role.get_ref<const std::string &>().empty(), "schema",
                    "Record roles must be nonempty strings");
    if (d.contains("references"))
        for (const auto &r : d["references"])
            ref_shape(r);
    if (d.contains("participants"))
        for (const auto &p : d["participants"]) {
            text(p, "role");
            require(p.contains("reference"), "schema", "Participant reference required");
            ref_shape(p["reference"]);
            if (p.contains("orientation")) {
                auto o = text(p, "orientation");
                require(o == "in" || o == "out" || o == "undirected", "schema",
                        "Invalid participant orientation");
            }
        }
    if (d.contains("order")) {
        text(d["order"], "before");
        text(d["order"], "after");
    }
    if (d.contains("temporal")) {
        require(d["temporal"].is_object(), "schema", "temporal must be an object");
        for (auto endpoint : {"start", "end"}) {
            require(d["temporal"].contains(endpoint), "schema",
                    "Both temporal endpoints are required; use unknown explicitly");
            const auto &e = d["temporal"][endpoint];
            auto kind = text(e, "kind");
            require(kind == "event" || kind == "unknown" || kind == "unbounded", "schema",
                    "Invalid temporal endpoint kind");
            if (kind == "event")
                text(e, "id");
        }
    }
    if (d.contains("artifacts"))
        for (const auto &a : d["artifacts"]) {
            require(digest(text(a, "sha256")), "schema", "Artifact digest must be sha256:<hex>");
            text(a, "media_type");
            require(a.contains("byte_length") && a["byte_length"].is_number_integer() &&
                        a["byte_length"] >= 0,
                    "schema", "Artifact byte_length must be a nonnegative integer");
        }
    if (d.contains("extensions"))
        for (const auto &e : d["extensions"]) {
            text(e, "type");
            require(e.contains("required") && e["required"].is_boolean() && e.contains("data"), "schema",
                    "Extensions need required and data");
        }
    static constexpr std::string_view fields[] = {
        "format", "id",         "type",         "payload", "attribution", "required",  "qualifiers",
        "roles",  "references", "participants", "order",   "temporal",    "artifacts", "extensions"};
    for (auto it = d.begin(); it != d.end(); ++it)
        require(std::find(std::begin(fields), std::end(fields), it.key()) != std::end(fields), "schema",
                "Unknown envelope field; use payload or extensions: " + it.key());
}
const Json &records(const Json &snapshot) {
    require(snapshot.is_object() && snapshot.contains("records") && snapshot["records"].is_array(),
            "snapshot", "Snapshot records must be an array");
    text(snapshot, "revision");
    if (snapshot.contains("staging"))
        text(snapshot["staging"], "base_revision");
    require(snapshot["records"].size() <= 10000, "snapshot_limit", "At most 10000 current records supported");
    if (snapshot.contains("archive"))
        require(snapshot["archive"].is_array() && snapshot["archive"].size() <= 10000, "snapshot_limit",
                "Archive must be an array of at most 10000 versions");
    return snapshot["records"];
}
Index index(const Json &snapshot, ValidationSession *session) {
    Index result;
    std::size_t total_bytes = 0;
    auto add = [&](const Json &r, bool current) {
        require(r.is_object() && r.contains("document"), "record", "Expected record wrapper");
        const auto id = text(r["document"], "id"), v = text(r, "version");
        auto size = session ? session->canonical_bytes(r) : canonical(r["document"]).size();
        require(size <= 32 * 1024 * 1024 - total_bytes, "snapshot_limit",
                "Snapshot and supplied archive exceed 32 MiB of canonical documents");
        total_bytes += size;
        if (current)
            require(result.current.emplace(id, &r).second, "duplicate_id",
                    "Snapshot repeats current record ID: " + id);
        auto [it, inserted] = result.versions.emplace(std::make_pair(id, v), &r);
        require(inserted || *it->second == r, "version_collision",
                "Same record version has different content");
    };
    for (const auto &r : records(snapshot))
        add(r, true);
    if (snapshot.contains("archive"))
        for (const auto &r : snapshot["archive"])
            add(r, false);
    return result;
}
Json unknown(const Json &d, const TypeRegistry &registry) {
    Json result = Json::array();
    if (!registry.known(d.at("type").get<std::string>()))
        result.push_back(d.at("type"));
    if (d.contains("extensions"))
        for (const auto &e : d["extensions"])
            if (!registry.known_extension(e.at("type").get<std::string>()))
                result.push_back(e.at("type"));
    return result;
}
void require_valid(const Json &s, const TypeRegistry &registry) {
    auto report = validate_snapshot(s, false, registry);
    require(report["valid"].get<bool>(), "invalid_snapshot",
            "Snapshot fails structural validation: " + report["errors"].dump());
}
} // namespace detail
using namespace detail;
TypeRegistry::TypeRegistry(bool install_defaults) {
    if (!install_defaults)
        return;
    for (auto type : {"gr.entity@1", "gr.state@1", "gr.process@1", "gr.property@1", "gr.assertion@1"})
        register_type(
            RecordTypeDescriptor{
                type, "Generic operational record profile; payload assertions are not adjudicated", {}},
            [](const Json &) {});
    register_type(RecordTypeDescriptor{"gr.event@1",
                                       "Operational event anchor; not a claim of instantaneous occurrence",
                                       {"event-anchor"}},
                  [](const Json &) {});
    register_type("gr.relation@1", [](const Json &d) {
        require(d.contains("participants") && !d["participants"].empty(), "relation",
                "Relation requires at least one participant incidence");
    });
    register_type(RecordTypeDescriptor{"gr.order@1", "Attributed strict precedence claim", {"precedence"}},
                  [](const Json &d) {
                      require(d.contains("order"), "order",
                              "Order claim requires before and after event IDs");
                  });
}
void TypeRegistry::register_type(std::string type, Validator validator) {
    register_type(RecordTypeDescriptor{std::move(type), "Custom record profile", {}}, std::move(validator));
}
void TypeRegistry::register_type(RecordTypeDescriptor descriptor, Validator validator) {
    kernel::validate_qualified_id(descriptor.id);
    require(!descriptor.description.empty() && descriptor.description.size() <= 4096 && bool(validator),
            "type", "Profile description and validator required");
    for (const auto &trait : descriptor.capabilities)
        require(trait == "event-anchor" || trait == "precedence", "type",
                "Unknown kernel record capability: " + trait);
    const auto id = descriptor.id;
    require(validators_.emplace(id, Definition{std::move(descriptor), std::move(validator)}).second,
            "duplicate_type", "Type already registered");
}
bool TypeRegistry::known(std::string_view type) const {
    return validators_.find(type) != validators_.end();
}
void TypeRegistry::register_extension(std::string type, Validator validator) {
    kernel::validate_qualified_id(type);
    require(bool(validator), "type", "Extension validator required");
    require(extension_validators_.emplace(std::move(type), std::move(validator)).second, "duplicate_type",
            "Extension type already registered");
}
bool TypeRegistry::known_extension(std::string_view type) const {
    return extension_validators_.find(type) != extension_validators_.end();
}
bool TypeRegistry::supports(std::string_view type, std::string_view capability) const {
    auto found = validators_.find(type);
    return found != validators_.end() &&
           found->second.descriptor.capabilities.contains(std::string(capability));
}
Json TypeRegistry::descriptors() const {
    Json records = Json::array(), extensions = Json::array();
    for (const auto &[id, definition] : validators_)
        records.push_back({{"id", id},
                           {"description", definition.descriptor.description},
                           {"capabilities", definition.descriptor.capabilities}});
    for (const auto &[id, validator] : extension_validators_) {
        (void)validator;
        extensions.push_back({{"id", id}, {"interpretation", "trusted registered data validator"}});
    }
    return {{"record_profiles", records},
            {"extensions", extensions},
            {"scope", "Structural/profile validation; no truth or authority certification"}};
}
const TypeRegistry &default_types() {
    static const TypeRegistry registry;
    return registry;
}
void TypeRegistry::validate(const Json &d) const {
    envelope(d);
    auto it = validators_.find(d.at("type").get<std::string>());
    if (it != validators_.end()) {
        if (it->second.descriptor.capabilities.contains("precedence"))
            require(d.contains("order"), "order", "A precedence profile requires an order account");
        it->second.validate(d);
    }
    if (d.contains("extensions"))
        for (const auto &e : d["extensions"]) {
            auto ext = extension_validators_.find(e.at("type").get<std::string>());
            if (ext != extension_validators_.end())
                ext->second(e.at("data"));
        }
}
Record Record::from_document(Json document, const TypeRegistry &registry) {
    kernel::validate_qualified_id(text(document, "type"));
    if (document.contains("extensions") && document["extensions"].is_array())
        for (const auto &extension : document["extensions"])
            kernel::validate_qualified_id(text(extension, "type"));
    registry.validate(document);
    auto version = "sha256:" + sha256(canonical(document));
    return Record(std::move(document), std::move(version));
}
Record Record::from_wire(const Json &record, const TypeRegistry &registry) {
    auto report = verify(record, registry);
    require(report["valid"].get<bool>(), "invalid_record", report["errors"].dump());
    return Record(record.at("document"), record.at("version").get<std::string>());
}
Json construct(Json document, const TypeRegistry &registry) {
    return Record::from_document(std::move(document), registry).wire();
}
Json verify(const Json &record, const TypeRegistry &registry) {
    Json errors = Json::array(), opaque = Json::array();
    try {
        require(record.is_object() && record.size() == 2 && record.contains("document"), "record",
                "Record wrapper must contain only document and version");
        const auto version = text(record, "version");
        registry.validate(record.at("document"));
        require(version == "sha256:" + sha256(canonical(record.at("document"))), "digest_mismatch",
                "Record content does not match version");
        opaque = unknown(record.at("document"), registry);
    } catch (const Error &e) {
        errors.push_back({{"code", e.code}, {"message", e.what()}});
    } catch (const std::exception &e) {
        errors.push_back({{"code", "validator_error"}, {"message", e.what()}});
    }
    return {
        {"valid", errors.empty()},
        {"semantic_complete", errors.empty() && opaque.empty()},
        {"scope",
         "envelope, content digest, registered validators; no truth, signature or authority verification"},
        {"unknown_types", opaque},
        {"errors", errors}};
}
ValidationSession::ValidationSession(const TypeRegistry &registry) : registry_(registry) {}
namespace {
// Json's numeric equality intentionally equates 1 and 1.0. Canonical record
// metadata rejects floating values, so that equality cannot authorize memo reuse.
// Signed/unsigned integer containers may differ after parsing the same v1 bytes.
bool same_wire_value(const Json &a, const Json &b) {
    if (a.is_number_integer() && b.is_number_integer()) {
        if (a.is_number_unsigned() && !b.is_number_unsigned())
            return b.get<std::int64_t>() >= 0 &&
                   a.get<std::uint64_t>() == static_cast<std::uint64_t>(b.get<std::int64_t>());
        if (!a.is_number_unsigned() && b.is_number_unsigned())
            return a.get<std::int64_t>() >= 0 &&
                   static_cast<std::uint64_t>(a.get<std::int64_t>()) == b.get<std::uint64_t>();
        return a == b;
    }
    if (a.type() != b.type() || a.is_number_float() || a.size() != b.size())
        return false;
    if (a.is_object()) {
        auto left = a.begin(), right = b.begin();
        for (; left != a.end(); ++left, ++right)
            if (left.key() != right.key() || !same_wire_value(left.value(), right.value()))
                return false;
        return true;
    }
    if (a.is_array()) {
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!same_wire_value(a[i], b[i]))
                return false;
        return true;
    }
    return a == b;
}
} // namespace
Json ValidationSession::verify(const Json &record) {
    const auto key = record.is_object() && record.contains("version") && record.at("version").is_string()
                         ? record.at("version").get<std::string>()
                         : std::string{};
    auto found = verified_.find(key);
    if (found != verified_.end() && same_wire_value(found->second.record, record)) {
        ++hits_;
        return found->second.report;
    }
    ++checks_;
    auto report = core::verify(record, registry_);
    if (report.at("valid").get<bool>() && found == verified_.end()) {
        const auto size = canonical(record.at("document")).size();
        // A retained-payload bound, not a process-memory guarantee.
        constexpr std::size_t limit = 64 * 1024 * 1024;
        if (size <= limit - bytes_ && verified_.size() < 20000) {
            verified_.emplace(key, Entry{record, report, size});
            bytes_ += size;
        }
    }
    return report;
}
std::size_t ValidationSession::canonical_bytes(const Json &record) {
    (void)verify(record);
    if (record.is_object() && record.contains("version") && record.at("version").is_string()) {
        const auto found = verified_.find(record.at("version").get_ref<const std::string &>());
        if (found != verified_.end() && same_wire_value(found->second.record, record))
            return found->second.bytes;
    }
    return canonical(record.at("document")).size();
}
Json ValidationSession::statistics() const {
    return {{"verified_wrappers", checks_},
            {"memo_hits", hits_},
            {"retained_document_bytes", bytes_},
            {"retained_wrappers", verified_.size()}};
}
Json ValidationSession::validate_snapshot(const Json &snapshot, bool for_mutation) {
    const auto &registry = registry_;
    Json errors = Json::array(), opaque = Json::array();
    try {
        const auto idx = index(snapshot, this);
        for (const auto &[key, r] : idx.versions) {
            auto report = verify(*r);
            if (!report["valid"].get<bool>())
                errors.push_back({{"id", key.first}, {"errors", report["errors"]}});
            for (const auto &t : report["unknown_types"])
                opaque.push_back({{"id", key.first}, {"type", t}});
        }
        require(errors.empty(), "invalid_record", "One or more supplied versions failed verification");
        auto resolve = [&](const Json &ref) {
            if (ref.at("scope") == "subject")
                return;
            const auto id = ref.at("id").get<std::string>();
            if (ref.contains("version"))
                require(idx.versions.contains({id, ref["version"].get<std::string>()}), "missing_version",
                        "Pinned record version unavailable: " + id);
            else
                require(idx.current.contains(id), "missing_reference", "Missing current record: " + id);
        };
        auto event = [&](const std::string &id) {
            require(idx.current.contains(id), "missing_event", "Missing temporal event: " + id);
            require(registry.supports(idx.current.at(id)->at("document").at("type").get<std::string>(),
                                      "event-anchor"),
                    "event_type",
                    "Temporal anchor requires an explicitly registered event-anchor capability: " + id);
        };
        // Only current records impose admission dependencies. Archived documents retain their original
        // claims.
        for (const auto &[id, r] : idx.current) {
            const auto &d = r->at("document");
            if (for_mutation) {
                require(!d.value("required", false) || registry.known(d.at("type").get<std::string>()),
                        "unknown_required_type", "Required type unavailable: " + id);
                if (d.contains("extensions"))
                    for (const auto &e : d["extensions"])
                        require(!e["required"].get<bool>() ||
                                    registry.known_extension(e["type"].get<std::string>()),
                                "unknown_required_extension", "Required extension unavailable: " + id);
            }
            if (d.contains("references"))
                for (const auto &ref : d["references"])
                    resolve(ref);
            if (d.contains("participants"))
                for (const auto &p : d["participants"])
                    resolve(p["reference"]);
            if (d.contains("order")) {
                event(d["order"]["before"]);
                event(d["order"]["after"]);
            }
            if (d.contains("temporal"))
                for (auto endpoint : {"start", "end"}) {
                    const auto &e = d["temporal"][endpoint];
                    if (e["kind"] == "event")
                        event(e["id"]);
                }
        }
    } catch (const Error &e) {
        errors.push_back({{"code", e.code}, {"message", e.what()}});
    } catch (const std::exception &e) {
        errors.push_back({{"code", "schema"}, {"message", e.what()}});
    }
    return {{"valid", errors.empty()},
            {"semantic_complete", errors.empty() && opaque.empty()},
            {"unknown_types", opaque},
            {"errors", errors},
            {"for_mutation", for_mutation}};
}
Json ValidationSession::apply(const Json &snapshot, const Json &updates) {
    require(updates.is_array() && updates.size() <= 1000, "updates",
            "Updates must be an array of at most 1000 records");
    // Required interpretation also protects the source: replacing an unknown
    // required type with a known type must not bypass its admission constraints.
    auto before = validate_snapshot(snapshot, true);
    require(before["valid"].get<bool>(), "invalid_snapshot", before["errors"].dump());
    Json candidate = snapshot;
    if (!candidate.contains("archive"))
        candidate["archive"] = Json::array();
    std::set<std::string> changed;
    std::map<std::string, std::size_t> positions;
    for (std::size_t i = 0; i < candidate["records"].size(); ++i)
        positions.emplace(candidate["records"][i]["document"]["id"].get<std::string>(), i);
    std::set<std::pair<std::string, std::string>> retained;
    for (const auto &r : candidate["archive"])
        retained.emplace(r["document"]["id"].get<std::string>(), r["version"].get<std::string>());
    for (const auto &r : updates) {
        auto report = verify(r);
        require(report["valid"].get<bool>(), "invalid_record", report["errors"].dump());
        const auto id = r["document"]["id"].get<std::string>();
        require(changed.insert(id).second, "duplicate_update", "A batch may update each record ID only once");
        auto position = positions.find(id);
        if (position != positions.end()) {
            auto &old = candidate["records"][position->second];
            if (old != r && retained.emplace(id, old["version"].get<std::string>()).second)
                candidate["archive"].push_back(old);
            old = r;
        } else {
            positions.emplace(id, candidate["records"].size());
            candidate["records"].push_back(r);
        }
    }
    auto after = validate_snapshot(candidate, true);
    require(after["valid"].get<bool>(), "admission", after["errors"].dump());
    if (candidate["records"] != snapshot["records"]) {
        candidate.erase("history");
        candidate["staging"] = {{"base_revision", snapshot.contains("staging")
                                                      ? snapshot["staging"].at("base_revision")
                                                      : snapshot["revision"]},
                                {"interpretation", "Unpublished candidate; revision denotes its base"}};
    }
    return candidate;
}
Json validate_snapshot(const Json &snapshot, bool for_mutation, const TypeRegistry &registry) {
    return ValidationSession(registry).validate_snapshot(snapshot, for_mutation);
}
Json apply(const Json &snapshot, const Json &updates, const TypeRegistry &registry) {
    return ValidationSession(registry).apply(snapshot, updates);
}
} // namespace gr::core
