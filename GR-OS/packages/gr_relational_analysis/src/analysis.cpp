#include <algorithm>
#include <gr/kernel/compact_history.hpp>
#include <gr_relational_analysis/analysis.hpp>
#include <map>
#include <numeric>
#include <set>

namespace gr_relational_analysis {
using gr::Json;
namespace rel = gr::data::relational;
namespace {
void need(bool condition, const std::string &message, const char *code = "analysis_contract") {
    if (!condition)
        throw gr::Error(code, message);
}
void stopped(std::stop_token stop) {
    if (stop.stop_requested())
        throw gr::Error("execution_cancelled", "Relational analysis cancelled");
}
void limits(const Limits &l) {
    need(l.max_vertices > 0 && l.max_vertices <= 110000 && l.max_occurrences > 0 &&
             l.max_occurrences <= rel::maximum_incidences && l.max_snapshot_bytes > 0 &&
             l.max_snapshot_bytes <= rel::maximum_payload_bytes,
         "Invalid analysis resource limits", "analysis_limit");
}
bool digest(const Json &j) {
    if (!j.is_string())
        return false;
    const auto &s = j.get_ref<const std::string &>();
    return s.size() == 71 && s.starts_with("sha256:") &&
           s.find_first_not_of("0123456789abcdef", 7) == std::string::npos;
}
std::string text(const Json &j, std::size_t limit = 512) {
    need(j.is_string() && !j.get_ref<const std::string &>().empty() &&
             j.get_ref<const std::string &>().size() <= limit &&
             j.get_ref<const std::string &>().find('\0') == std::string::npos,
         "Expected bounded nonempty string");
    return j.get<std::string>();
}
std::size_t count(const Json &j, std::size_t bound) {
    need(j.is_number_integer() && (!j.is_number_unsigned() ? j.get<std::int64_t>() >= 0 : true),
         "Expected nonnegative integer");
    const auto n = j.get<std::uint64_t>();
    need(n <= bound, "Integer exceeds analysis limit", "analysis_limit");
    return static_cast<std::size_t>(n);
}
void fields(const Json &j, std::initializer_list<const char *> names) {
    need(j.is_object() && j.size() == names.size(), "Unexpected object fields");
    for (auto name : names)
        need(j.contains(name), "Missing field: " + std::string(name));
}
void tree(const Json &j, std::size_t depth, std::size_t &nodes, std::size_t &bytes, std::size_t limit) {
    need(depth <= 80 && ++nodes <= 2000000, "Analysis container complexity limit", "analysis_limit");
    auto add = [&](std::size_t n) {
        need(n <= limit && bytes <= limit - n, "Analysis container byte limit", "analysis_limit");
        bytes += n;
    };
    add(1);
    if (j.is_string())
        add(j.get_ref<const std::string &>().size());
    else if (j.is_object()) {
        for (auto i = j.begin(); i != j.end(); ++i) {
            add(i.key().size());
            tree(i.value(), depth + 1, nodes, bytes, limit);
        }
    } else if (j.is_array()) {
        for (const auto &v : j)
            tree(v, depth + 1, nodes, bytes, limit);
    } else if (j.is_number_unsigned())
        need(j.get<std::uint64_t>() <= 9007199254740991ULL, "Unsafe integer");
    else if (j.is_number_integer())
        need(j.get<std::int64_t>() >= -9007199254740991LL && j.get<std::int64_t>() <= 9007199254740991LL,
             "Unsafe integer");
    else
        need(j.is_null() || j.is_boolean(), "Unsupported JSON number/value");
}
std::string wire(const Json &j, std::size_t limit) {
    std::size_t nodes = 0, bytes = 0;
    tree(j, 0, nodes, bytes, limit);
    auto s = j.dump(-1, ' ', false, Json::error_handler_t::strict);
    need(s.size() <= limit, "Encoded analysis container exceeds limit", "analysis_limit");
    return s;
}
Json losses() {
    return Json::array(
        {"Topology describes recorded participant incidences, not verified social relationships or "
         "causality.",
         "Owner and target roles are distinct vertices even when they designate the same record/version.",
         "Subject designations are grouped by spelling without establishing real-world identity.",
         "Portable snapshots omit complete source wrappers, direct archive, artifact bytes, full history DAG "
         "and computational provenance details.",
         "Source digests and revision pins are custody references, not authentication or proof of omitted "
         "derivation.",
         "Roles and temporal classes remain qualifications; recording ancestry is not represented event "
         "precedence."});
}
Json vertex_identity(const Vertex &v) {
    return {{"format", "gr.analysis.vertex/1"},
            {"kind", to_string(v.kind)},
            {"scope", v.scope},
            {"id", v.id},
            {"version", v.version ? Json(*v.version) : Json(nullptr)}};
}
std::string vertex_key(const Vertex &v) {
    return "sha256:" + gr::sha256(gr::canonical(vertex_identity(v)));
}
Json occurrence_identity(const Occurrence &o) {
    return {{"relation", o.relation}, {"relation_version", o.relation_version}, {"ordinal", o.ordinal}};
}
std::string occurrence_key(const Occurrence &o) {
    auto identity = occurrence_identity(o);
    identity["format"] = "gr.analysis.occurrence/1";
    return "sha256:" + gr::sha256(gr::canonical(identity));
}
Json vertex_json(const Vertex &v) {
    auto j = vertex_identity(v);
    j.erase("format");
    j["key"] = v.key;
    return j;
}
Json occurrence_json(const Occurrence &o) {
    auto j = occurrence_identity(o);
    j.update({{"key", o.key},
              {"owner", o.owner},
              {"target", o.target},
              {"role", o.role},
              {"temporal_class", o.temporal_class},
              {"participant", o.participant}});
    return j;
}
using Classes = std::map<std::string, std::string, std::less<>>;
Classes classes(const Json &context) {
    Classes out;
    const auto &t = context.at("temporal");
    if (!t.is_null() && t.contains("cut") && !t.at("cut").is_null()) {
        const auto &cut = t.at("cut");
        for (auto name : {"active", "inactive", "indeterminate", "unscoped"}) {
            need(cut.contains(name) && cut.at(name).is_array(), "Malformed temporal cut");
            for (const auto &id : cut.at(name))
                need(out.emplace(text(id), name).second, "Temporal cut repeats a classified record");
        }
    }
    return out;
}
std::string temporal_class(const Json &context, const Classes &c, const std::string &id) {
    auto it = c.find(id);
    return it == c.end() ? (context.at("temporal_query").is_null() ? "not_requested" : "unclassified")
                         : it->second;
}
Json body(const std::vector<Vertex> &vertices, const std::vector<Occurrence> &occurrences,
          const Json &context, const Json &identity) {
    Json v = Json::array(), o = Json::array();
    for (const auto &x : vertices)
        v.push_back(vertex_json(x));
    for (const auto &x : occurrences)
        o.push_back(occurrence_json(x));
    return {{"format", snapshot_format},
            {"identity", identity},
            {"context", context},
            {"vertices", std::move(v)},
            {"occurrences", std::move(o)}};
}
void context_valid(const Json &c, const Limits &l) {
    fields(c, {"pins", "history", "roles", "temporal_query", "temporal", "excluded_incidences",
               "source_record_count", "archive_count", "qualifications", "losses"});
    const auto &p = c.at("pins");
    fields(p, {"workspace", "physical_revision", "semantic_revision", "manifest_version", "representation"});
    text(p.at("workspace"));
    text(p.at("physical_revision"), 256);
    text(p.at("representation"));
    need(p.at("physical_revision") != "0" && digest(p.at("semantic_revision")) &&
             digest(p.at("manifest_version")),
         "Invalid source pins");
    (void)gr::canonical(p);
    const auto &h = c.at("history");
    fields(h, {"parents", "attribution", "interpretation"});
    need(h.at("parents").is_array() && h.at("parents").size() <= 32 && h.at("attribution").is_object() &&
             h.at("interpretation") == "Recording ancestry, not represented event precedence",
         "Invalid recording context");
    std::set<std::string> parents;
    for (const auto &v : h.at("parents"))
        need(digest(v) && parents.insert(v.get<std::string>()).second, "Invalid recording parent");
    for (auto f : {"agent", "method", "reason"})
        text(h.at("attribution").at(f), 1024 * 1024);
    (void)gr::canonical(h);
    const auto &roles = c.at("roles");
    need(roles.is_null() || (roles.is_array() && roles.size() <= 1024), "Invalid roles selection");
    std::string previous;
    if (!roles.is_null())
        for (const auto &r : roles) {
            auto role = text(r);
            need(previous.empty() || previous < role, "Roles must be sorted and unique");
            previous = std::move(role);
        }
    need(c.at("temporal_query").is_null() || c.at("temporal_query").is_object(), "Invalid temporal query");
    need(c.at("temporal_query").is_null() == c.at("temporal").is_null(),
         "Missing or spurious temporal report");
    if (!c.at("temporal").is_null())
        need(c.at("temporal").is_object() && c.at("temporal").contains("cut"), "Invalid temporal report");
    (void)classes(c);
    need(c.at("excluded_incidences").is_array() && c.at("excluded_incidences").size() <= l.max_occurrences,
         "Excluded incidence limit", "analysis_limit");
    count(c.at("source_record_count"), 10000);
    count(c.at("archive_count"), 10000);
    need(c.at("qualifications").is_array() && c.at("losses") == losses(),
         "Missing qualification/loss inventory");
    for (const auto &q : c.at("qualifications"))
        text(q, 1024 * 1024);
}
void identity_valid(const Json &id) {
    fields(id, {"source_spec", "source_payload_digest", "source_provenance_digest", "source_value_id"});
    need(gr::data::ValueSpec::from_json(id.at("source_spec")) ==
             gr::data::ValueSpec{rel::incidence_type(), rel::representation},
         "Snapshot requires qualified incidence source");
    need(digest(id.at("source_payload_digest")) && digest(id.at("source_provenance_digest")) &&
             digest(id.at("source_value_id")),
         "Invalid source identity digest");
    auto base = id;
    base.erase("source_value_id");
    base["format"] = "gr.analysis.source-value/1";
    need(id.at("source_value_id") == "sha256:" + gr::sha256(gr::canonical(base)),
         "Source value identity mismatch", "analysis_integrity");
}
Json source_identity(const gr::data::DataValue &value) {
    Json result = {{"source_spec", value.spec.to_json()},
                   {"source_payload_digest", value.payload.content_digest()},
                   {"source_provenance_digest", "sha256:" + gr::sha256(gr::canonical(value.provenance))}};
    auto base = result;
    base["format"] = "gr.analysis.source-value/1";
    result["source_value_id"] = "sha256:" + gr::sha256(gr::canonical(base));
    return result;
}
} // namespace
struct NetworkSnapshot::Impl {
    std::vector<Vertex> vertices;
    std::vector<Occurrence> occurrences;
    Json context, identity;
    std::string id;
    bool derived = false;
    Limits limits;
};
struct NetworkView::Impl {
    rel::QualifiedView source;
    NetworkSnapshot snapshot;
};
std::string to_string(VertexKind kind) {
    return kind == VertexKind::participant_owner ? "participant_owner" : "participant_target";
}
NetworkSnapshot::NetworkSnapshot(std::shared_ptr<const Impl> p) : impl_(std::move(p)) {}
NetworkSnapshot::operator bool() const noexcept {
    return bool(impl_);
}
const std::vector<Vertex> &NetworkSnapshot::vertices() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->vertices;
}
const std::vector<Occurrence> &NetworkSnapshot::occurrences() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->occurrences;
}
const Json &NetworkSnapshot::context() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->context;
}
const Json &NetworkSnapshot::identity() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->identity;
}
const std::string &NetworkSnapshot::id() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->id;
}
Limits NetworkSnapshot::resource_limits() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->limits;
}
std::string NetworkSnapshot::validation_basis() const {
    need(bool(impl_), "Empty snapshot");
    return impl_->derived ? "source_derived" : "structural_only";
}
Json NetworkSnapshot::to_json() const {
    auto result = body(vertices(), occurrences(), context(), identity());
    result["snapshot_id"] = id();
    return result;
}
std::string NetworkSnapshot::encode() const {
    need(bool(impl_), "Empty snapshot");
    return wire(to_json(), impl_->limits.max_snapshot_bytes);
}
NetworkSnapshot NetworkSnapshot::decode(std::span<const std::uint8_t> bytes, Limits l) {
    limits(l);
    need(bytes.size() <= l.max_snapshot_bytes, "Snapshot input exceeds byte budget", "analysis_limit");
    try {
        return from_json(gr::history::decode_object(bytes, l.max_snapshot_bytes), l);
    } catch (const Json::exception &e) {
        throw gr::Error("analysis_contract", e.what());
    }
}
NetworkSnapshot NetworkSnapshot::from_json(const Json &j, Limits l) {
    limits(l);
    try {
        (void)wire(j, l.max_snapshot_bytes); // Check before copying caller-owned trees.
        fields(j, {"format", "identity", "context", "vertices", "occurrences", "snapshot_id"});
        need(j.at("format") == snapshot_format && digest(j.at("snapshot_id")),
             "Invalid snapshot format/identity");
        context_valid(j.at("context"), l);
        identity_valid(j.at("identity"));
        need(j.at("vertices").is_array() && j.at("vertices").size() <= l.max_vertices &&
                 j.at("occurrences").is_array() && j.at("occurrences").size() <= l.max_occurrences,
             "Snapshot topology exceeds limits", "analysis_limit");
        auto p = std::make_shared<Impl>();
        p->limits = l;
        p->context = j.at("context");
        p->identity = j.at("identity");
        std::set<std::string> keys, owners;
        std::string previous;
        for (const auto &x : j.at("vertices")) {
            fields(x, {"key", "kind", "scope", "id", "version"});
            Vertex v;
            v.key = text(x.at("key"));
            v.scope = text(x.at("scope"));
            v.id = text(x.at("id"));
            need(x.at("kind") == "participant_owner" || x.at("kind") == "participant_target",
                 "Unknown vertex kind");
            v.kind = x.at("kind") == "participant_owner" ? VertexKind::participant_owner
                                                         : VertexKind::participant_target;
            need(v.scope == "record" || v.scope == "subject", "Unknown reference scope");
            if (!x.at("version").is_null()) {
                need(digest(x.at("version")), "Invalid record version");
                v.version = x.at("version").get<std::string>();
            }
            need((v.scope == "record") == bool(v.version), "Record/subject version mismatch");
            if (v.kind == VertexKind::participant_owner)
                need(v.scope == "record" && owners.insert(v.id).second,
                     "Owner must identify one current record version");
            need(v.key == vertex_key(v) && keys.insert(v.key).second &&
                     (previous.empty() || previous < v.key),
                 "Invalid, repeated or unsorted vertex identity", "analysis_integrity");
            previous = v.key;
            p->vertices.push_back(std::move(v));
        }
        need(owners.size() <= count(p->context.at("source_record_count"), 10000),
             "Owner count exceeds source inventory");
        const auto classification = classes(p->context);
        std::set<std::string> occurrences;
        std::map<std::string, std::set<std::size_t>> ordinals;
        std::vector<std::size_t> target_degrees(p->vertices.size());
        previous.clear();
        for (const auto &x : j.at("occurrences")) {
            fields(x, {"key", "relation", "relation_version", "ordinal", "owner", "target", "role",
                       "temporal_class", "participant"});
            Occurrence o;
            o.key = text(x.at("key"));
            o.relation = text(x.at("relation"));
            need(digest(x.at("relation_version")), "Invalid occurrence owner version");
            o.relation_version = x.at("relation_version").get<std::string>();
            o.ordinal = count(x.at("ordinal"), rel::maximum_incidences - 1);
            o.owner = count(x.at("owner"), l.max_vertices);
            o.target = count(x.at("target"), l.max_vertices);
            need(o.owner < p->vertices.size() && o.target < p->vertices.size(),
                 "Occurrence vertex index out of range");
            const auto &owner = p->vertices[o.owner], &target = p->vertices[o.target];
            need(owner.kind == VertexKind::participant_owner &&
                     target.kind == VertexKind::participant_target && owner.id == o.relation &&
                     *owner.version == o.relation_version,
                 "Occurrence owner/target mismatch");
            o.role = text(x.at("role"));
            o.temporal_class = text(x.at("temporal_class"));
            o.participant = x.at("participant");
            need(o.participant.is_object() && o.participant.contains("role") &&
                     o.participant.at("role") == o.role && o.participant.contains("reference"),
                 "Participant role missing/mismatched");
            (void)gr::canonical(o.participant);
            if (o.participant.contains("orientation")) {
                const auto &orientation = o.participant.at("orientation");
                need(orientation == "in" || orientation == "out" || orientation == "undirected",
                     "Unknown participant orientation");
            }
            const auto &ref = o.participant.at("reference");
            // The retained L5 reference envelope permits opaque extra fields.
            // Preserve them; only scope/id/version have topology meaning here.
            need(ref.is_object() && ref.at("scope") == target.scope && ref.at("id") == target.id,
                 "Participant target mismatch");
            if (ref.contains("version"))
                need(target.version && ref.at("version") == *target.version,
                     "Pinned participant version mismatch");
            if (!p->context.at("roles").is_null())
                need(std::find(p->context.at("roles").begin(), p->context.at("roles").end(), Json(o.role)) !=
                         p->context.at("roles").end(),
                     "Selected role absent from selection");
            need(o.temporal_class == temporal_class(p->context, classification, o.relation),
                 "Occurrence temporal class differs from report");
            need(o.key == occurrence_key(o) && occurrences.insert(o.key).second &&
                     (previous.empty() || previous < o.key),
                 "Invalid, repeated or unsorted occurrence identity", "analysis_integrity");
            previous = o.key;
            ++target_degrees[o.target];
            ordinals[owner.key].insert(o.ordinal);
            p->occurrences.push_back(std::move(o));
        }
        std::set<std::string> excluded;
        for (const auto &x : p->context.at("excluded_incidences")) {
            fields(x, {"relation", "relation_version", "ordinal"});
            Occurrence o;
            o.relation = text(x.at("relation"));
            need(digest(x.at("relation_version")), "Invalid excluded owner version");
            o.relation_version = x.at("relation_version").get<std::string>();
            o.ordinal = count(x.at("ordinal"), rel::maximum_incidences - 1);
            const auto k = occurrence_key(o);
            need(!occurrences.contains(k) && excluded.insert(k).second,
                 "Repeated or selected excluded occurrence");
            Vertex v{"", VertexKind::participant_owner, "record", o.relation, o.relation_version};
            need(keys.contains(vertex_key(v)), "Excluded occurrence owner missing");
            ordinals[vertex_key(v)].insert(o.ordinal);
        }
        need(excluded.size() <= l.max_occurrences - p->occurrences.size(), "Total occurrence budget exceeded",
             "analysis_limit");
        for (std::size_t i = 0; i < p->vertices.size(); ++i)
            if (p->vertices[i].kind == VertexKind::participant_target)
                need(target_degrees[i] > 0, "Unreferenced target vertex");
        for (const auto &[owner, indices] : ordinals)
            need(!indices.empty() && *indices.rbegin() + 1 == indices.size(),
                 "Occurrence inventory has ordinal gaps");
        auto content = j;
        content.erase("snapshot_id");
        p->id = "sha256:" + gr::sha256(wire(content, l.max_snapshot_bytes));
        need(j.at("snapshot_id") == p->id, "Snapshot digest mismatch", "analysis_integrity");
        return NetworkSnapshot(std::move(p));
    } catch (const Json::exception &e) {
        throw gr::Error("analysis_contract", e.what());
    }
}
NetworkView::NetworkView(std::shared_ptr<const Impl> p) : impl_(std::move(p)) {}
NetworkView::operator bool() const noexcept {
    return bool(impl_);
}
const rel::QualifiedView &NetworkView::source() const {
    need(bool(impl_), "Empty network view");
    return impl_->source;
}
NetworkSnapshot NetworkView::snapshot() const {
    need(bool(impl_), "Empty network view");
    return impl_->snapshot;
}
const Json &NetworkView::identity() const {
    return snapshot().identity();
}
NetworkView analyze(const gr::data::DataValue &value, std::shared_ptr<rel::Session> session, Limits l,
                    std::stop_token stop) {
    limits(l);
    stopped(stop);
    need(bool(session), "Analysis requires a relational validation session");
    need(value.spec == gr::data::ValueSpec{rel::incidence_type(), rel::representation},
         "Expected qualified incidence input");
    auto source = session->view(value);
    stopped(stop);
    auto p = std::make_shared<NetworkSnapshot::Impl>();
    p->limits = l;
    p->derived = true;
    p->identity = source_identity(value);
    const auto &s = source.source(), &d = source.derivation();
    p->context = {{"pins", s.at("provenance")},
                  {"history", s.at("history")},
                  {"roles", d.at("roles")},
                  {"temporal_query", d.at("temporal_query")},
                  {"temporal", d.at("temporal")},
                  {"excluded_incidences", d.at("excluded_incidences")},
                  {"source_record_count", s.at("records").size()},
                  {"archive_count", s.at("archive").size()},
                  {"qualifications", d.at("qualifications")},
                  {"losses", losses()}};
    need(d.at("incidences").size() <= l.max_occurrences &&
             d.at("excluded_incidences").size() <= l.max_occurrences - d.at("incidences").size(),
         "Incidence count exceeds analysis budget", "analysis_limit");
    std::map<std::string, Vertex> vertices;
    auto insert = [&](Vertex v) {
        v.key = vertex_key(v);
        auto key = v.key;
        if (!vertices.contains(key)) {
            need(vertices.size() < l.max_vertices, "Vertex budget exceeded", "analysis_limit");
            vertices.emplace(key, std::move(v));
        }
        return key;
    };
    for (const auto &r : s.at("records")) {
        stopped(stop);
        if (r.at("document").contains("participants"))
            insert({"", VertexKind::participant_owner, "record", r.at("document").at("id"),
                    r.at("version").get<std::string>()});
    }
    for (const auto &i : d.at("incidences")) {
        stopped(stop);
        const auto &r = i.at("resolution");
        insert({"", VertexKind::participant_target, r.at("scope"), r.at("id"),
                r.at("version").is_null() ? std::optional<std::string>{}
                                          : std::optional<std::string>{r.at("version").get<std::string>()}});
    }
    std::map<std::string, std::size_t> indices;
    for (auto &[key, v] : vertices) {
        indices.emplace(key, p->vertices.size());
        p->vertices.push_back(std::move(v));
    }
    for (const auto &i : d.at("incidences")) {
        stopped(stop);
        Occurrence o;
        o.relation = i.at("relation");
        o.relation_version = i.at("relation_version");
        o.ordinal = i.at("ordinal");
        o.participant = i.at("participant");
        o.role = o.participant.at("role");
        o.temporal_class = i.at("temporal_class");
        const auto &r = i.at("resolution");
        o.owner = indices.at(
            vertex_key({"", VertexKind::participant_owner, "record", o.relation, o.relation_version}));
        o.target = indices.at(vertex_key(
            {"", VertexKind::participant_target, r.at("scope"), r.at("id"),
             r.at("version").is_null() ? std::optional<std::string>{}
                                       : std::optional<std::string>{r.at("version").get<std::string>()}}));
        o.key = occurrence_key(o);
        p->occurrences.push_back(std::move(o));
    }
    std::sort(p->occurrences.begin(), p->occurrences.end(),
              [](const auto &a, const auto &b) { return a.key < b.key; });
    auto content = body(p->vertices, p->occurrences, p->context, p->identity);
    p->id = "sha256:" + gr::sha256(wire(content, l.max_snapshot_bytes));
    content["snapshot_id"] = p->id;
    (void)wire(content, l.max_snapshot_bytes);
    stopped(stop);
    return NetworkView(std::make_shared<const NetworkView::Impl>(
        NetworkView::Impl{std::move(source), NetworkSnapshot(std::move(p))}));
}
Json summarize(const NetworkSnapshot &snapshot, std::stop_token stop) {
    stopped(stop);
    const auto &v = snapshot.vertices();
    const auto &o = snapshot.occurrences();
    std::vector<std::size_t> parents(v.size()), degrees(v.size());
    std::iota(parents.begin(), parents.end(), 0);
    std::vector<std::set<std::size_t>> neighbors(v.size());
    auto find = [&](std::size_t x) {
        while (parents[x] != x) {
            parents[x] = parents[parents[x]];
            x = parents[x];
        }
        return x;
    };
    std::map<std::string, std::size_t> roles, times;
    for (const auto &e : o) {
        stopped(stop);
        ++degrees[e.owner];
        ++degrees[e.target];
        neighbors[e.owner].insert(e.target);
        neighbors[e.target].insert(e.owner);
        auto a = find(e.owner), b = find(e.target);
        if (a != b)
            parents[std::max(a, b)] = std::min(a, b);
        ++roles[e.role];
        ++times[e.temporal_class];
    }
    Json per_vertex = Json::array(), components = Json::array();
    std::map<std::size_t, Json> groups;
    std::size_t owner_count = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        stopped(stop);
        bool owner = v[i].kind == VertexKind::participant_owner;
        owner_count += owner;
        per_vertex.push_back({{"vertex", v[i].key},
                              {"kind", to_string(v[i].kind)},
                              {"occurrence_degree", degrees[i]},
                              {"distinct_neighbor_degree", neighbors[i].size()}});
        auto root = find(i);
        if (!groups.contains(root))
            groups.emplace(root, Json{{"key", v[root].key},
                                      {"owners", 0},
                                      {"targets", 0},
                                      {"occurrences", 0},
                                      {"vertices", Json::array()}});
        auto &g = groups.at(root);
        auto field = owner ? "owners" : "targets";
        g[field] = g.at(field).get<std::size_t>() + 1;
        g["vertices"].push_back(v[i].key);
        if (owner)
            g["occurrences"] = g.at("occurrences").get<std::size_t>() + degrees[i];
    }
    for (auto &[key, g] : groups)
        components.push_back(std::move(g));
    Json result = {{"format", "gr.analysis.incidence-summary/1"},
                   {"snapshot_id", snapshot.id()},
                   {"source_value_id", snapshot.identity().at("source_value_id")},
                   {"validation_basis", snapshot.validation_basis()},
                   {"pins", snapshot.context().at("pins")},
                   {"owners", owner_count},
                   {"targets", v.size() - owner_count},
                   {"occurrences", o.size()},
                   {"excluded_occurrences", snapshot.context().at("excluded_incidences").size()},
                   {"role_counts", roles},
                   {"temporal_counts", times},
                   {"degrees", std::move(per_vertex)},
                   {"components", std::move(components)},
                   {"qualifications", losses()}};
    (void)wire(result, snapshot.resource_limits().max_snapshot_bytes);
    stopped(stop);
    return result;
}
Json summarize(const NetworkView &n, std::stop_token stop) {
    return summarize(n.snapshot(), stop);
}
Json compare(const NetworkView &left, const NetworkView &right, std::stop_token stop) {
    stopped(stop);
    auto a = left.snapshot(), b = right.snapshot();
    need(a.context().at("pins").at("workspace") == b.context().at("pins").at("workspace"),
         "Cross-workspace comparison requires an explicit identity mapping", "analysis_selection");
    need(a.context().at("roles") == b.context().at("roles") &&
             a.context().at("temporal_query") == b.context().at("temporal_query"),
         "Comparison requires identical role and temporal selections", "analysis_selection");
    std::map<std::string, std::string> ar, br;
    for (const auto &r : left.source().source().at("records")) {
        stopped(stop);
        ar.emplace(r.at("document").at("id"), r.at("version"));
    }
    for (const auto &r : right.source().source().at("records")) {
        stopped(stop);
        br.emplace(r.at("document").at("id"), r.at("version"));
    }
    Json records = {{"added", Json::array()}, {"removed", Json::array()}, {"changed", Json::array()}};
    for (const auto &[id, version] : ar) {
        stopped(stop);
        if (!br.contains(id))
            records["removed"].push_back({{"id", id}, {"version", version}});
        else if (br.at(id) != version)
            records["changed"].push_back(
                {{"id", id}, {"left_version", version}, {"right_version", br.at(id)}});
    }
    for (const auto &[id, version] : br) {
        stopped(stop);
        if (!ar.contains(id))
            records["added"].push_back({{"id", id}, {"version", version}});
    }
    std::map<std::string, const Occurrence *> ao, bo;
    for (const auto &o : a.occurrences()) {
        stopped(stop);
        ao.emplace(o.key, &o);
    }
    for (const auto &o : b.occurrences()) {
        stopped(stop);
        bo.emplace(o.key, &o);
    }
    auto semantic_occurrence = [](const NetworkSnapshot &s, const Occurrence &o) {
        return Json{{"occurrence", occurrence_identity(o)},
                    {"participant", o.participant},
                    {"target", vertex_identity(s.vertices().at(o.target))},
                    {"temporal_class", o.temporal_class}};
    };
    Json occurrences = {{"added", Json::array()}, {"removed", Json::array()}, {"changed", Json::array()}};
    for (const auto &[key, o] : ao) {
        stopped(stop);
        if (!bo.contains(key))
            occurrences["removed"].push_back(occurrence_identity(*o));
        else {
            auto av = semantic_occurrence(a, *o), bv = semantic_occurrence(b, *bo.at(key));
            if (av != bv)
                occurrences["changed"].push_back(
                    {{"key", key}, {"left", std::move(av)}, {"right", std::move(bv)}});
        }
    }
    for (const auto &[key, o] : bo) {
        stopped(stop);
        if (!ao.contains(key))
            occurrences["added"].push_back(occurrence_identity(*o));
    }
    auto as = summarize(a, stop), bs = summarize(b, stop);
    Json delta;
    for (auto field : {"owners", "targets", "occurrences", "excluded_occurrences"})
        delta[field] = bs.at(field).get<std::int64_t>() - as.at(field).get<std::int64_t>();
    delta["components"] = static_cast<std::int64_t>(bs.at("components").size()) -
                          static_cast<std::int64_t>(as.at("components").size());
    Json result = {
        {"format", "gr.analysis.recording-comparison/1"},
        {"left_snapshot", a.id()},
        {"right_snapshot", b.id()},
        {"left_identity", a.identity()},
        {"right_identity", b.identity()},
        {"left_pins", a.context().at("pins")},
        {"right_pins", b.context().at("pins")},
        {"records", std::move(records)},
        {"occurrences", std::move(occurrences)},
        {"summary_delta", std::move(delta)},
        {"qualifications",
         Json::array(
             {"Record-ID alignment concerns recorded versions, not real-world identity or relationship "
              "persistence.",
              "Occurrence keys retain owner version and ordinal; a revised owner has new occurrence keys.",
              "Unchanged occurrence keys can resolve different unpinned target versions or temporal "
              "classifications.",
              "Differences in recording states do not establish represented event order or causation."})}};
    (void)wire(result,
               std::min(a.resource_limits().max_snapshot_bytes, b.resource_limits().max_snapshot_bytes));
    stopped(stop);
    return result;
}
gr::data::ValueSpec summary_spec() {
    return {{summary_family}, summary_representation};
}
namespace {
Json validate_summary(const gr::kernel::Value &v, Limits l) {
    need(v.representation() == gr::kernel::Representation::Bytes, "Summary requires bytes");
    auto j = gr::history::decode_object(v.as_bytes(), l.max_snapshot_bytes);
    fields(j, {"format", "source_payload_digest", "lineage_scope", "validation_basis", "pins", "owners",
               "targets", "occurrences", "excluded_occurrences", "role_counts", "temporal_counts", "degrees",
               "components", "qualifications"});
    need(j.at("format") == "gr.analysis.prepared-summary/1" && digest(j.at("source_payload_digest")) &&
             j.at("lineage_scope") ==
                 "Carrier only; complete input computational lineage belongs to Program.materialize" &&
             j.at("validation_basis") == "qualified_carrier",
         "Invalid summary identity or basis");
    const auto owners = count(j.at("owners"), 10000), targets = count(j.at("targets"), l.max_vertices),
               edges = count(j.at("occurrences"), l.max_occurrences);
    need(owners + targets <= l.max_vertices, "Summary vertex budget", "analysis_limit");
    need(count(j.at("excluded_occurrences"), l.max_occurrences) <= l.max_occurrences - edges,
         "Summary occurrence budget", "analysis_limit");
    need(j.at("degrees").is_array() && j.at("degrees").size() == owners + targets &&
             j.at("components").is_array(),
         "Summary inventory mismatch");
    for (auto field : {"role_counts", "temporal_counts"}) {
        need(j.at(field).is_object(), "Invalid histogram");
        std::size_t sum = 0;
        for (const auto &n : j.at(field)) {
            auto value = count(n, l.max_occurrences);
            need(value <= l.max_occurrences - sum, "Histogram overflow");
            sum += value;
        }
        need(sum == edges, "Histogram does not conserve occurrences");
    }
    std::set<std::string> vertices;
    std::map<std::string, const Json *> degree_rows;
    std::size_t owner_degrees = 0, target_degrees = 0, owner_rows = 0;
    for (const auto &d : j.at("degrees")) {
        fields(d, {"vertex", "kind", "occurrence_degree", "distinct_neighbor_degree"});
        need(digest(d.at("vertex")) && vertices.insert(d.at("vertex").get<std::string>()).second,
             "Duplicate degree vertex");
        const auto n = count(d.at("occurrence_degree"), edges);
        const auto distinct = count(d.at("distinct_neighbor_degree"), n);
        need((n == 0) == (distinct == 0), "Invalid distinct degree");
        need(d.at("kind") == "participant_owner" || d.at("kind") == "participant_target",
             "Invalid degree kind");
        if (d.at("kind") == "participant_owner") {
            ++owner_rows;
            need(n <= edges - owner_degrees, "Owner degree total exceeds occurrences");
            owner_degrees += n;
        } else {
            need(n > 0, "Summary contains an unreferenced target");
            need(n <= edges - target_degrees, "Target degree total exceeds occurrences");
            target_degrees += n;
        }
        degree_rows.emplace(d.at("vertex").get<std::string>(), &d);
    }
    need(owner_rows == owners && owner_degrees == edges && target_degrees == edges,
         "Degree conservation failure");
    std::set<std::string> covered;
    std::size_t component_owners = 0, component_targets = 0, component_edges = 0;
    for (const auto &c : j.at("components")) {
        fields(c, {"key", "owners", "targets", "occurrences", "vertices"});
        need(digest(c.at("key")) && c.at("vertices").is_array() && !c.at("vertices").empty(),
             "Invalid component");
        auto co = count(c.at("owners"), owners), ct = count(c.at("targets"), targets),
             ce = count(c.at("occurrences"), edges);
        need(c.at("vertices").size() == co + ct, "Component size mismatch");
        std::size_t actual_owners = 0, actual_targets = 0, owner_edges = 0, target_edges = 0;
        std::string previous;
        for (const auto &k : c.at("vertices")) {
            need(k.is_string() && vertices.contains(k.get<std::string>()) &&
                     covered.insert(k.get<std::string>()).second,
                 "Component partition mismatch");
            const auto key = k.get<std::string>();
            need(previous.empty() || previous < key, "Component vertices must be sorted");
            previous = key;
            const auto &row = *degree_rows.at(key);
            const auto degree = row.at("occurrence_degree").get<std::size_t>();
            if (row.at("kind") == "participant_owner") {
                ++actual_owners;
                need(degree <= ce - owner_edges, "Component owner degree total exceeds occurrences");
                owner_edges += degree;
                need(row.at("distinct_neighbor_degree").get<std::size_t>() <= ct,
                     "Owner neighbor count exceeds component targets");
            } else {
                ++actual_targets;
                need(degree <= ce - target_edges, "Component target degree total exceeds occurrences");
                target_edges += degree;
                need(row.at("distinct_neighbor_degree").get<std::size_t>() <= co,
                     "Target neighbor count exceeds component owners");
            }
        }
        need(actual_owners == co && actual_targets == ct && owner_edges == ce && target_edges == ce,
             "Component counts differ from degree rows");
        need(ce > 0 || (co == 1 && ct == 0), "Zero-edge component must be one isolated owner");
        need(c.at("key") == c.at("vertices").front(), "Component key mismatch");
        need(co <= owners - component_owners && ct <= targets - component_targets &&
                 ce <= edges - component_edges,
             "Component totals exceed summary");
        component_owners += co;
        component_targets += ct;
        component_edges += ce;
    }
    need(covered == vertices && component_owners == owners && component_targets == targets &&
             component_edges == edges,
         "Component conservation failure");
    need(j.at("qualifications") == losses(), "Missing summary qualifications");
    (void)gr::canonical(j.at("pins"));
    const auto encoded = wire(j, l.max_snapshot_bytes);
    need(encoded.size() == v.as_bytes().size() &&
             std::equal(encoded.begin(), encoded.end(), v.as_bytes().begin(),
                        [](char a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; }),
         "Summary bytes are not deterministic encoding");
    return j;
}
} // namespace
gr::Json inspect_summary(const gr::data::DataValue &v, Limits l) {
    limits(l);
    need(v.spec == summary_spec(), "Expected incidence summary value");
    try {
        return validate_summary(v.payload, l);
    } catch (const Json::exception &e) {
        throw gr::Error("analysis_contract", e.what());
    }
}
void install(gr::data::Library &library, std::shared_ptr<rel::Session> session, Limits l) {
    limits(l);
    need(bool(session), "Summary registration requires relational Session");
    gr::data::TypeFamily family;
    family.id = summary_family;
    family.meaning = "Exact structural statistics of a qualified recorded-incidence bipartite network";
    family.validate = [](const gr::data::Type &t) {
        need(t == summary_spec().type, "Summary type has no parameters or arguments");
    };
    library.register_type(std::move(family));
    gr::data::Representation repr;
    repr.id = summary_representation;
    repr.carrier = gr::kernel::Representation::Bytes;
    repr.meaning = "Deterministic bounded JSON summary with explicit source references and structural "
                   "conservation checks";
    repr.metadata = {
        {"max_vertices", l.max_vertices},
        {"max_occurrences", l.max_occurrences},
        {"max_bytes", l.max_snapshot_bytes},
        {"validation", "Structural only; source input is required to independently recompute statistics"}};
    repr.supports = [](const auto &t) { return t == summary_spec().type; };
    repr.validate = [l](const auto &, const auto &v) { (void)validate_summary(v, l); };
    library.register_representation(std::move(repr));
    gr::data::Operator op;
    op.id = summary_operation;
    op.meaning =
        "Occurrence and distinct-neighbor degrees, role/temporal counts and undirected incidence components";
    op.qualifications = losses();
    op.metadata = {{"method", "gr.analysis.bipartite-incidence@1"},
                   {"max_vertices", l.max_vertices},
                   {"max_occurrences", l.max_occurrences}};
    op.infer = [](const auto &inputs, const Json &attrs) {
        need(attrs.is_object() && attrs.empty(), "Summary accepts no attributes");
        need(inputs.size() == 1 &&
                 inputs.front() == gr::data::ValueSpec{rel::incidence_type(), rel::representation},
             "Expected qualified incidence input");
        return std::vector<gr::data::ValueSpec>{summary_spec()};
    };
    op.specialize = [session, l](const auto &, const auto &, const Json &) {
        gr::data::Specialization s;
        s.contract.meaning = "Exact bounded recorded-incidence structural summary";
        s.contract.invariants = {"No historical mutation or external effects",
                                 "Repeated occurrences conserved", "Source selection and losses explicit"};
        gr::data::Provider p;
        p.descriptor.id = "gr.analysis.cpu.incidence-summary@1";
        p.descriptor.backend_ids = {"cpu.sequential", "cpu.threaded"};
        p.descriptor.device_kinds = {"cpu"};
        p.descriptor.memory_domains = {"host"};
        p.descriptor.thread_safe = true;
        p.function = [session, l](const gr::kernel::Invocation &args, std::stop_token stop) {
            gr::data::DataValue v{{rel::incidence_type(), rel::representation}, args.at(0), Json::object()};
            auto network = analyze(v, session, l, stop);
            auto result = summarize(network, stop);
            result["format"] = "gr.analysis.prepared-summary/1";
            result.erase("snapshot_id");
            result.erase("source_value_id");
            result["source_payload_digest"] = v.payload.content_digest();
            result["lineage_scope"] =
                "Carrier only; complete input computational lineage belongs to Program.materialize";
            result["validation_basis"] = "qualified_carrier";
            auto encoded = wire(result, l.max_snapshot_bytes);
            stopped(stop);
            return gr::kernel::Invocation{
                gr::kernel::Value::bytes(summary_representation, {encoded.begin(), encoded.end()})};
        };
        s.providers.push_back(std::move(p));
        return s;
    };
    library.register_operator(std::move(op));
}
} // namespace gr_relational_analysis
