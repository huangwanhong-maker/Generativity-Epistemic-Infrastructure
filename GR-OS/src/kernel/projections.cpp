#include "internal.hpp"
#include <algorithm>
#include <deque>
#include <set>

namespace gr::core {
using namespace detail;
namespace {
Json source(const Json &snapshot) {
    if (snapshot.contains("staging"))
        return {{"source_revision", nullptr},
                {"source_state", "staged"},
                {"base_revision", snapshot["staging"].at("base_revision")}};
    return {{"source_revision", snapshot["revision"]}, {"source_state", "supplied revision"}};
}
} // namespace
Json match(const Json &snapshot, const Json &filter, const TypeRegistry &registry) {
    require_valid(snapshot, registry);
    require(filter.is_object(), "query", "Match filter must be an object");
    for (auto it = filter.begin(); it != filter.end(); ++it)
        require(it.key() == "id" || it.key() == "type" || it.key() == "role" || it.key() == "payload",
                "query", "Unknown match filter: " + it.key());
    if (filter.contains("payload"))
        require(filter["payload"].is_object(), "query", "payload filter must be an object");
    Json out = Json::array();
    for (const auto &r : records(snapshot)) {
        const auto &d = r["document"];
        bool selected = true;
        for (auto field : {"id", "type"})
            if (filter.contains(field) && filter[field] != d[field])
                selected = false;
        if (filter.contains("role")) {
            selected = selected && d.contains("roles") &&
                       std::find(d["roles"].begin(), d["roles"].end(), filter["role"]) != d["roles"].end();
        }
        if (filter.contains("payload"))
            for (auto it = filter["payload"].begin(); it != filter["payload"].end(); ++it)
                if (!d["payload"].contains(it.key()) || d["payload"][it.key()] != it.value())
                    selected = false;
        if (selected)
            out.push_back(r);
    }
    auto result = source(snapshot);
    result["records"] = std::move(out);
    return result;
}
Json graph(const Json &snapshot, const TypeRegistry &registry) {
    require_valid(snapshot, registry);
    Json nodes = Json::array(), incidences = Json::array(), references = Json::array();
    for (const auto &r : records(snapshot)) {
        const auto &d = r["document"];
        nodes.push_back({{"id", d["id"]},
                         {"version", r["version"]},
                         {"type", d["type"]},
                         {"roles", d.value("roles", Json::array())}});
        if (d.contains("participants"))
            for (std::size_t i = 0; i < d["participants"].size(); ++i)
                incidences.push_back({{"relation", d["id"]},
                                      {"relation_version", r["version"]},
                                      {"ordinal", i},
                                      {"participant", d["participants"][i]}});
        if (d.contains("references"))
            for (const auto &ref : d["references"])
                references.push_back({{"source", d["id"]}, {"reference", ref}});
    }
    auto result = source(snapshot);
    result.update(
        {{"nodes", nodes},
         {"incidences", incidences},
         {"references", references},
         {"interpretation_scope",
          "Envelope structure only; optional typed payloads/extensions are not interpreted. Pinned "
          "references resolve by (id,version), including supplied archive."},
         {"losses",
          {"payloads, attribution, qualifiers, artifacts and temporal fields require the source records"}},
         {"interpretation",
          "Incidence is a recorded participation; no causal, legal or epistemic conclusion is inferred"}});
    return result;
}
Json temporal(const Json &snapshot, const Json &query, const TypeRegistry &registry) {
    require_valid(snapshot, registry);
    require(query.is_object(), "query", "Temporal query must be an object");
    for (auto it = query.begin(); it != query.end(); ++it)
        require(it.key() == "order_ids" || it.key() == "after", "query",
                "Unknown temporal query field: " + it.key());
    array_field(query, "order_ids");
    array_field(query, "after");
    std::set<std::string> selected, events;
    bool explicit_selection = query.contains("order_ids");
    if (explicit_selection)
        for (const auto &id : query["order_ids"]) {
            require(id.is_string(), "query", "Order IDs must be strings");
            require(selected.insert(id.get<std::string>()).second, "query", "Repeated selected order ID");
        }
    std::map<std::string, std::set<std::string>> edges;
    std::map<std::string, std::size_t> degrees;
    Json included = Json::array(), excluded = Json::array(), unsupported = Json::array();
    for (const auto &r : records(snapshot))
        if (registry.supports(r["document"]["type"].get<std::string>(), "event-anchor")) {
            auto id = r["document"]["id"].get<std::string>();
            events.insert(id);
            degrees[id] = 0;
        }
    std::set<std::string> found;
    auto required_unknown = [&](const Json &d) {
        if (d.value("required", false) && !registry.known(d["type"].get<std::string>()))
            return true;
        if (d.contains("extensions"))
            for (const auto &e : d["extensions"])
                if (e["required"].get<bool>() && !registry.known_extension(e["type"].get<std::string>()))
                    return true;
        return false;
    };
    std::set<std::string> opaque_events;
    for (const auto &r : records(snapshot))
        if (registry.supports(r["document"]["type"].get<std::string>(), "event-anchor") &&
            required_unknown(r["document"]))
            opaque_events.insert(r["document"]["id"].get<std::string>());
    bool selected_unsupported = false;
    for (const auto &r : records(snapshot)) {
        const auto &d = r["document"];
        if (!registry.supports(d["type"].get<std::string>(), "precedence") || required_unknown(d)) {
            if (d.contains("order")) {
                unsupported.push_back({{"id", d["id"]}, {"type", d["type"]}, {"version", r["version"]}});
                if (explicit_selection && !selected.contains(d["id"].get<std::string>()))
                    excluded.push_back(d["id"]);
                if (explicit_selection && selected.contains(d["id"].get<std::string>())) {
                    found.insert(d["id"].get<std::string>());
                    selected_unsupported = true;
                }
            }
            continue;
        }
        auto id = d["id"].get<std::string>();
        if (explicit_selection && !selected.contains(id)) {
            excluded.push_back(id);
            continue;
        }
        included.push_back({{"id", id}, {"version", r["version"]}});
        found.insert(id);
        auto a = d["order"]["before"].get<std::string>(), b = d["order"]["after"].get<std::string>();
        if (opaque_events.contains(a) || opaque_events.contains(b))
            selected_unsupported = true;
        if (edges[a].insert(b).second)
            ++degrees[b];
    }
    require(!explicit_selection || found == selected, "query",
            "Selected order claim not present in this snapshot");
    std::deque<std::string> ready;
    for (const auto &[e, n] : degrees)
        if (n == 0)
            ready.push_back(e);
    std::size_t visited = 0;
    while (!ready.empty()) {
        auto e = ready.front();
        ready.pop_front();
        ++visited;
        for (const auto &next : edges[e])
            if (--degrees[next] == 0)
                ready.push_back(next);
    }
    bool consistent = visited == events.size();
    Json report = source(snapshot);
    report.update(
        {{"selected_orders", included},
         {"excluded_orders", excluded},
         {"unsupported_order_records", unsupported},
         {"interpretation_complete", !selected_unsupported && (explicit_selection || unsupported.empty())},
         {"consistent", consistent},
         {"cut", nullptr},
         {"diagnostics", Json::array()},
         {"interpretation",
          "Strict precedence claims; incomparability does not imply simultaneity; recording "
          "order is not occurrence order"}});
    if (selected_unsupported || (!explicit_selection && !unsupported.empty())) {
        report["diagnostics"].push_back(
            {{"code", "unsupported_order_semantics"},
             {"message", "Default account contains uninterpreted order records; select supported order IDs "
                         "explicitly for a scoped cut"}});
        return report;
    }
    if (!consistent) {
        report["diagnostics"] =
            Json::array({{{"code", "cyclic_order"},
                          {"message", "Selected claims contain a cycle; source accounts remain preserved"}}});
        return report;
    }
    if (!query.contains("after"))
        return report;
    std::set<std::string> past;
    for (const auto &e : query["after"]) {
        require(e.is_string() && events.contains(e.get<std::string>()), "cut",
                "Cut names an unavailable event");
        require(past.insert(e.get<std::string>()).second, "cut", "Cut repeats an event");
        require(!opaque_events.contains(e.get<std::string>()), "unsupported_event_semantics",
                "Cut uses event with unavailable required interpretation");
    }
    for (const auto &[before, afters] : edges)
        for (const auto &after : afters)
            require(!past.contains(after) || past.contains(before), "cut_not_closed",
                    "Cut must contain all known predecessors");
    Json active = Json::array(), inactive = Json::array(), indeterminate = Json::array(),
         unscoped = Json::array();
    auto precedes = [&](const std::string &start, const std::string &end) {
        std::set<std::string> seen;
        std::vector<std::string> todo{start};
        while (!todo.empty()) {
            auto here = todo.back();
            todo.pop_back();
            if (!seen.insert(here).second)
                continue;
            for (const auto &next : edges[here]) {
                if (next == end)
                    return true;
                todo.push_back(next);
            }
        }
        return false;
    };
    for (const auto &r : records(snapshot)) {
        const auto &d = r["document"];
        if (!d.contains("temporal")) {
            unscoped.push_back(d["id"]);
            continue;
        }
        const auto &start = d["temporal"]["start"];
        const auto &end = d["temporal"]["end"];
        if (required_unknown(d) ||
            (start["kind"] == "event" && opaque_events.contains(start["id"].get<std::string>())) ||
            (end["kind"] == "event" && opaque_events.contains(end["id"].get<std::string>()))) {
            indeterminate.push_back(d["id"]);
            report["diagnostics"].push_back({{"code", "unsupported_temporal_semantics"}, {"id", d["id"]}});
            continue;
        }
        if (start["kind"] == "event" && end["kind"] == "event" &&
            !precedes(start["id"].get<std::string>(), end["id"].get<std::string>())) {
            indeterminate.push_back(d["id"]);
            report["diagnostics"].push_back(
                {{"code", "unestablished_interval"},
                 {"id", d["id"]},
                 {"message", "Selected order does not establish strict start-before-end; interval retained "
                             "without active/inactive classification"}});
            continue;
        }
        auto endpoint = [&](const Json &e, bool start) -> int {
            if (e["kind"] == "unknown")
                return -1;
            if (e["kind"] == "unbounded")
                return 1;
            bool passed = past.contains(e["id"].get<std::string>());
            return (start ? passed : !passed) ? 1 : 0;
        };
        int a = endpoint(d["temporal"]["start"], true), b = endpoint(d["temporal"]["end"], false);
        if (a == 0 || b == 0)
            inactive.push_back(d["id"]);
        else if (a < 0 || b < 0)
            indeterminate.push_back(d["id"]);
        else
            active.push_back(d["id"]);
    }
    report["cut"] = {{"after", query["after"]}, {"active", active},
                     {"inactive", inactive},    {"indeterminate", indeterminate},
                     {"unscoped", unscoped},    {"interval_convention", "start inclusive, end exclusive"}};
    return report;
}
} // namespace gr::core
