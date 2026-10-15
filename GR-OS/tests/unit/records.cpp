#include "gr/core.hpp"
#include <iostream>

using gr::Json;
using namespace gr::core;
namespace {
int checks = 0;
void check(bool b, const char *m) {
    ++checks;
    if (!b)
        throw std::runtime_error(m);
}
template <class F> void fails(F f, const char *message) {
    bool failed = false;
    try {
        f();
    } catch (const gr::Error &) {
        failed = true;
    }
    check(failed, message);
}
Json doc(std::string id, std::string type = "gr.entity@1") {
    return {{"format", "gr.record/1"},
            {"id", id},
            {"type", type},
            {"payload", Json::object()},
            {"attribution", {{"agent", "fixture:alice"}, {"method", "reported"}}}};
}
Json snapshot(Json rs = Json::array()) {
    return {{"revision", "0"}, {"records", rs}};
}
Json order(std::string id, std::string a, std::string b) {
    auto d = doc(id, "gr.order@1");
    d["order"] = {{"before", a}, {"after", b}};
    return construct(d);
}
} // namespace
int main() {
    try {
        check(gr::sha256(std::string_view("abc")) ==
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              "SHA256 known answer");
        check(gr::canonical(Json{{"b", 2}, {"a", 1}}) == "{\"a\":1,\"b\":2}", "canonical sorted keys");
        check(gr::canonical(Json(9007199254740991LL)) == "9007199254740991", "safe upper bound");
        fails([] { gr::canonical(Json(1.0)); }, "floating metadata rejected");
        fails([] { gr::canonical(Json(9007199254740992ULL)); }, "unsafe integer rejected");
        fails([] { gr::canonical(Json(std::string("\xff"))); }, "invalid UTF8 rejected");
        Json nested = 0;
        for (int i = 0; i < 66; ++i)
            nested = Json::array({nested});
        fails([&] { gr::canonical(nested); }, "metadata depth bounded");
        auto a = construct(doc("a"));
        check(verify(a)["valid"], "constructed record verifies");
        auto immutable = Record::from_wire(a);
        auto copy = immutable.wire();
        copy["document"]["payload"]["change"] = true;
        check(immutable.wire() == a, "record value owns immutable validated document");
        fails([&] { construct(doc("bad-type", "gr.@1")); }, "empty namespace segment rejected");
        fails([&] { construct(doc("bad-type", "gr.value@")); }, "empty type version rejected");
        auto older_document = doc("older", "legacy..type@1");
        Json older = {{"document", older_document},
                      {"version", "sha256:" + gr::sha256(gr::canonical(older_document))}};
        check(verify(older)["valid"] && !verify(older)["semantic_complete"].get<bool>(),
              "v1 permissive unknown identifiers remain readable with unchanged identity");
        check(gr::core::apply(snapshot(Json::array({older})), Json::array({a}))["records"][0] == older,
              "unrelated edits preserve opaque older record bytes");
        auto tamper = a;
        tamper["document"]["payload"]["claim"] = "different";
        check(!verify(tamper)["valid"].get<bool>(), "tampering detected");
        auto incomplete = doc("missing");
        incomplete.erase("attribution");
        fails([&] { construct(incomplete); }, "attribution required");

        auto opaque = construct(doc("opaque", "lab.tensor@9"));
        check(verify(opaque)["valid"] && !verify(opaque)["semantic_complete"].get<bool>(),
              "unknown type structurally preserved without semantic claim");
        auto required = doc("required", "lab.tensor@9");
        required["required"] = true;
        auto required_record = construct(required);
        check(validate_snapshot(snapshot(Json::array({required_record})))["valid"],
              "unknown required archive readable");
        fails([&] { gr::core::apply(snapshot(), Json::array({required_record})); },
              "required unknown blocks mutation");
        fails(
            [&] {
                gr::core::apply(snapshot(Json::array({required_record})),
                                Json::array({construct(doc("required"))}));
            },
            "cannot rewrite unknown required source into known type");
        TypeRegistry registry;
        registry.register_type("lab.tensor@9", [](const Json &d) {
            if (!d.at("payload").contains("shape"))
                throw gr::Error("shape", "tensor shape required");
        });
        fails([&] { construct(required, registry); }, "registered type checks additional constraint");
        required["payload"]["shape"] = {2, 3};
        check(gr::core::apply(snapshot(), Json::array({construct(required, registry)}), registry)["records"]
                      .size() == 1,
              "registered required type admits without core modification");
        fails([&] { registry.register_type("lab.tensor@9", [](const Json &) {}); }, "duplicate type denied");
        auto ext = doc("extension");
        ext["extensions"] =
            Json::array({{{"type", "lab.unknown@1"}, {"required", true}, {"data", Json::object()}}});
        fails([&] { gr::core::apply(snapshot(), Json::array({construct(ext)})); },
              "unknown required extension blocks mutation");
        ext["extensions"][0]["type"] = "gr.entity@1";
        ext["extensions"][0]["data"] = 42;
        check(!verify(construct(ext))["semantic_complete"].get<bool>(),
              "record type registry cannot confer extension validation");
        fails([&] { gr::core::apply(snapshot(), Json::array({construct(ext)})); },
              "required extension cannot borrow builtin record validator");
        registry.register_extension("lab.units@1", [](const Json &value) {
            if (!value.is_object() || value.value("unit", std::string{}) != "ms")
                throw gr::Error("unit", "Expected ms");
        });
        ext["extensions"][0] = {{"type", "lab.units@1"}, {"required", true}, {"data", {{"unit", "ms"}}}};
        check(verify(construct(ext, registry), registry)["semantic_complete"],
              "extension validator explicitly registered");

        auto relation = doc("rel", "gr.relation@1");
        relation["roles"] = {"Relation", "Entity"};
        relation["participants"] = Json::array(
            {{{"role", "contributor"}, {"reference", {{"scope", "record"}, {"id", "a"}}}},
             {{"role", "reviewer"}, {"reference", {{"scope", "record"}, {"id", "a"}}}},
             {{"role", "subject"}, {"reference", {{"scope", "subject"}, {"id", "world:unregistered"}}}}});
        auto s = gr::core::apply(snapshot(), Json::array({construct(relation), a}));
        check(s["records"].size() == 2, "whole candidate resolves forward references");
        check(graph(s)["incidences"].size() == 3, "repeated nary participants preserved");
        check(match(s, {{"role", "Entity"}})["records"].size() == 1, "overlapping role matching");
        auto absent = relation;
        absent["participants"][0]["reference"]["id"] = "absent";
        fails([&] { gr::core::apply(s, Json::array({construct(absent)})); },
              "unresolved reference prevents candidate");
        check(s["records"][0]["document"]["participants"][0]["reference"]["id"] == "a",
              "failed apply leaves original intact");
        fails([&] { gr::core::apply(s, Json::array({a, a})); }, "duplicate update rejected");
        auto revised = doc("a");
        revised["payload"]["revision"] = 2;
        auto s2 = gr::core::apply(s, Json::array({construct(revised)}));
        auto pinned = doc("pinned");
        pinned["references"] = Json::array({{{"id", "a"}, {"scope", "record"}, {"version", a["version"]}}});
        s2 = gr::core::apply(s2, Json::array({construct(pinned)}));
        check(s2["archive"].size() == 1 && validate_snapshot(s2, true)["valid"],
              "old version pin resolves retained history");
        auto without = s2;
        without.erase("archive");
        check(!validate_snapshot(without)["valid"].get<bool>(), "missing pinned version explicit");
        check(graph(s2)["nodes"].size() == 3, "archive excluded from current graph");

        auto e1 = construct(doc("e1", "gr.event@1")), e2 = construct(doc("e2", "gr.event@1")),
             e3 = construct(doc("e3", "gr.event@1"));
        auto state = doc("state", "gr.state@1");
        state["temporal"] = {{"start", {{"kind", "event"}, {"id", "e1"}}},
                             {"end", {{"kind", "event"}, {"id", "e2"}}}};
        auto unsure = doc("unsure", "gr.state@1");
        unsure["temporal"] = {{"start", {{"kind", "unknown"}}}, {"end", {{"kind", "unbounded"}}}};
        auto t = gr::core::apply(snapshot(), Json::array({e1, e2, e3, order("o1", "e1", "e2"),
                                                          construct(state), construct(unsure)}));
        auto cut = temporal(t, {{"after", {"e1"}}});
        check(cut["consistent"] && cut["cut"]["active"] == Json::array({"state"}), "inclusive start");
        check(cut["cut"]["indeterminate"] == Json::array({"unsure"}), "unknown temporal endpoint preserved");
        check(temporal(t, {{"after", {"e1", "e2"}}})["cut"]["inactive"] == Json::array({"state"}),
              "exclusive end");
        fails([&] { temporal(t, {{"after", {"e2"}}}); }, "ancestor closure required");
        check(temporal(t, {{"after", {"e3"}}})["consistent"],
              "independent event allowed without implying simultaneity");
        TypeRegistry custom;
        custom.register_type(
            RecordTypeDescriptor{"lab.boundary@1", "Experimental domain boundary", {"event-anchor"}},
            [](const Json &d) {
                if (d.at("payload").value("basis", std::string{}).empty())
                    throw gr::Error("basis", "Explicit boundary basis required");
            });
        custom.register_type(
            RecordTypeDescriptor{"lab.order@1", "Experimental qualified precedence", {"precedence"}},
            [](const Json &) {});
        auto c1 = doc("c1", "lab.boundary@1");
        c1["payload"]["basis"] = "synthetic boundary";
        auto c2 = c1;
        c2["id"] = "c2";
        auto co = doc("co", "lab.order@1");
        co["order"] = {{"before", "c1"}, {"after", "c2"}};
        auto custom_state = gr::core::apply(
            snapshot(), Json::array({construct(c1, custom), construct(c2, custom), construct(co, custom)}),
            custom);
        check(temporal(custom_state, {{"after", {"c1"}}}, custom)["consistent"],
              "custom event and order profiles participate through declared capabilities");
        check(graph(custom_state, custom)["nodes"].size() == 3, "projection retains caller registry");
        check(!validate_snapshot(custom_state)["valid"].get<bool>(),
              "unsupported profile cannot silently inherit event semantics");
        check(custom.descriptors()["record_profiles"].size() == 10, "record profile descriptors inspectable");
        t = gr::core::apply(t, Json::array({order("o2", "e2", "e1")}));
        check(validate_snapshot(t, true)["valid"], "contradictory accounts admitted as records");
        auto cyc = temporal(t, {{"after", {"e1"}}});
        check(!cyc["consistent"].get<bool>() && cyc["cut"].is_null(),
              "cycle blocks projection not preservation");
        auto selected = temporal(t, {{"order_ids", {"o1"}}, {"after", {"e1"}}});
        check(selected["consistent"] && selected["excluded_orders"] == Json::array({"o2"}),
              "selection yields coherent account with declared exclusions");
        auto future = doc("future-order", "gr.order@2");
        future["order"] = {{"before", "e1"}, {"after", "e3"}};
        auto future_snapshot = gr::core::apply(t, Json::array({construct(future)}));
        auto blocked = temporal(future_snapshot, {{"after", {"e1"}}});
        check(!blocked["interpretation_complete"].get<bool>() && blocked["cut"].is_null(),
              "uninterpreted order blocks default cut");
        check(
            temporal(future_snapshot, {{"order_ids", {"o1"}}, {"after", {"e1"}}})["interpretation_complete"],
            "explicit account excludes unknown order with disclosed scope");
        auto required_order = order("required-order", "e1", "e2")["document"];
        required_order["extensions"] =
            Json::array({{{"type", "lab.order-meaning@1"}, {"required", true}, {"data", Json::object()}}});
        auto required_order_snapshot = t;
        required_order_snapshot["records"].push_back(construct(required_order));
        auto required_order_view =
            temporal(required_order_snapshot, {{"order_ids", {"required-order"}}, {"after", {"e1"}}});
        check(!required_order_view["interpretation_complete"].get<bool>() &&
                  required_order_view["cut"].is_null(),
              "selected known order with unknown required semantics cannot produce cut");
        auto required_state = state;
        required_state["id"] = "required-state";
        required_state["extensions"] = required_order["extensions"];
        auto required_state_snapshot = t;
        required_state_snapshot["records"].push_back(construct(required_state));
        auto required_state_view =
            temporal(required_state_snapshot, {{"order_ids", {"o1"}}, {"after", {"e1"}}});
        check(required_state_view["cut"]["indeterminate"].size() == 2,
              "unknown required scoped semantics produce indeterminate state");
        auto ambiguous = state;
        ambiguous["id"] = "ambiguous";
        ambiguous["temporal"]["end"]["id"] = "e3";
        auto ambiguous_snapshot = gr::core::apply(t, Json::array({construct(ambiguous)}));
        auto ambiguous_cut = temporal(ambiguous_snapshot, {{"order_ids", {"o1"}}, {"after", {"e1"}}});
        check(ambiguous_cut["cut"]["indeterminate"].size() == 2,
              "incomparable interval endpoints remain indeterminate");
        auto backwards = state;
        backwards["id"] = "backwards";
        backwards["temporal"]["start"]["id"] = "e2";
        backwards["temporal"]["end"]["id"] = "e1";
        check(temporal(gr::core::apply(t, Json::array({construct(backwards)})),
                       {{"order_ids", {"o1"}}, {"after", {"e1"}}})["diagnostics"]
                      .size() == 1,
              "reversed interval diagnosed");
        fails([&] { temporal(t, {{"order_ids", {"not-there"}}}); }, "unknown selected order rejected");
        auto wrongevent = doc("bad", "gr.order@1");
        wrongevent["order"] = {{"before", "a"}, {"after", "e1"}};
        fails([&] { gr::core::apply(s, Json::array({e1, construct(wrongevent)})); },
              "non-event temporal anchor rejected");
        fails([&] { match(s, {{"typo", 1}}); }, "unknown query fields not silently ignored");
        std::cout << "core: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "core failure: " << e.what() << '\n';
        return 1;
    }
}
