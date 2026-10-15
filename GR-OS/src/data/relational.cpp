#include "gr/data/relational.hpp"
#include "gr/kernel/compact_history.hpp"
#include "gr/kernel/projections.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

namespace gr::data::relational {
namespace {
constexpr const char *format = "gr.data.qualified/1";
using Key = std::pair<std::string, std::string>;
void require(bool ok, std::string message) {
    if (!ok)
        throw Error("invalid_value", std::move(message));
}
void stopped(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Qualified incidence operation cancelled");
}
bool digest(const Json &value) {
    if (!value.is_string())
        return false;
    const auto &s = value.get_ref<const std::string &>();
    return s.size() == 71 && s.starts_with("sha256:") &&
           s.find_first_not_of("0123456789abcdef", 7) == std::string::npos;
}
bool physical_revision(const Json &value) {
    if (!value.is_string())
        return false;
    const auto &s = value.get_ref<const std::string &>();
    // Storage revisions are opaque identifiers; they are not semantic digests.
    return !s.empty() && s != "0" && s.size() <= 256 && s.find('\0') == std::string::npos;
}
std::string serialized(const Json &value) {
    auto text = value.dump(-1, ' ', false, Json::error_handler_t::strict);
    require(text.size() <= maximum_payload_bytes, "Qualified value exceeds 64 MiB");
    return text;
}
Json decoded(const kernel::Value &value) {
    require(value.representation() == kernel::Representation::Bytes,
            "Qualified representation requires an immutable byte carrier");
    // Reuse the bounded strict JSON container parser, not the history object model.
    // Its 80-level / 2-million-value limits apply in addition to the byte cap.
    return history::decode_object(value.as_bytes(), maximum_payload_bytes);
}
void family(const Type &type) {
    require(
        (type.family == records_family || type.family == incidence_family || type.family == counts_family) &&
            type.arguments.empty() && type.parameters.is_object() && type.parameters.empty(),
        "Qualified logical types have no arguments or parameters");
}
Json losses() {
    return Json::array(
        {"Incidences count recorded participations, not factual, causal or legal relationships.",
         "Subject references remain designations; grouping does not establish referent identity.",
         "Artifact descriptors are preserved; artifact bytes are not copied into this value.",
         "Neutral references remain in source wrappers and are not participant incidences.",
         "Unknown payload/extension semantics remain opaque; no uncertainty is converted to probability.",
         "Only the selected recording snapshot and direct archive are retained, not the full history DAG."});
}
Json source(Json snapshot) {
    Json result;
    for (auto field : {"revision", "records", "history", "provenance"})
        result[field] = std::move(snapshot.at(field));
    result["archive"] = snapshot.contains("archive") ? std::move(snapshot.at("archive")) : Json::array();
    return result;
}
struct Source {
    Json document;
    std::string wire;
    std::map<std::string, const Json *> current;
    std::map<Key, const Json *> versions;
    Source(Json value, std::string bytes) : document(std::move(value)), wire(std::move(bytes)) {
        for (auto field : {"records", "archive"})
            for (const auto &r : document.at(field)) {
                auto id = r.at("document").at("id").get<std::string>();
                versions.emplace(Key{id, r.at("version").get<std::string>()}, &r);
                if (std::string_view(field) == "records")
                    current.emplace(std::move(id), &r);
            }
    }
};
struct Parsed {
    std::shared_ptr<const Source> source;
    Json view; // Full qualified container minus its independently retained source.
    std::string family;
};
// Preserve the exact v1 sorted-key encoding while inserting the already encoded
// immutable source, without constructing/copying another source JSON tree.
std::string encode_parts(const Json &view, const std::string &source_wire) {
    require(view.is_object() && !view.contains("source"), "Invalid separated qualified view");
    std::string output = "{";
    bool first = true, source_written = false;
    auto append = [&](std::string_view text) {
        require(text.size() <= maximum_payload_bytes - output.size(), "Qualified value exceeds 64 MiB");
        output.append(text);
    };
    auto field = [&](std::string_view key, std::string_view value) {
        if (!first)
            append(",");
        first = false;
        append(Json(key).dump());
        append(":");
        append(value);
    };
    for (auto i = view.begin(); i != view.end(); ++i) {
        if (!source_written && i.key() > "source") {
            field("source", source_wire);
            source_written = true;
        }
        field(i.key(), serialized(i.value()));
    }
    if (!source_written)
        field("source", source_wire);
    append("}");
    return output;
}
Json presented(const Parsed &value) {
    auto result = value.view;
    result["source"] = value.source->document;
    return result;
}
void validate_source(const Json &s, const core::TypeRegistry &registry) {
    require(s.is_object() && s.size() == 5, "Invalid pinned source shape");
    const auto report = core::validate_snapshot(s, false, registry);
    require(report.at("valid").get<bool>(), "Invalid pinned source: " + report.at("errors").dump());
    const auto &p = s.at("provenance");
    require(p.is_object() && p.size() == 5 && p.at("workspace").is_string() &&
                !p.at("workspace").get_ref<const std::string &>().empty() &&
                p.at("workspace").get_ref<const std::string &>().size() <= 512 &&
                physical_revision(p.at("physical_revision")) && digest(p.at("semantic_revision")) &&
                digest(p.at("manifest_version")) && p.at("representation").is_string() &&
                p.at("semantic_revision") == s.at("revision"),
            "Source requires exact physical, semantic and manifest pins");
    (void)canonical(p);
    const auto &h = s.at("history");
    require(h.is_object() && h.size() == 3 && h.at("parents").is_array() && h.at("parents").size() <= 32 &&
                h.at("attribution").is_object() &&
                h.at("interpretation") == "Recording ancestry, not represented event precedence",
            "Source requires recording-history context");
    (void)canonical(h);
    std::set<std::string> parents;
    for (const auto &parent : h.at("parents"))
        require(digest(parent) && parents.insert(parent.get<std::string>()).second,
                "Invalid or duplicate recording parent");
    for (auto field : {"agent", "method", "reason"})
        require(h.at("attribution").contains(field) && h.at("attribution").at(field).is_string() &&
                    !h.at("attribution").at(field).get_ref<const std::string &>().empty(),
                "Recording attribution requires agent, method and reason");
    const Json descriptor = {{"format", "gr.history-node/1"},
                             {"parents", h.at("parents")},
                             {"attribution", h.at("attribution")},
                             {"state_digest", history::snapshot_digest(history::normalized_snapshot(s))}};
    require(s.at("revision") == "sha256:" + sha256(canonical(descriptor)),
            "Selected semantic revision does not identify the retained source state and history");
    std::size_t count = 0;
    for (const auto &r : s.at("records")) {
        const auto &d = r.at("document");
        const auto n = d.contains("participants") ? d.at("participants").size() : 0;
        require(n <= maximum_incidences - count, "Source exceeds 100000 participant incidences");
        count += n;
    }
}
Json role_selection(const Json &roles) {
    require(roles.is_array() && roles.size() <= 1024, "roles must be an array of at most 1024 strings");
    std::set<std::string> sorted;
    for (const auto &role : roles) {
        require(role.is_string() && !role.get_ref<const std::string &>().empty() &&
                    role.get_ref<const std::string &>().size() <= 512,
                "Selected roles must be nonempty strings of at most 512 bytes");
        require(sorted.insert(role.get<std::string>()).second, "Repeated selected role");
    }
    Json out = Json::array();
    for (const auto &role : sorted)
        out.push_back(role);
    return out;
}
Json occurrence_key(const Json &i) {
    return {{"relation", i.at("relation")},
            {"relation_version", i.at("relation_version")},
            {"ordinal", i.at("ordinal")}};
}
Json derived(const Source &source, const Json &query, const Json &roles, bool count,
             const core::TypeRegistry &registry, std::stop_token stop = {}) {
    stopped(stop);
    const auto &s = source.document;
    require(query.is_null() || query.is_object(), "Temporal query must be an object or absent");
    Json temporal = query.is_null() ? Json(nullptr) : core::temporal(s, query, registry);
    std::map<std::string, std::string> classes;
    if (!temporal.is_null() && !temporal.at("cut").is_null())
        for (auto name : {"active", "inactive", "indeterminate", "unscoped"})
            for (const auto &id : temporal.at("cut").at(name))
                classes.emplace(id.get<std::string>(), name);
    const auto selected_roles = roles.is_null() ? Json(nullptr) : role_selection(roles);
    std::set<std::string> selected;
    if (!selected_roles.is_null())
        for (const auto &r : selected_roles)
            selected.insert(r.get<std::string>());
    Json included = Json::array(), excluded = Json::array();
    std::map<std::string, Json> groups;
    std::size_t total = 0;
    for (const auto &r : s.at("records")) {
        stopped(stop);
        const auto &d = r.at("document");
        if (!d.contains("participants"))
            continue;
        const auto id = d.at("id").get<std::string>();
        const std::string temporal_class =
            classes.contains(id) ? classes.at(id) : (query.is_null() ? "not_requested" : "unclassified");
        for (std::size_t ordinal = 0; ordinal < d.at("participants").size(); ++ordinal) {
            if (!(ordinal % 256))
                stopped(stop);
            require(++total <= maximum_incidences, "Source exceeds 100000 participant incidences");
            const auto &participant = d.at("participants").at(ordinal);
            Json i = {{"relation", id}, {"relation_version", r.at("version")}, {"ordinal", ordinal}};
            if (!roles.is_null() && !selected.contains(participant.at("role").get<std::string>())) {
                excluded.push_back(occurrence_key(i));
                continue;
            }
            const auto &ref = participant.at("reference");
            Json resolution = {{"scope", ref.at("scope")}, {"id", ref.at("id")}, {"version", nullptr}};
            if (ref.at("scope") == "record") {
                const auto rid = ref.at("id").get<std::string>();
                const Json *resolved =
                    ref.contains("version")
                        ? source.versions.at(Key{rid, ref.at("version").get<std::string>()})
                        : source.current.at(rid);
                resolution["version"] = resolved->at("version");
            }
            i.update({{"participant", participant},
                      {"resolution", resolution},
                      {"temporal_class", temporal_class}});
            if (count) {
                Json grouping = resolution;
                grouping["role"] = participant.at("role");
                grouping["temporal_class"] = temporal_class;
                const auto key = canonical(grouping);
                if (!groups.contains(key))
                    groups.emplace(key,
                                   Json{{"group", grouping}, {"count", 0}, {"occurrences", Json::array()}});
                auto &g = groups.at(key);
                g["count"] = g.at("count").get<std::size_t>() + 1;
                g["occurrences"].push_back(occurrence_key(i));
            }
            included.push_back(std::move(i));
        }
    }
    Json result = {{"format", format},
                   {"kind", count ? "counts" : "incidence"},
                   {"temporal_query", query},
                   {"temporal", std::move(temporal)},
                   {"roles", selected_roles},
                   {"incidences", std::move(included)},
                   {"excluded_incidences", std::move(excluded)},
                   {"qualifications", losses()}};
    if (count) {
        result["counts"] = Json::array();
        for (auto &[key, value] : groups)
            result["counts"].push_back(std::move(value));
    }
    stopped(stop);
    return result;
}
void attributes(const Json &attrs, std::string_view op) {
    require(attrs.is_object(), "Operator attributes must be an object");
    (void)canonical(attrs);
    if (op == project_operation) {
        require(attrs.empty() ||
                    (attrs.size() == 1 && attrs.contains("temporal") && attrs.at("temporal").is_object()),
                "Incidence accepts only an optional temporal query");
        if (attrs.contains("temporal")) {
            for (auto i = attrs.at("temporal").begin(); i != attrs.at("temporal").end(); ++i)
                require((i.key() == "order_ids" || i.key() == "after") && i.value().is_array(),
                        "Temporal query accepts order_ids and after arrays");
        }
    } else if (op == roles_operation) {
        require(attrs.size() == 1 && attrs.contains("roles"), "Role selection requires exactly roles");
        (void)role_selection(attrs.at("roles"));
    } else
        require(attrs.empty(), "Count accepts no attributes");
}
struct Timer {
    std::atomic<std::uint64_t> &total;
    bool enabled;
    std::chrono::steady_clock::time_point start;
    Timer(std::atomic<std::uint64_t> &t, bool on) : total(t), enabled(on) {
        if (enabled)
            start = std::chrono::steady_clock::now();
    }
    ~Timer() {
        if (enabled)
            total.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                           std::chrono::steady_clock::now() - start)
                                                           .count()),
                            std::memory_order_relaxed);
    }
};
struct Memo {
    struct Entry {
        std::weak_ptr<const void> owner;
        std::shared_ptr<const Parsed> parsed;
        std::size_t charge;
        std::list<const kernel::Bytes *>::iterator recent;
    };
    const std::shared_ptr<const core::TypeRegistry> registry;
    const CacheOptions options;
    mutable std::mutex mutex;
    std::unordered_map<const kernel::Bytes *, Entry> entries;
    std::list<const kernel::Bytes *> recent;
    std::map<std::string, std::weak_ptr<const Source>> sources;
    std::size_t bytes = 0, peak_bytes = 0, epoch = 0;
    std::atomic<std::uint64_t> hits{0}, misses{0}, full_checks{0}, failures{0}, source_hits{0},
        source_checks{0}, decoded_bytes{0}, serialized_bytes{0}, derivations{0}, operator_calls{0},
        evictions{0}, expired{0}, decode_ns{0}, source_ns{0}, derivation_ns{0}, encode_ns{0};
    Memo(std::shared_ptr<const core::TypeRegistry> r, CacheOptions o) : registry(std::move(r)), options(o) {
        require(options.max_entries <= 1024 && options.max_bytes <= 256 * 1024 * 1024,
                "Parsed cache limits exceed 1024 entries or 256 MiB wire-byte charge");
    }
    bool enabled() const { return options.max_entries != 0 && options.max_bytes != 0; }
    void prune() {
        for (auto i = entries.begin(); i != entries.end();) {
            if (i->second.owner.expired()) {
                bytes -= i->second.charge;
                recent.erase(i->second.recent);
                i = entries.erase(i);
                ++expired;
            } else
                ++i;
        }
        for (auto i = sources.begin(); i != sources.end();)
            if (i->second.expired())
                i = sources.erase(i);
            else
                ++i;
    }
    std::string wire(const Json &view, const Source &source) {
        Timer timer(encode_ns, options.profile_timing);
        auto value = encode_parts(view, source.wire);
        serialized_bytes += value.size();
        return value;
    }
    Json derive(const Source &source, const Json &query, const Json &roles, bool count,
                std::stop_token stop = {}) {
        ++derivations;
        Timer timer(derivation_ns, options.profile_timing);
        return derived(source, query, roles, count, *registry, stop);
    }
    std::shared_ptr<const Parsed> get(const Type &type, const kernel::Value &value) {
        family(type);
        require(value.representation() == kernel::Representation::Bytes,
                "Qualified representation requires immutable bytes");
        const auto *key = &value.as_bytes();
        const auto owner = value.owner_token();
        std::size_t observed_epoch;
        {
            std::lock_guard lock(mutex);
            prune();
            observed_epoch = epoch;
            if (auto i = entries.find(key); i != entries.end() && !i->second.owner.owner_before(owner) &&
                                            !owner.owner_before(i->second.owner) &&
                                            i->second.parsed->family == type.family) {
                recent.splice(recent.begin(), recent, i->second.recent);
                ++hits;
                return i->second.parsed;
            }
        }
        ++misses;
        ++full_checks;
        try {
            Json j;
            {
                Timer timer(decode_ns, options.profile_timing);
                j = decoded(value);
                decoded_bytes += value.as_bytes().size();
            }
            require(j.is_object() && j.at("format") == format, "Invalid qualified container format");
            auto candidate = std::move(j.at("source"));
            j.erase("source");
            std::string source_wire;
            {
                Timer timer(encode_ns, options.profile_timing);
                source_wire = serialized(candidate);
                serialized_bytes += source_wire.size();
            }
            // A claim of semantic/physical identity is only a lookup hint. Exact
            // deterministic source bytes must match a previously validated source.
            const auto source_key = canonical(candidate.at("provenance"));
            std::shared_ptr<const Source> selected;
            if (enabled()) {
                std::lock_guard lock(mutex);
                if (auto i = sources.find(source_key); i != sources.end())
                    if (auto found = i->second.lock(); found && found->wire == source_wire) {
                        selected = std::move(found);
                        ++source_hits;
                    }
            }
            if (!selected) {
                ++source_checks;
                Timer timer(source_ns, options.profile_timing);
                validate_source(candidate, *registry);
                selected = std::make_shared<const Source>(std::move(candidate), std::move(source_wire));
            }
            Json expected;
            if (type.family == records_family)
                expected = {{"format", format}, {"kind", "records"}, {"qualifications", losses()}};
            else
                expected =
                    derive(*selected, j.at("temporal_query"), j.at("roles"), type.family == counts_family);
            const auto encoded = wire(expected, *selected);
            require(value.as_bytes().size() == encoded.size() &&
                        std::equal(value.as_bytes().begin(), value.as_bytes().end(), encoded.begin(),
                                   [](std::uint8_t a, char b) { return a == static_cast<std::uint8_t>(b); }),
                    "Qualified payload differs from its retained source or declared derivation");
            auto result = std::make_shared<const Parsed>(Parsed{selected, std::move(j), type.family});
            // Includes wire, detached decoded view's serialized size and source
            // wire, charging shared sources conservatively once for each entry.
            // The source field adds ten bytes (key, colon and separator) to
            // a nonempty detached root. Avoid another complete serialization.
            const auto view_bytes = encoded.size() - selected->wire.size() - 10;
            const auto charge = value.as_bytes().size() + view_bytes + selected->wire.size();
            if (enabled() && charge <= options.max_bytes) {
                std::lock_guard lock(mutex);
                prune();
                if (epoch == observed_epoch) {
                    if (auto old = entries.find(key); old != entries.end()) {
                        bytes -= old->second.charge;
                        recent.erase(old->second.recent);
                        entries.erase(old);
                    }
                    while (!entries.empty() &&
                           (entries.size() >= options.max_entries || bytes > options.max_bytes - charge)) {
                        auto old = entries.find(recent.back());
                        bytes -= old->second.charge;
                        recent.pop_back();
                        entries.erase(old);
                        ++evictions;
                    }
                    recent.push_front(key);
                    try {
                        entries.emplace(key, Entry{owner, result, charge, recent.begin()});
                    } catch (...) {
                        recent.pop_front();
                        throw;
                    }
                    bytes += charge;
                    peak_bytes = std::max(peak_bytes, bytes);
                    sources[source_key] = selected;
                    // Weak source lookup metadata is bounded independently.
                    while (sources.size() > options.max_entries)
                        sources.erase(sources.begin());
                }
            }
            return result;
        } catch (const Json::exception &e) {
            ++failures;
            throw Error("invalid_value", e.what());
        } catch (...) {
            ++failures;
            throw;
        }
    }
    void clear() {
        std::lock_guard lock(mutex);
        entries.clear();
        sources.clear();
        recent.clear();
        bytes = 0;
        ++epoch;
    }
    Json statistics() {
        std::lock_guard lock(mutex);
        prune();
        return {{"format", "gr.data.relational-profile/1"},
                {"enabled", enabled()},
                {"max_entries", options.max_entries},
                {"max_bytes", options.max_bytes},
                {"entries", entries.size()},
                {"charged_bytes", bytes},
                {"peak_charged_bytes", peak_bytes},
                {"source_slots", sources.size()},
                {"hits", hits.load()},
                {"misses", misses.load()},
                {"full_checks", full_checks.load()},
                {"failures", failures.load()},
                {"source_hits", source_hits.load()},
                {"source_checks", source_checks.load()},
                {"decoded_bytes", decoded_bytes.load()},
                {"serialized_bytes", serialized_bytes.load()},
                {"derivations", derivations.load()},
                {"operator_calls", operator_calls.load()},
                {"evictions", evictions.load()},
                {"expired", expired.load()},
                {"timing_enabled", options.profile_timing},
                {"timing_ns",
                 {{"decode", decode_ns.load()},
                  {"source_validation", source_ns.load()},
                  {"derive", derivation_ns.load()},
                  {"encode", encode_ns.load()}}},
                {"charge_scope",
                 "wire + detached view wire + source wire per entry; excludes JSON allocator overhead"}};
    }
};

Json selected_view(const Parsed &input, const Json &requested, std::stop_token stop) {
    auto roles = role_selection(requested);
    if (!input.view.at("roles").is_null()) {
        Json intersection = Json::array();
        for (const auto &r : roles)
            if (std::find(input.view.at("roles").begin(), input.view.at("roles").end(), r) !=
                input.view.at("roles").end())
                intersection.push_back(r);
        roles = std::move(intersection);
    }
    std::set<std::string> selected;
    for (const auto &role : roles)
        selected.insert(role.get<std::string>());
    Json output = input.view;
    output["roles"] = roles;
    output["incidences"] = Json::array();
    for (const auto &i : input.view.at("incidences")) {
        stopped(stop);
        if (selected.contains(i.at("participant").at("role").get<std::string>()))
            output["incidences"].push_back(i);
    }
    output["excluded_incidences"] = Json::array();
    for (const auto &r : input.source->document.at("records")) {
        stopped(stop);
        const auto &d = r.at("document");
        if (!d.contains("participants"))
            continue;
        for (std::size_t ordinal = 0; ordinal < d.at("participants").size(); ++ordinal) {
            if (!(ordinal % 256))
                stopped(stop);
            if (!selected.contains(d.at("participants").at(ordinal).at("role").get<std::string>()))
                output["excluded_incidences"].push_back(
                    {{"relation", d.at("id")}, {"relation_version", r.at("version")}, {"ordinal", ordinal}});
        }
    }
    return output;
}
Json counted_view(const Parsed &input, std::stop_token stop) {
    Json output = input.view;
    output["kind"] = "counts";
    std::map<std::string, Json> groups;
    for (const auto &i : input.view.at("incidences")) {
        stopped(stop);
        auto grouping = i.at("resolution");
        grouping["role"] = i.at("participant").at("role");
        grouping["temporal_class"] = i.at("temporal_class");
        const auto key = canonical(grouping);
        if (!groups.contains(key))
            groups.emplace(key, Json{{"group", grouping}, {"count", 0}, {"occurrences", Json::array()}});
        auto &g = groups.at(key);
        g["count"] = g.at("count").get<std::size_t>() + 1;
        g["occurrences"].push_back(occurrence_key(i));
    }
    output["counts"] = Json::array();
    for (auto &[key, value] : groups)
        output["counts"].push_back(std::move(value));
    return output;
}
} // namespace
struct Session::Impl : Memo {
    using Memo::Memo;
};
Session::Session(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
Session::~Session() = default;
Json Session::inspect(const DataValue &value) const {
    require(value.spec.representation == representation, "Unsupported qualified representation");
    return presented(*impl_->get(value.spec.type, value.payload));
}
Json Session::statistics() const {
    return impl_->statistics();
}
void Session::clear_cache() {
    impl_->clear();
}
Type records_type() {
    return {records_family};
}
Type incidence_type() {
    return {incidence_family};
}
Type counts_type() {
    return {counts_family};
}

DataValue select_history(runtime::TransactionCoordinator &coordinator, const HistoricalSelection &selection) {
    auto snapshot = coordinator.history_snapshot(selection.workspace, selection.semantic_revision,
                                                 selection.physical_revision);
    auto provenance = snapshot.at("provenance");
    provenance["derivation"] = "Eager selected recording-history snapshot; source documents preserved";
    provenance["qualifications"] = losses();
    Json view = {{"format", format}, {"kind", "records"}, {"qualifications", losses()}};
    const auto wire = encode_parts(view, serialized(source(std::move(snapshot))));
    return {{records_type(), representation},
            kernel::Value::bytes(representation, {wire.begin(), wire.end()}),
            std::move(provenance)};
}
Json inspect(const DataValue &value, const core::TypeRegistry &registry) {
    require(value.spec.representation == representation, "Unsupported qualified representation");
    Memo uncached(std::make_shared<const core::TypeRegistry>(registry), {0, 0, false});
    return presented(*uncached.get(value.spec.type, value.payload));
}
std::shared_ptr<Session> install(Library &library, std::shared_ptr<const core::TypeRegistry> supplied,
                                 CacheOptions options) {
    auto registry = std::make_shared<const core::TypeRegistry>(supplied ? *supplied : core::default_types());
    auto context = std::make_shared<Session::Impl>(registry, options);
    for (auto id : {records_family, incidence_family, counts_family}) {
        TypeFamily t;
        t.id = id;
        t.meaning =
            "Qualified selected-history records, participation incidences or structural occurrence counts";
        t.traits = {{"qualified", true}, {"historically_pinned", true}, {"numeric_probability", false}};
        t.validate = family;
        library.register_type(std::move(t));
    }
    Representation repr;
    repr.id = representation;
    repr.carrier = kernel::Representation::Bytes;
    repr.meaning =
        "Bounded deterministic UTF-8 JSON bytes retaining full source wrappers and explicit derivation";
    repr.metadata = {{"maximum_bytes", maximum_payload_bytes},
                     {"maximum_incidences", maximum_incidences},
                     {"record_profiles", registry->descriptors()},
                     {"artifact_bytes_included", false}};
    repr.supports = [](const Type &t) {
        return t.family == records_family || t.family == incidence_family || t.family == counts_family;
    };
    repr.validate = [context](const Type &t, const kernel::Value &v) { (void)context->get(t, v); };
    library.register_representation(std::move(repr));
    for (auto id : {project_operation, roles_operation, count_operation}) {
        const std::string opid = id;
        Operator op;
        op.id = opid;
        op.meaning =
            opid == project_operation
                ? "Project every explicit participant occurrence with exact source and reference versions"
            : opid == roles_operation
                ? "Intersect prior incidence selection with exact role strings, retaining omissions"
                : "Count selected occurrences by reference scope, ID, resolved version, role and temporal "
                  "class";
        op.qualifications = losses();
        op.metadata = {{"record_profiles", registry->descriptors()}, {"status", "EXPERIMENTAL"}};
        op.infer = [opid](const std::vector<ValueSpec> &inputs, const Json &attrs) {
            attributes(attrs, opid);
            require(inputs.size() == 1 && inputs.front().representation == representation,
                    "Qualified operator requires one qualified byte value");
            family(inputs.front().type);
            require(inputs.front().type.family ==
                        (opid == project_operation ? records_family : incidence_family),
                    "Qualified operator input logical family mismatch");
            return std::vector<ValueSpec>{
                {opid == count_operation ? counts_type() : incidence_type(), representation}};
        };
        op.specialize = [opid, context, meaning = op.meaning](const auto &, const auto &, const Json &attrs) {
            Specialization result;
            result.contract.meaning = meaning;
            result.contract.preconditions = {"Validated pinned source and exact qualified representation"};
            result.contract.invariants = {"Source wrappers and history context preserved",
                                          "No storage reads or mutation"};
            Provider provider;
            provider.descriptor.id = "gr.data.cpu." + opid.substr(std::string("gr.data.").size());
            provider.descriptor.backend_ids = {"cpu.sequential", "cpu.threaded"};
            provider.descriptor.device_kinds = {"cpu"};
            provider.descriptor.memory_domains = {"host"};
            provider.descriptor.thread_safe = true;
            provider.function = [opid, attrs, context](const kernel::Invocation &args, std::stop_token stop) {
                stopped(stop);
                ++context->operator_calls;
                const auto input =
                    context->get(opid == project_operation ? records_type() : incidence_type(), args.at(0));
                Json output;
                if (opid == project_operation)
                    output = context->derive(*input->source, attrs.value("temporal", Json(nullptr)), nullptr,
                                             false, stop);
                else {
                    ++context->derivations;
                    Timer timer(context->derivation_ns, context->options.profile_timing);
                    output = opid == roles_operation ? selected_view(*input, attrs.at("roles"), stop)
                                                     : counted_view(*input, stop);
                }
                const auto wire = context->wire(output, *input->source);
                auto value = kernel::Value::bytes(representation, {wire.begin(), wire.end()});
                stopped(stop);
                return kernel::Invocation{std::move(value)};
            };
            result.providers.push_back(std::move(provider));
            return result;
        };
        library.register_operator(std::move(op));
    }
    return std::shared_ptr<Session>(new Session(std::move(context)));
}
} // namespace gr::data::relational
