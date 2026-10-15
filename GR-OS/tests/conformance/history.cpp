#include "gr/kernel/history.hpp"
#include "gr/kernel/projections.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <set>

using gr::Json;
namespace {
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void fails(F &&f, const char *message) {
    bool failed = false;
    try {
        f();
    } catch (const gr::Error &) {
        failed = true;
    }
    check(failed, message);
}
Json record(const std::string &id, int value) {
    return gr::core::construct(
        {{"format", "gr.record/1"},
         {"id", id},
         {"type", "gr.assertion@1"},
         {"payload", {{"value", value}}},
         {"attribution", {{"agent", "fixture:observer"}, {"method", "reported"}}},
         {"qualifiers", {{"epistemic_status", "contested"}, {"modality", "hypothetical"}}}});
}
Json snapshot(Json records = Json::array()) {
    return {{"revision", "0"}, {"records", records}};
}
Json attribution(std::string reason) {
    return {{"agent", "fixture:recorder"}, {"method", "declared checkpoint"}, {"reason", reason}};
}
std::string head(const Json &history) {
    return history["nodes"].back()["version"].get<std::string>();
}
std::map<std::string, int> values(const Json &state) {
    std::map<std::string, int> result;
    for (const auto &r : state["records"])
        result[r["document"]["id"].get<std::string>()] = r["document"]["payload"]["value"].get<int>();
    return result;
}
} // namespace
int main() {
    try {
        Json h = {{"format", "gr.history/1"}, {"nodes", Json::array()}};
        check(gr::history::validate(h)["valid"], "empty DAG validates");
        h = gr::history::append(h, snapshot(Json::array({record("a", 1), record("b", 1)})), Json::array(),
                                attribution("initial"));
        const auto initial = head(h);
        auto baseline = h;
        check(gr::history::append(h, snapshot(Json::array({record("b", 1), record("a", 1)})), Json::array(),
                                  attribution("initial")) == h,
              "logical record order does not change checkpoint identity and exact append is idempotent");
        auto left_state =
            gr::core::apply(gr::history::reconstruct(h, initial), Json::array({record("a", 2)}));
        auto staged_projection = gr::core::match(left_state);
        check(!left_state.contains("history") && staged_projection["source_revision"].is_null() &&
                  staged_projection["source_state"] == "staged" &&
                  staged_projection["base_revision"] == initial,
              "staged edits do not claim the base's retained checkpoint identity");
        h = gr::history::append(h, left_state, Json::array({initial}), attribution("left changes a"));
        const auto left = head(h);
        auto right_state =
            gr::core::apply(gr::history::reconstruct(h, initial), Json::array({record("b", 3)}));
        h = gr::history::append(h, right_state, Json::array({initial}), attribution("right changes b"));
        const auto right = head(h);
        check(values(gr::history::reconstruct(h, initial)) == std::map<std::string, int>{{"a", 1}, {"b", 1}},
              "older checkpoint is unchanged by branches");
        auto delta = gr::history::diff(h, initial, left);
        check(delta["changed"].size() == 1 && delta["changed"][0]["id"] == "a",
              "version diff identifies change");
        auto joined = gr::history::merge(h, initial, left, right, Json::object(),
                                         attribution("explicit compatible merge"));
        check(joined["merged"], "independent edits merge");
        h = joined["history"];
        const auto merged = joined["revision"].get<std::string>();
        auto state = gr::history::reconstruct(h, merged);
        check(values(state) == std::map<std::string, int>{{"a", 2}, {"b", 3}},
              "three-way merge preserves both independent edits");
        check(state["history"]["parents"] == Json::array({left, right}), "both parent identities preserved");
        check(state["archive"].size() == 2, "pre-merge versions retained");
        check(state["records"][0]["document"]["qualifiers"]["epistemic_status"] == "contested",
              "history does not discard epistemic qualification");

        auto conflict_state =
            gr::core::apply(gr::history::reconstruct(h, initial), Json::array({record("a", 9)}));
        h = gr::history::append(h, conflict_state, Json::array({initial}),
                                attribution("alternate interpretation"));
        const auto conflict_tip = head(h);
        const auto before = h;
        auto conflict =
            gr::history::merge(h, initial, left, conflict_tip, Json::object(), attribution("must resolve"));
        check(!conflict["merged"].get<bool>() && conflict["conflicts"].size() == 1 && h == before,
              "conflict returns without mutation");
        auto resolved = gr::history::merge(h, initial, left, conflict_tip, Json{{"a", record("a", 7)}},
                                           attribution("explicit synthesis; no truth claim"));
        check(values(gr::history::reconstruct(resolved["history"], resolved["revision"]))["a"] == 7,
              "explicit conflict resolution produces a new version");
        auto absent = gr::history::merge(h, initial, left, conflict_tip, Json{{"a", nullptr}},
                                         attribution("record absence in this account"));
        auto absent_state = gr::history::reconstruct(absent["history"], absent["revision"]);
        check(!values(absent_state).contains("a") && absent_state["archive"].size() >= 3,
              "explicit absence preserves earlier versions");
        fails(
            [&] {
                gr::history::merge(h, right, left, conflict_tip, Json::object(), attribution("invalid base"));
            },
            "base must be ancestor of both tips");
        fails(
            [&] {
                gr::history::merge(h, initial, left, right, Json{{"b", record("b", 6)}},
                                   attribution("stale resolution"));
            },
            "resolution for nonconflict rejected");
        fails(
            [&] {
                gr::history::merge(h, initial, left, conflict_tip, Json{{"a", record("different", 7)}},
                                   attribution("identity mismatch"));
            },
            "resolution cannot silently change identity");
        fails(
            [&] {
                gr::history::append(h, snapshot(), Json::array({"sha256:" + std::string(64, '0')}),
                                    attribution("missing parent"));
            },
            "missing parent rejected");
        fails(
            [&] { gr::history::append(h, snapshot(), Json::array({left, left}), attribution("duplicate")); },
            "duplicate parents rejected");
        auto equal_state = gr::history::reconstruct(h, initial);
        auto another =
            gr::history::append(h, equal_state, Json::array({left}), attribution("return to prior endpoint"));
        check(head(another) != initial, "equal current states do not imply equal histories");

        auto damaged = h;
        damaged["nodes"][0]["document"]["attribution"]["reason"] = "changed";
        check(!gr::history::validate(damaged)["valid"].get<bool>(), "descriptor tamper detected");
        damaged = h;
        damaged["nodes"][0]["snapshot"]["records"][0] = record("a", 999);
        check(!gr::history::validate(damaged)["valid"].get<bool>(),
              "valid but substituted record version breaks checkpoint digest");
        damaged = h;
        damaged["nodes"].erase(damaged["nodes"].begin());
        check(!gr::history::validate(damaged)["valid"].get<bool>(), "incomplete parent closure detected");
        auto shuffled = h;
        std::reverse(shuffled["nodes"].begin(), shuffled["nodes"].end());
        check(gr::history::reconstruct(shuffled, left) == gr::history::reconstruct(h, left),
              "bundle serialization order does not define ancestry");

        // An imported base remains readable without its profile. Its constraints
        // still govern three-way resolution even when both tips replace it away.
        gr::core::TypeRegistry extended;
        extended.register_type("test.constraint@1", [](const Json &) {});
        auto constrained_document = record("constraint", 1)["document"];
        constrained_document["type"] = "test.constraint@1";
        constrained_document["required"] = true;
        Json imported = {{"format", "gr.history/1"}, {"nodes", Json::array()}};
        imported = gr::history::append(
            imported, snapshot(Json::array({gr::core::construct(constrained_document, extended)})),
            Json::array(), attribution("base with required profile"), extended);
        const auto imported_base = head(imported);
        imported = gr::history::append(imported, snapshot(Json::array({record("left", 2)})),
                                       Json::array({imported_base}), attribution("known left tip"), extended);
        const auto imported_left = head(imported);
        imported =
            gr::history::append(imported, snapshot(Json::array({record("right", 3)})),
                                Json::array({imported_base}), attribution("known right tip"), extended);
        const auto imported_right = head(imported);
        check(gr::history::validate(imported)["valid"], "imported opaque base remains readable");
        fails(
            [&] {
                gr::history::merge(imported, imported_base, imported_left, imported_right, Json::object(),
                                   attribution("unavailable base constraints"));
            },
            "merge requires base profile even when both tips no longer use it");

        // Independent generated model: copy a randomly chosen earlier map, replace
        // one value, then demand exact reconstruction for every resulting branch.
        std::mt19937 random(0x47524832);
        auto generated = baseline;
        std::vector<std::string> tips{initial};
        std::vector<std::map<std::string, int>> model{{{"a", 1}, {"b", 1}}};
        for (int step = 0; step < 36; ++step) {
            const auto parent = static_cast<std::size_t>(random() % tips.size());
            auto expected = model[parent];
            auto id = "generated:" + std::to_string(random() % 7);
            auto value = static_cast<int>(random() % 1000);
            expected[id] = value;
            auto next = gr::core::apply(gr::history::reconstruct(generated, tips[parent]),
                                        Json::array({record(id, value)}));
            generated = gr::history::append(generated, next, Json::array({tips[parent]}),
                                            attribution("generated step " + std::to_string(step)));
            tips.push_back(head(generated));
            model.push_back(expected);
        }
        for (std::size_t i = 0; i < tips.size(); ++i)
            check(values(gr::history::reconstruct(generated, tips[i])) == model[i],
                  "generated branch reconstruction matches independent model");
        check(gr::history::validate(generated)["valid"], "generated DAG validates");
        std::cout << "history: " << checks << " checks passed, including 36 generated branching revisions\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "history failure: " << e.what() << '\n';
        return 1;
    }
}
