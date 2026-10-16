#include <algorithm>
#include <array>
#include <gr_visualization/visualization.hpp>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <streambuf>

namespace gr_visualization {
using gr::Error;
using gr::Json;
namespace {
constexpr std::array<const char *, 6> classes{"active",   "inactive",     "indeterminate",
                                              "unscoped", "unclassified", "not_requested"};
constexpr std::size_t hard_bytes = 64 * 1024 * 1024;
constexpr std::uint64_t max_coordinate = 1000000;
void cancelled(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Visualization cancelled");
}
void require(bool condition, const char *message) {
    if (!condition)
        throw Error("visualization_contract", message);
}
void limits_valid(const Limits &l) {
    require(l.max_vertices > 0 && l.max_vertices <= 110000 && l.max_occurrences > 0 &&
                l.max_occurrences <= 100000 && l.max_scene_bytes > 0 && l.max_scene_bytes <= hard_bytes &&
                l.max_svg_bytes > 0 && l.max_svg_bytes <= hard_bytes,
            "Invalid visualization limits");
}
bool known_class(const std::string &s) {
    return std::find(classes.begin(), classes.end(), s) != classes.end();
}
// Decode strictly, and admit only XML 1.0 characters. Reject rather than silently
// changing identifiers containing invalid UTF-8 or non-XML characters.
std::vector<std::size_t> boundaries(std::string_view text, bool collect = true) {
    std::vector<std::size_t> out{0};
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        std::uint32_t cp = 0;
        std::size_t n = 0;
        if (first < 0x80) {
            cp = first;
            n = 1;
        } else if (first >= 0xc2 && first <= 0xdf) {
            cp = first & 0x1f;
            n = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            cp = first & 0x0f;
            n = 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            cp = first & 0x07;
            n = 4;
        } else
            throw Error("visualization_text", "Invalid UTF-8 in visualization text");
        if (n > text.size() - i)
            throw Error("visualization_text", "Truncated UTF-8 in visualization text");
        for (std::size_t j = 1; j < n; ++j) {
            const auto b = static_cast<unsigned char>(text[i + j]);
            if ((b & 0xc0) != 0x80)
                throw Error("visualization_text", "Invalid UTF-8 continuation");
            cp = (cp << 6) | (b & 0x3f);
        }
        if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10ffff ||
            (cp >= 0xd800 && cp <= 0xdfff))
            throw Error("visualization_text", "Noncanonical UTF-8 in visualization text");
        if (!(cp == 9 || cp == 10 || cp == 13 || (cp >= 0x20 && cp <= 0xd7ff) ||
              (cp >= 0xe000 && cp <= 0xfffd) || (cp >= 0x10000 && cp <= 0x10ffff)))
            throw Error("visualization_text", "Character is not permitted in XML 1.0");
        i += n;
        if (collect)
            out.push_back(i);
    }
    return out;
}
std::string xml(std::string_view text) {
    (void)boundaries(text, false);
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '\"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out += c;
        }
    }
    return out;
}
void xml_stream(std::ostream &out, std::string_view text, std::stop_token stop) {
    (void)boundaries(text, false);
    for (std::size_t i = 0; i < text.size(); ++i) {
        if ((i & 4095) == 0)
            cancelled(stop);
        switch (text[i]) {
        case '&':
            out << "&amp;";
            break;
        case '<':
            out << "&lt;";
            break;
        case '>':
            out << "&gt;";
            break;
        case '\"':
            out << "&quot;";
            break;
        case '\'':
            out << "&apos;";
            break;
        default:
            out.put(text[i]);
        }
    }
}
std::pair<std::string, bool> label(const std::string &text, std::size_t maximum) {
    auto b = boundaries(text);
    bool truncated = b.size() - 1 > maximum;
    auto result = truncated ? text.substr(0, b[maximum - 1]) + "\xe2\x80\xa6" : text;
    for (auto &c : result)
        if (c == '\n' || c == '\r' || c == '\t')
            c = ' ';
    return {std::move(result), truncated};
}
class BoundedBuffer : public std::streambuf {
  public:
    explicit BoundedBuffer(std::size_t limit) : limit_(limit) {}
    std::string data;

  protected:
    std::streamsize xsputn(const char *s, std::streamsize n) override {
        if (n < 0 || static_cast<std::uint64_t>(n) > limit_ - data.size())
            throw Error("visualization_limit", "Serialized visualization exceeds byte limit");
        data.append(s, static_cast<std::size_t>(n));
        return n;
    }
    int_type overflow(int_type c) override {
        if (!traits_type::eq_int_type(c, traits_type::eof())) {
            const char value = traits_type::to_char_type(c);
            xsputn(&value, 1);
        }
        return traits_type::not_eof(c);
    }

  private:
    std::size_t limit_;
};
std::string encoded(const Json &j, std::size_t cap) {
    BoundedBuffer buffer(cap);
    std::ostream stream(&buffer);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream << j;
    return std::move(buffer.data);
}
void normalize(std::optional<std::vector<std::string>> &selection, bool temporal) {
    if (!selection)
        return;
    require(selection->size() <= 4096, "Too many filter entries");
    for (const auto &entry : *selection) {
        require(!entry.empty() && entry.size() <= 4096, "Invalid filter entry length");
        (void)boundaries(entry);
        require(!temporal || known_class(entry), "Unknown temporal class filter");
    }
    std::sort(selection->begin(), selection->end());
    selection->erase(std::unique(selection->begin(), selection->end()), selection->end());
}
ProjectionOptions normalized(ProjectionOptions options) {
    require(options.layout == Layout::incidence_columns || options.layout == Layout::temporal_bands,
            "Unknown layout");
    require(options.canvas_width >= 640 && options.canvas_width <= 32768 && options.row_spacing >= 48 &&
                options.row_spacing <= 512 && options.margin >= 16 && options.margin <= 128 &&
                options.max_label_codepoints >= 4 && options.max_label_codepoints <= 256 &&
                options.title.size() <= 4096,
            "Invalid projection geometry or title limits");
    (void)boundaries(options.title);
    normalize(options.roles, false);
    normalize(options.temporal_classes, true);
    return options;
}
bool selected(const std::optional<std::vector<std::string>> &filter, const std::string &value) {
    return !filter || std::binary_search(filter->begin(), filter->end(), value);
}
std::map<std::string, std::string> owner_classes(const gr_relational_analysis::NetworkSnapshot &s) {
    std::map<std::string, std::string> found;
    const auto &c = s.context();
    if (c.contains("temporal") && c.at("temporal").is_object()) {
        const auto &t = c.at("temporal");
        if (t.contains("cut") && t.at("cut").is_object()) {
            for (const auto *name : classes) {
                if (!t.at("cut").contains(name))
                    continue;
                for (const auto &id : t.at("cut").at(name)) {
                    auto [it, added] = found.emplace(id.get<std::string>(), name);
                    require(added || it->second == name, "Conflicting owner temporal classes");
                }
            }
        }
    }
    for (const auto &e : s.occurrences()) {
        auto [it, added] = found.emplace(e.relation, e.temporal_class);
        require(added || it->second == e.temporal_class, "Conflicting owner temporal classes");
    }
    return found;
}
const char *color(const std::string &name) {
    if (name == "active")
        return "#256c4c";
    if (name == "inactive")
        return "#64748b";
    if (name == "indeterminate")
        return "#9a5a10";
    if (name == "unscoped")
        return "#72529b";
    return "#476579";
}
} // namespace

std::string to_string(Layout layout) {
    switch (layout) {
    case Layout::incidence_columns:
        return "incidence_columns";
    case Layout::temporal_bands:
        return "temporal_bands";
    }
    throw Error("visualization_contract", "Unknown layout");
}
Json ProjectionOptions::to_json() const {
    const auto o = normalized(*this);
    return {{"layout", to_string(o.layout)},
            {"roles", o.roles ? Json(*o.roles) : Json(nullptr)},
            {"temporal_classes", o.temporal_classes ? Json(*o.temporal_classes) : Json(nullptr)},
            {"canvas_width", o.canvas_width},
            {"row_spacing", o.row_spacing},
            {"margin", o.margin},
            {"max_label_codepoints", o.max_label_codepoints},
            {"occurrence_labels", o.occurrence_labels},
            {"title", o.title}};
}
ProjectionOptions ProjectionOptions::from_json(const Json &j) {
    require(j.is_object(), "Projection options must be an object");
    ProjectionOptions o;
    const std::set<std::string> allowed{"layout",      "roles",  "temporal_classes",     "canvas_width",
                                        "row_spacing", "margin", "max_label_codepoints", "occurrence_labels",
                                        "title"};
    for (const auto &[k, v] : j.items()) {
        require(allowed.contains(k), "Unknown projection option");
        if (k == "layout") {
            require(v.is_string(), "Layout must be a string");
            if (v == "incidence_columns")
                o.layout = Layout::incidence_columns;
            else if (v == "temporal_bands")
                o.layout = Layout::temporal_bands;
            else
                throw Error("visualization_contract", "Unknown layout");
        } else if (k == "roles" || k == "temporal_classes") {
            require(v.is_null() || v.is_array(), "Selection must be null or a string array");
            if (v.is_null())
                continue;
            require(v.size() <= 4096, "Too many filter entries");
            std::vector<std::string> values;
            for (const auto &entry : v) {
                require(entry.is_string(), "Filter entry must be a string");
                values.push_back(entry.get<std::string>());
            }
            if (k == "roles")
                o.roles = std::move(values);
            else
                o.temporal_classes = std::move(values);
        } else if (k == "title") {
            require(v.is_string(), "Title must be a string");
            o.title = v.get<std::string>();
        } else if (k == "occurrence_labels") {
            require(v.is_boolean(), "occurrence_labels must be boolean");
            o.occurrence_labels = v.get<bool>();
        } else {
            require(v.is_number_integer() && (!v.is_number_unsigned() ? v.get<std::int64_t>() >= 0 : true),
                    "Geometry must use nonnegative integers");
            const auto value = v.get<std::uint64_t>();
            require(value <= std::numeric_limits<std::uint32_t>::max(), "Geometry integer out of range");
            if (k == "canvas_width")
                o.canvas_width = static_cast<std::uint32_t>(value);
            else if (k == "row_spacing")
                o.row_spacing = static_cast<std::uint32_t>(value);
            else if (k == "margin")
                o.margin = static_cast<std::uint32_t>(value);
            else
                o.max_label_codepoints = static_cast<std::uint32_t>(value);
        }
    }
    return normalized(std::move(o));
}

struct Scene::Impl {
    Json document;
    std::string encoded, id;
};
Scene::Scene(std::shared_ptr<const Impl> p) : impl_(std::move(p)) {}
const Json &Scene::document() const {
    require(bool(impl_), "Empty scene");
    return impl_->document;
}
Json Scene::to_json() const {
    return document();
}
std::string Scene::encode() const {
    require(bool(impl_), "Empty scene");
    return impl_->encoded;
}
const std::string &Scene::id() const {
    require(bool(impl_), "Empty scene");
    return impl_->id;
}
Scene::operator bool() const noexcept {
    return bool(impl_);
}

Scene project(const gr_relational_analysis::NetworkSnapshot &snapshot, ProjectionOptions options,
              Limits limits, std::stop_token stop) {
    cancelled(stop);
    limits_valid(limits);
    require(bool(snapshot), "Empty network snapshot");
    options = normalized(std::move(options));
    const auto &vertices = snapshot.vertices();
    const auto &occurrences = snapshot.occurrences();
    if (vertices.size() > limits.max_vertices || occurrences.size() > limits.max_occurrences)
        throw Error("visualization_limit", "Input topology exceeds visualization limits");
    // Check context size before copying it into the derived scene.
    (void)encoded(snapshot.context(), limits.max_scene_bytes);
    auto temporal = owner_classes(snapshot);
    const std::string fallback =
        snapshot.context().at("temporal_query").is_null() ? "not_requested" : "unclassified";
    std::vector<std::string> classification(vertices.size());
    std::vector<std::size_t> owners, targets;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        cancelled(stop);
        const auto &v = vertices[i];
        if (v.kind == gr_relational_analysis::VertexKind::participant_owner) {
            owners.push_back(i);
            classification[i] = temporal.contains(v.id) ? temporal.at(v.id) : fallback;
            require(known_class(classification[i]), "Unknown owner temporal class");
        } else
            targets.push_back(i);
    }
    auto rank = [](const std::string &value) {
        return std::find(classes.begin(), classes.end(), value) - classes.begin();
    };
    auto order = [&](std::size_t a, std::size_t b) { return vertices[a].key < vertices[b].key; };
    std::sort(targets.begin(), targets.end(), order);
    std::sort(owners.begin(), owners.end(), [&](std::size_t a, std::size_t b) {
        if (options.layout == Layout::temporal_bands && classification[a] != classification[b])
            return rank(classification[a]) < rank(classification[b]);
        return order(a, b);
    });
    const std::uint64_t width = options.canvas_width, margin = options.margin;
    const std::uint64_t box_width = std::min<std::uint64_t>(360, (width - 2 * margin) / 3);
    const std::uint64_t box_height = 44, top = 196;
    std::vector<std::uint64_t> xs(vertices.size()), ys(vertices.size());
    Json bands = Json::array();
    std::uint64_t row = top;
    std::string previous;
    for (auto i : owners) {
        cancelled(stop);
        if (options.layout == Layout::temporal_bands && previous != classification[i]) {
            bands.push_back({{"class", classification[i]}, {"y", row}, {"label", classification[i]}});
            row += 36;
            previous = classification[i];
        }
        xs[i] = margin;
        ys[i] = row;
        row += options.row_spacing;
    }
    std::uint64_t target_row = top;
    for (auto i : targets) {
        cancelled(stop);
        xs[i] = width - margin - box_width;
        ys[i] = target_row;
        target_row += options.row_spacing;
    }
    const auto height = std::max(row, target_row) + margin + 100;
    if (height > max_coordinate)
        throw Error("visualization_limit", "Layout height exceeds coordinate limit");
    Json nodes = Json::array(), edges = Json::array(), excluded = Json::array();
    std::vector<std::size_t> all;
    all.insert(all.end(), owners.begin(), owners.end());
    all.insert(all.end(), targets.begin(), targets.end());
    std::vector<std::string> dom_ids(vertices.size());
    for (std::size_t n = 0; n < all.size(); ++n) {
        cancelled(stop);
        const auto i = all[n];
        const auto &v = vertices[i];
        dom_ids[i] = "n" + std::to_string(n);
        const auto label_budget = std::min<std::uint64_t>(options.max_label_codepoints, (box_width - 24) / 8);
        auto [text, truncated] = label(v.id, label_budget);
        const std::string secondary =
            v.version ? "record | " + v.version->substr(7, 12) : "subject designation";
        auto [secondary_label, secondary_truncated] = label(secondary, (box_width - 24) / 6);
        nodes.push_back(
            {{"glyph_id", dom_ids[i]},
             {"vertex_key", v.key},
             {"kind", gr_relational_analysis::to_string(v.kind)},
             {"scope", v.scope},
             {"record_id", v.id},
             {"version", v.version ? Json(*v.version) : Json(nullptr)},
             {"x", xs[i]},
             {"y", ys[i]},
             {"width", box_width},
             {"height", box_height},
             {"label", text},
             {"label_truncated", truncated},
             {"secondary_label", secondary_label},
             {"secondary_label_truncated", secondary_truncated},
             {"temporal_class", classification[i].empty() ? Json(nullptr) : Json(classification[i])}});
    }
    std::vector<std::size_t> sequence(occurrences.size());
    std::iota(sequence.begin(), sequence.end(), 0);
    std::sort(sequence.begin(), sequence.end(),
              [&](auto a, auto b) { return occurrences[a].key < occurrences[b].key; });
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> counts, seen;
    for (auto i : sequence) {
        cancelled(stop);
        const auto &e = occurrences[i];
        require(e.owner < vertices.size() && e.target < vertices.size(), "Invalid occurrence endpoints");
        if (selected(options.roles, e.role) && selected(options.temporal_classes, e.temporal_class))
            ++counts[{e.owner, e.target}];
    }
    for (auto i : sequence) {
        cancelled(stop);
        const auto &e = occurrences[i];
        if (!selected(options.roles, e.role) || !selected(options.temporal_classes, e.temporal_class)) {
            excluded.push_back(
                {{"occurrence_key", e.key},
                 {"role_excluded", !selected(options.roles, e.role)},
                 {"temporal_excluded", !selected(options.temporal_classes, e.temporal_class)}});
            continue;
        }
        const auto pair = std::make_pair(e.owner, e.target);
        const auto parallel = seen[pair]++, count = counts[pair];
        const auto x1 = xs[e.owner] + box_width, x2 = xs[e.target];
        const auto y1 = ys[e.owner] + box_height / 2, y2 = ys[e.target] + box_height / 2;
        // Integer control points distinguish repeated occurrences without inventing
        // distances. Parallel lane ordinals remain authoritative when raster overlap occurs.
        const auto middle = (x1 + x2) / 2;
        const auto offset = static_cast<std::int64_t>(2 * parallel) - static_cast<std::int64_t>(count - 1);
        const auto bend = static_cast<std::int64_t>(middle) + offset;
        const auto safe_bend = std::clamp<std::int64_t>(bend, static_cast<std::int64_t>(x1 + 8),
                                                        static_cast<std::int64_t>(x2 - 8));
        const auto control_y1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(y1) + offset * 4, 4,
                                                         static_cast<std::int64_t>(height) - 4);
        const auto control_y2 = std::clamp<std::int64_t>(static_cast<std::int64_t>(y2) + offset * 4, 4,
                                                         static_cast<std::int64_t>(height) - 4);
        auto [text, truncated] =
            label(e.role, std::min<std::uint64_t>(options.max_label_codepoints, (x2 - x1 - 16) / 7));
        edges.push_back(
            {{"glyph_id", "e" + std::to_string(edges.size())},
             {"occurrence_key", e.key},
             {"owner_glyph", dom_ids[e.owner]},
             {"target_glyph", dom_ids[e.target]},
             {"owner_vertex_key", vertices[e.owner].key},
             {"target_vertex_key", vertices[e.target].key},
             {"relation", e.relation},
             {"relation_version", e.relation_version},
             {"ordinal", e.ordinal},
             {"role", e.role},
             {"participant", e.participant},
             {"temporal_class", e.temporal_class},
             {"parallel_ordinal", parallel},
             {"parallel_count", count},
             {"points", Json::array({Json::array({x1, y1}), Json::array({safe_bend, control_y1}),
                                     Json::array({safe_bend, control_y2}), Json::array({x2, y2})})},
             {"label", text},
             {"label_truncated", truncated},
             {"label_x", middle},
             {"label_y", (y1 + y2) / 2 + 14 * (parallel % 3)}});
    }
    // All edge labels occupy the same inter-column region. A stable global
    // lane pass prevents unrelated occurrences sharing an identical midpoint.
    constexpr std::uint64_t label_spacing = 14;
    const std::uint64_t label_min = top + 12, label_max = height - margin - 12;
    const auto label_capacity = (label_max - label_min) / label_spacing + 1;
    const bool dense_labels = edges.size() > label_capacity;
    std::vector<std::size_t> labels(edges.size());
    std::iota(labels.begin(), labels.end(), 0);
    std::sort(labels.begin(), labels.end(), [&](auto a, auto b) {
        if (edges[a].at("label_y") != edges[b].at("label_y"))
            return edges[a].at("label_y") < edges[b].at("label_y");
        return edges[a].at("occurrence_key") < edges[b].at("occurrence_key");
    });
    std::vector<std::uint64_t> positions(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        cancelled(stop);
        if (dense_labels) {
            positions[i] = label_min + (label_max - label_min) * static_cast<std::uint64_t>(i) /
                                           static_cast<std::uint64_t>(labels.size() - 1);
        } else {
            const auto desired =
                std::clamp(edges[labels[i]].at("label_y").get<std::uint64_t>(), label_min, label_max);
            positions[i] = i ? std::max(desired, positions[i - 1] + label_spacing) : desired;
        }
    }
    if (!positions.empty() && !dense_labels) {
        positions.back() = std::min(positions.back(), label_max);
        for (std::size_t i = positions.size() - 1; i > 0; --i) {
            cancelled(stop);
            positions[i - 1] = std::min(positions[i - 1], positions[i] - label_spacing);
        }
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        cancelled(stop);
        auto &edge = edges[labels[i]];
        const auto desired = edge.at("label_y").get<std::uint64_t>();
        edge["label_desired_y"] = desired;
        edge["label_y"] = positions[i];
        edge["label_offset_y"] = static_cast<std::int64_t>(positions[i]) - static_cast<std::int64_t>(desired);
    }
    auto impl = std::make_shared<Scene::Impl>();
    impl->document = {
        {"format", scene_format},
        {"snapshot_id", snapshot.id()},
        {"source_identity", snapshot.identity()},
        {"validation_basis", snapshot.validation_basis()},
        {"source_context", snapshot.context()},
        {"projection", options.to_json()},
        {"label_placement",
         {{"algorithm", "stable-global-lanes/1"},
          {"enabled", options.occurrence_labels},
          {"minimum_spacing", label_spacing},
          {"minimum_y", label_min},
          {"maximum_y", label_max},
          {"spaced_capacity", label_capacity},
          {"dense_fallback", dense_labels},
          {"fallback", "compressed ordered lanes; overlap remains possible"}}},
        {"width", width},
        {"height", height},
        {"nodes", std::move(nodes)},
        {"edges", std::move(edges)},
        {"bands", std::move(bands)},
        {"excluded_occurrences", std::move(excluded)},
        {"counts",
         {{"source_vertices", vertices.size()},
          {"source_occurrences", occurrences.size()},
          {"visible_vertices", vertices.size()},
          {"visible_occurrences",
           seen.empty() ? 0
                        : std::accumulate(seen.begin(), seen.end(), std::size_t(0),
                                          [](auto sum, const auto &p) { return sum + p.second; })}}},
        {"interpretation",
         {{"geometry", "symbolic; coordinates are layout choices, not measured position or time"},
          {"incidence",
           "one edge per recorded participant occurrence; no clique expansion or causal inference"},
          {"temporal", "classes describe participant-owner records; bands are categories, not a chronology"},
          {"filtering",
           "occurrence filters retain all source vertices, including isolated owners and targets"},
          {"labels", "display labels may be truncated; full identities and roles remain in mappings"},
          {"custody", "source records, archive and history bodies are omitted; pins and hashes are not "
                      "derivation proof"}}}};
    cancelled(stop);
    impl->id = "sha256:" + gr::sha256(encoded(impl->document, limits.max_scene_bytes));
    impl->document["scene_id"] = impl->id;
    impl->encoded = encoded(impl->document, limits.max_scene_bytes);
    cancelled(stop);
    return Scene(std::move(impl));
}

std::string svg(const Scene &scene, Limits limits, std::stop_token stop) {
    cancelled(stop);
    limits_valid(limits);
    const auto &d = scene.document();
    if (d.at("nodes").size() > limits.max_vertices || d.at("edges").size() > limits.max_occurrences)
        throw Error("visualization_limit", "Scene topology exceeds export limits");
    BoundedBuffer buffer(limits.max_svg_bytes);
    std::ostream out(&buffer);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" << d.at("width")
        << "\" height=\"" << d.at("height") << "\" viewBox=\"0 0 " << d.at("width") << ' ' << d.at("height")
        << "\" role=\"img\" aria-labelledby=\"scene-title scene-desc\">\n<title id=\"scene-title\">"
        << xml(d.at("projection").at("title").get<std::string>()) << "</title>\n"
        << "<desc id=\"scene-desc\">Symbolic recorded-incidence projection. Geometry is not measured space "
           "or time; "
           "lines do not imply causality. Full source custody is separate. Validation basis: "
        << xml(d.at("validation_basis").get<std::string>()) << ".</desc>\n<metadata>";
    xml_stream(out, scene.encode(), stop);
    out << "</metadata>\n<defs><clipPath id=\"header-clip\"><rect x=\"40\" y=\"12\" width=\""
        << d.at("width").get<std::uint64_t>() - 80 << "\" height=\"132\"/></clipPath>";
    for (const auto &n : d.at("nodes")) {
        cancelled(stop);
        out << "<clipPath id=\"clip-" << n.at("glyph_id").get<std::string>() << "\"><rect x=\""
            << n.at("x").get<std::uint64_t>() + 8 << "\" y=\"" << n.at("y") << "\" width=\""
            << n.at("width").get<std::uint64_t>() - 16 << "\" height=\"" << n.at("height")
            << "\"/></clipPath>";
    }
    out << "</defs>\n<rect width=\"100%\" height=\"100%\" fill=\"#f6f8fb\"/>\n"
        << "<g font-family=\"sans-serif\" fill=\"#193347\">\n"
        << "<g clip-path=\"url(#header-clip)\"><text x=\"40\" y=\"42\" font-size=\"24\" font-weight=\"bold\">"
        << xml(label(d.at("projection").at("title").get<std::string>(),
                     (d.at("width").get<std::uint64_t>() - 80) / 15)
                   .first)
        << "</text>\n"
        << "<text x=\"40\" y=\"72\" font-size=\"13\">Symbolic incidence | one line per recorded occurrence | "
           "no causal arrows</text>\n"
        << "<text x=\"40\" y=\"96\" font-size=\"13\">Validation: "
        << xml(d.at("validation_basis").get<std::string>()) << " | shown " << d.at("edges").size() << " of "
        << d.at("counts").at("source_occurrences") << " occurrences</text>\n"
        << "<text x=\"40\" y=\"120\" font-size=\"12\">Temporal classes qualify owner records; bands are "
           "categories, not time.</text>\n"
        << "<text x=\"40\" y=\"140\" font-size=\"12\">Complete source custody is separate.</text></g>\n"
        << "<text x=\"40\" y=\"166\" font-size=\"13\" font-weight=\"bold\">Participant-owner records</text>\n"
        << "<text x=\""
        << d.at("width").get<std::uint64_t>() - d.at("projection").at("margin").get<std::uint64_t>()
        << "\" y=\"166\" text-anchor=\"end\" font-size=\"13\" font-weight=\"bold\">Referenced "
           "targets</text>\n";
    for (const auto &b : d.at("bands")) {
        cancelled(stop);
        out << "<text x=\"" << d.at("projection").at("margin") << "\" y=\"" << b.at("y")
            << "\" font-size=\"13\" font-weight=\"bold\" fill=\"" << color(b.at("class")) << "\">"
            << xml(b.at("label").get<std::string>()) << "</text>\n";
    }
    for (const auto &e : d.at("edges")) {
        cancelled(stop);
        const auto &p = e.at("points");
        out << "<g id=\"" << e.at("glyph_id").get<std::string>() << "\"><title>"
            << xml(e.at("relation").get<std::string>() + " | ordinal " +
                   std::to_string(e.at("ordinal").get<std::size_t>()) + " | role " +
                   e.at("role").get<std::string>() + " | " + e.at("temporal_class").get<std::string>())
            << "</title><path fill=\"none\" stroke=\"" << color(e.at("temporal_class"))
            << "\" stroke-width=\"1.5\" opacity=\"0.75\" d=\"M " << p[0][0] << ' ' << p[0][1] << " C "
            << p[1][0] << ' ' << p[1][1] << ", " << p[2][0] << ' ' << p[2][1] << ", " << p[3][0] << ' '
            << p[3][1] << "\"/>";
        if (d.at("projection").at("occurrence_labels").get<bool>())
            out << "<text x=\"" << e.at("label_x") << "\" y=\"" << e.at("label_y")
                << "\" text-anchor=\"middle\" font-size=\"11\" stroke=\"#f6f8fb\" stroke-width=\"3\" "
                   "paint-order=\"stroke\">"
                << xml(e.at("label").get<std::string>()) << "</text>";
        out << "</g>\n";
    }
    for (const auto &n : d.at("nodes")) {
        cancelled(stop);
        const bool owner = n.at("kind") == "participant_owner";
        out << "<g id=\"" << n.at("glyph_id").get<std::string>() << "\"><title>"
            << xml(n.at("scope").get<std::string>() + ":" + n.at("record_id").get<std::string>() +
                   (n.at("version").is_null() ? " | subject designation; no record version"
                                              : " | " + n.at("version").get<std::string>()))
            << "</title><rect x=\"" << n.at("x") << "\" y=\"" << n.at("y") << "\" width=\"" << n.at("width")
            << "\" height=\"" << n.at("height") << "\" rx=\"" << (owner ? 6 : 22) << "\" fill=\""
            << (owner ? "#e5edf4" : "#ffffff") << "\" stroke=\""
            << (owner ? color(n.at("temporal_class")) : "#60798b") << "\" stroke-width=\"2\"/>"
            << "<g clip-path=\"url(#clip-" << n.at("glyph_id").get<std::string>() << ")\"><text x=\""
            << n.at("x").get<std::uint64_t>() + 12 << "\" y=\"" << n.at("y").get<std::uint64_t>() + 18
            << "\" font-size=\"13\">" << xml(n.at("label").get<std::string>()) << "</text>"
            << "<text x=\"" << n.at("x").get<std::uint64_t>() + 12 << "\" y=\""
            << n.at("y").get<std::uint64_t>() + 34 << "\" font-size=\"10\" fill=\"#476579\">"
            << xml(n.at("secondary_label").get<std::string>()) << "</text></g></g>\n";
    }
    out << "</g>\n</svg>\n";
    cancelled(stop);
    return std::move(buffer.data);
}
} // namespace gr_visualization
