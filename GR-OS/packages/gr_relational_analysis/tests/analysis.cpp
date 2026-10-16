#include "fixture.hpp"
#include <atomic>
#include <iostream>
#include <thread>

namespace a = gr_relational_analysis;
namespace f = analysis_fixture;
using gr::Json;
namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void fails(F &&fn, const std::string &code) {
    ++checks;
    try {
        fn();
    } catch (const gr::Error &e) {
        if (e.code == code)
            return;
        throw std::runtime_error("Expected " + code + ", got " + e.code + ": " + e.what());
    }
    throw std::runtime_error("Expected " + code);
}
void resign(Json &j) {
    j.erase("snapshot_id");
    j["snapshot_id"] = "sha256:" + gr::sha256(j.dump());
}
const Json &degree(const Json &summary, const std::string &key) {
    for (const auto &r : summary.at("degrees"))
        if (r.at("vertex") == key)
            return r;
    throw std::runtime_error("Missing degree row");
}
void structure_and_portability() {
    f::Fixture fixture;
    fixture.small();
    auto value = fixture.incidence(fixture.selected());
    auto network = a::analyze(value, fixture.session);
    const auto snapshot = network.snapshot();
    auto summary = a::summarize(network);
    check(summary.at("owners") == 5 && summary.at("targets") == 5 && summary.at("occurrences") == 7,
          "Hand-computed owner, target and occurrence counts include unknown profiles and repeated roles");
    check(summary.at("components").size() == 4, "Bipartite components include one isolated zero-arity owner");
    check(summary.at("role_counts") == Json{{"actor", 4}, {"witness", 1}, {"observer", 2}},
          "Role histogram exact");
    check(summary.at("temporal_counts") == Json{{"not_requested", 7}},
          "No chronology inferred without request");
    bool repeated = false, higher = false, disjoint = false;
    for (const auto &v : snapshot.vertices()) {
        auto d = degree(summary, v.key);
        if (v.kind == a::VertexKind::participant_owner && v.id == "r")
            repeated = d.at("occurrence_degree") == 4 && d.at("distinct_neighbor_degree") == 3;
        if (v.kind == a::VertexKind::participant_target && v.id == "a")
            check(d.at("occurrence_degree") == 3 && d.at("distinct_neighbor_degree") == 2,
                  "Target degree counts repeat occurrences separately from distinct owners");
        if (v.kind == a::VertexKind::participant_target && v.id == "r")
            disjoint = true;
    }
    for (const auto &o : snapshot.occurrences())
        if (o.relation == "higher")
            higher = snapshot.vertices().at(o.target).id == "r";
    check(repeated && higher && disjoint,
          "N-ary, repeated and higher-order incidences preserve distinct owner/target roles");
    check(network.source().source().at("records").size() == 7 &&
              snapshot.context().at("qualifications").size() > 0,
          "Complete qualified source custody and declared caveats remain available");
    auto shared = network.snapshot();
    check(&shared.vertices() == &snapshot.vertices(), "Native snapshots share immutable topology");
    auto encoded = snapshot.encode();
    auto imported = a::NetworkSnapshot::decode(
        std::span(reinterpret_cast<const std::uint8_t *>(encoded.data()), encoded.size()));
    check(imported.encode() == encoded && imported.id() == snapshot.id(),
          "Portable deterministic snapshot round trip");
    bool annotation = false;
    for (const auto &o : imported.occurrences())
        if (o.relation == "r" && o.ordinal == 0)
            annotation = o.participant.at("reference").at("basis") == "opaque reference annotation";
    check(annotation, "Opaque reference annotations survive portable round trip without interpretation");
    check(imported.validation_basis() == "structural_only" && snapshot.validation_basis() == "source_derived",
          "Import never promotes omitted-source validation claims");
    auto imported_summary = a::summarize(imported);
    auto direct_summary = summary;
    imported_summary.erase("validation_basis");
    direct_summary.erase("validation_basis");
    check(imported_summary == direct_summary,
          "Shared native and imported topology produce equal statistics with explicit basis difference");
    auto selected =
        a::analyze(fixture.incidence(fixture.selected(), Json::array({"actor"})), fixture.session);
    auto selected_summary = a::summarize(selected);
    check(selected_summary.at("owners") == 5 && selected_summary.at("targets") == 2 &&
              selected_summary.at("occurrences") == 4 && selected_summary.at("excluded_occurrences") == 3 &&
              selected_summary.at("components").size() == 4,
          "Role filtering retains isolated owners and explicit excluded occurrence inventory");
    fails([&] { a::compare(network, selected); }, "analysis_selection");
    auto empty = a::analyze(fixture.incidence(fixture.selected(), Json::array()), fixture.session);
    check(a::summarize(empty).at("components").size() == 5 && empty.snapshot().occurrences().empty(),
          "Empty role selection means no edges, not unrestricted selection");
    auto different_provenance = value;
    different_provenance.provenance["analysis_perspective"] = "separate caller";
    auto variant = a::analyze(different_provenance, fixture.session);
    check(variant.identity().at("source_value_id") != network.identity().at("source_value_id") &&
              variant.snapshot().id() != snapshot.id(),
          "Computational-provenance-only change alters value and projection identity");
    check(variant.snapshot().to_json().at("vertices") == snapshot.to_json().at("vertices"),
          "Provenance identity does not invent topology changes");
    auto wrong = snapshot.to_json();
    wrong["validation_basis"] = "source_derived";
    resign(wrong);
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_contract");
    wrong = snapshot.to_json();
    wrong["occurrences"][0]["target"] = 999;
    resign(wrong);
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_contract");
    wrong = snapshot.to_json();
    wrong["context"]["pins"]["workspace"] = "forged";
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_integrity");
    wrong = snapshot.to_json();
    wrong["occurrences"][0]["participant"]["reference"]["scope"] = "unsupported";
    resign(wrong);
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_contract");
    wrong = snapshot.to_json();
    wrong["occurrences"][0]["participant"]["reference"].erase("id");
    resign(wrong);
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_contract");
    wrong = snapshot.to_json();
    wrong["occurrences"][0]["participant"]["orientation"] = 42;
    resign(wrong);
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_contract");
    wrong = snapshot.to_json();
    wrong["occurrences"][0]["temporal_class"] = "active";
    resign(wrong);
    fails([&] { a::NetworkSnapshot::from_json(wrong); }, "analysis_contract");
    auto duplicate = encoded;
    duplicate.insert(1, "\"format\":\"gr.analysis.incidence-snapshot/1\",");
    fails(
        [&] {
            a::NetworkSnapshot::decode(
                std::span(reinterpret_cast<const std::uint8_t *>(duplicate.data()), duplicate.size()));
        },
        "duplicate_key");
    fails([&] { a::NetworkSnapshot{}.encode(); }, "analysis_contract");
    fails([&] { a::analyze(value, fixture.session, {1, 100, 1024 * 1024}); }, "analysis_limit");
    fails(
        [&] {
            a::NetworkSnapshot::decode(
                std::span(reinterpret_cast<const std::uint8_t *>(encoded.data()), encoded.size()),
                {100, 100, 100});
        },
        "analysis_limit");
    std::stop_source stop;
    stop.request_stop();
    fails([&] { a::analyze(value, fixture.session, {}, stop.get_token()); }, "execution_cancelled");
    fails([&] { a::summarize(network, stop.get_token()); }, "execution_cancelled");
    fails([&] { a::compare(network, network, stop.get_token()); }, "execution_cancelled");
    std::atomic<bool> valid{true};
    std::vector<std::jthread> threads;
    for (int i = 0; i < 4; ++i)
        threads.emplace_back([&] {
            try {
                for (int j = 0; j < 3; ++j)
                    if (a::summarize(network) != summary)
                        valid = false;
            } catch (...) {
                valid = false;
            }
        });
    threads.clear();
    check(valid, "Concurrent direct analysis reads share immutable data safely");
}
void historical_comparison() {
    f::Fixture fixture;
    fixture.small();
    auto source = fixture.selected();
    const auto old_semantic = source.provenance.at("semantic_revision").get<std::string>();
    auto before = a::analyze(fixture.incidence(source), fixture.session);
    auto revised = f::document("a");
    revised["payload"]["label"] = "new label";
    fixture.publish(Json::array({revised}));
    auto after = a::analyze(fixture.incidence(fixture.selected()), fixture.session);
    auto delta = a::compare(before, after);
    check(delta.at("records").at("changed").size() == 1 && delta.at("occurrences").at("changed").size() == 3,
          "Unchanged occurrence identities detect newly resolved versions of unpinned targets");
    check(delta.at("occurrences").at("added").empty() && delta.at("occurrences").at("removed").empty(),
          "Target revision is not a new participant occurrence");
    auto repinned = a::analyze(fixture.incidence(fixture.selected(old_semantic)), fixture.session);
    auto pins_delta = a::compare(before, repinned);
    check(pins_delta.at("records").at("changed").empty() &&
              pins_delta.at("occurrences").at("changed").empty() &&
              before.snapshot().id() != repinned.snapshot().id(),
          "Same semantic snapshot through newer physical manifest preserves topology and changes provenance "
          "identity");
    auto r = f::relation("r", Json::array({f::participant("a", "actor"), f::participant("a", "actor"),
                                           f::participant("b", "witness"),
                                           f::participant("designation", "actor", "subject")}));
    const auto &old_records = before.source().source().at("records");
    for (const auto &x : old_records)
        if (x.at("document").at("id") == "a")
            r["participants"][0]["reference"]["version"] = x.at("version");
    fixture.publish(Json::array({r}));
    auto pinned = a::analyze(fixture.incidence(fixture.selected()), fixture.session);
    check(a::summarize(pinned).at("targets") == 6 && pinned.source().source().at("archive").size() >= 1,
          "Pinned archived and selected current target versions remain different vertices");
    auto revised_owner = a::compare(after, pinned);
    check(revised_owner.at("occurrences").at("added").size() == 4 &&
              revised_owner.at("occurrences").at("removed").size() == 4,
          "Owner record revision produces new exact occurrence keys without invented persistence");
    f::Fixture other("analysis:other");
    other.small();
    auto other_network = a::analyze(other.incidence(other.selected()), other.session);
    fails([&] { a::compare(before, other_network); }, "analysis_selection");
}
void temporal_and_composition() {
    f::Fixture fixture;
    auto r = f::relation("time:r", Json::array({f::participant("time:subject", "actor", "subject")}));
    r["temporal"] = {{"start", {{"kind", "event"}, {"id", "e0"}}}, {"end", {{"kind", "unknown"}}}};
    fixture.publish(Json::array({f::document("e0", "gr.event@1"), f::document("e1", "gr.event@1"), r,
                                 f::order("ab", "e0", "e1"), f::order("ba", "e1", "e0")}));
    auto value = fixture.incidence(fixture.selected(), nullptr, {{"after", {"e0"}}});
    auto n = a::analyze(value, fixture.session);
    check(n.snapshot().occurrences().size() == 1 &&
              n.snapshot().occurrences()[0].temporal_class == "unclassified" &&
              n.snapshot().context().at("temporal").at("cut").is_null(),
          "Cyclic temporal claims preserve occurrences without manufacturing chronology");
    auto unknown = f::order("unknown", "e0", "e1");
    unknown["extensions"] =
        Json::array({{{"type", "fixture.opaque@1"}, {"required", true}, {"data", Json::object()}}});
    fixture.publish(Json::array({unknown}));
    auto unresolved = a::analyze(
        fixture.incidence(fixture.selected(), nullptr, {{"order_ids", {"unknown"}}, {"after", {"e0"}}}),
        fixture.session);
    check(!unresolved.snapshot().context().at("temporal").at("interpretation_complete").get<bool>() &&
              unresolved.snapshot().occurrences()[0].temporal_class == "unclassified",
          "Unknown required temporal extension preserves source while withholding an interpreted cut");
    auto records = fixture.selected();
    auto input = f::d::input(fixture.library, "records", {f::rel::records_type(), f::rel::representation});
    auto incidence = f::d::apply(fixture.library, f::rel::project_operation, {input});
    auto summary = f::d::apply(fixture.library, a::summary_operation, {incidence});
    gr::data::Function function({incidence, summary});
    auto program = function.bind({{"records", records}});
    auto result = fixture.runtime.run(program.plan());
    check(result.status == gr::runtime::JobStatus::succeeded,
          "Independent package summary lowers into existing Runtime");
    auto outputs = program.materialize(result);
    auto report = a::inspect_summary(outputs.at(1));
    auto direct = a::summarize(a::analyze(outputs.at(0), fixture.session));
    for (auto key :
         {"degrees", "components", "role_counts", "temporal_counts", "owners", "targets", "occurrences"})
        check(report.at(key) == direct.at(key), "Prepared and direct statistics agree");
    check(
        report.at("source_payload_digest") == outputs.at(0).payload.content_digest() &&
            !outputs.at(1).provenance.empty() && !report.contains("source_value_id"),
        "Carrier-scoped provider report delegates complete computational lineage to Program materialization");
    auto again = program.materialize(fixture.runtime.run(program.plan()));
    check(again[1].payload == outputs[1].payload,
          "Prepared summary reuses one plan with exact deterministic results");
    auto forged = report;
    forged["owners"] = 999;
    auto bytes = forged.dump();
    auto bad = outputs[1];
    bad.payload = gr::kernel::Value::bytes(a::summary_representation, {bytes.begin(), bytes.end()});
    fails([&] { a::inspect_summary(bad); }, "analysis_contract");
    gr::data::CallOptions small;
    small.max_output_bytes = 128;
    auto limited =
        f::d::compile({f::d::apply(fixture.library, a::summary_operation,
                                   {f::d::literal(fixture.library, value)}, Json::object(), small)});
    check(fixture.runtime.run(limited.plan()).status == gr::runtime::JobStatus::failed,
          "Scheduled result budget rejects oversized output");
}
void lifetime() {
    a::NetworkView retained;
    std::weak_ptr<const void> owner;
    {
        f::Fixture fixture;
        fixture.small();
        auto value = fixture.incidence(fixture.selected());
        owner = value.payload.owner_token();
        retained = a::analyze(value, fixture.session);
        fixture.session->clear_cache();
    }
    check(!owner.expired() && a::summarize(retained).at("occurrences") == 7,
          "Network retains qualified payload and parsed state after all producer contexts close");
    auto snapshot = retained.snapshot();
    retained = {};
    check(owner.expired() && a::summarize(snapshot).at("occurrences") == 7,
          "Shared compact topology survives without retaining omitted source payload");
}
void large_portable_projection() {
    f::Fixture fixture("analysis:large");
    Json documents = Json::array();
    for (int i = 0; i < 3; ++i) {
        auto participant = f::participant("large:designation", "actor", "subject");
        participant["opaque_annotation"] = std::string(380000, static_cast<char>('a' + i));
        documents.push_back(f::relation("large:" + std::to_string(i), Json::array({participant})));
    }
    fixture.publish(documents);
    auto network = a::analyze(fixture.incidence(fixture.selected()), fixture.session);
    auto encoded = network.snapshot().encode();
    check(encoded.size() > 1024 * 1024, "Portable topology bytes can exceed the metadata 1 MiB bound");
    auto decoded = a::NetworkSnapshot::decode(
        std::span(reinterpret_cast<const std::uint8_t *>(encoded.data()), encoded.size()));
    check(decoded.id() == network.snapshot().id() && decoded.occurrences().size() == 3 &&
              decoded.occurrences()[0]
                      .participant.at("opaque_annotation")
                      .get_ref<const std::string &>()
                      .size() == 380000,
          "Large opaque participant annotations survive bounded structural import exactly");
    fails([&] { a::analyze(network.source().value(), fixture.session, {100, 100, 1024 * 1024}); },
          "analysis_limit");
}
} // namespace
int main() {
    try {
        structure_and_portability();
        historical_comparison();
        temporal_and_composition();
        lifetime();
        large_portable_projection();
        std::cout << checks << " independent relational analysis checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
