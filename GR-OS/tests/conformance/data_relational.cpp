#include "gr/data/relational.hpp"
#include "gr/execution/cpu.hpp"
#include "gr/storage/memory_store.hpp"
#include <atomic>
#include <iostream>
#include <thread>

using gr::Json;
namespace d = gr::data;
namespace rel = gr::data::relational;
namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
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
Json doc(const std::string &id, const std::string &type = "gr.entity@1") {
    return {{"format", "gr.record/1"},
            {"id", id},
            {"type", type},
            {"payload", {{"label", id}}},
            {"attribution", {{"agent", "fixture:observer"}, {"method", "reported"}}}};
}
Json participant(const std::string &id, const std::string &role, const std::string &scope = "record") {
    return {{"role", role}, {"reference", {{"scope", scope}, {"id", id}}}};
}
Json relation(const std::string &id, Json participants) {
    auto value = doc(id, "gr.relation@1");
    value["participants"] = std::move(participants);
    value["qualifiers"] = {{"status", "contested"}, {"uncertainty", "not quantified"}};
    return value;
}
Json order(const std::string &id, const std::string &before, const std::string &after) {
    auto value = doc(id, "gr.order@1");
    value["order"] = {{"before", before}, {"after", after}};
    return value;
}
struct Fixture {
    std::shared_ptr<gr::storage::MemoryStore> store = std::make_shared<gr::storage::MemoryStore>();
    std::shared_ptr<gr::core::TypeRegistry> registry = std::make_shared<gr::core::TypeRegistry>();
    std::unique_ptr<gr::runtime::TransactionCoordinator> coordinator;
    int next = 0;
    Fixture() {
        registry->register_extension("fixture.opaque@1", [](const Json &) {});
        store->create_workspace("relations");
        coordinator = std::make_unique<gr::runtime::TransactionCoordinator>(store, registry);
    }
    void publish(const Json &documents) {
        gr::runtime::Stage stage;
        for (const auto &document : documents)
            stage.records.push_back(gr::core::construct(document, *registry));
        gr::runtime::TransactionOptions options;
        options.workspace = "relations";
        options.transaction_id = "fixture:" + std::to_string(++next);
        options.actor = "fixture:recorder";
        options.reason = "Synthetic relational-history conformance";
        options.track_history = true;
        coordinator->commit(options, stage);
    }
    d::DataValue select(const std::string &semantic = "", const std::string &physical = "") {
        return rel::select_history(*coordinator, {"relations", semantic, physical});
    }
};
std::shared_ptr<d::Library> library() {
    auto lib = std::make_shared<d::Library>();
    rel::install(*lib);
    lib->seal();
    return lib;
}
d::CallOptions call_options(const std::string &backend) {
    d::CallOptions options;
    options.selection.backend = backend;
    options.max_input_bytes = 8 * 1024 * 1024;
    options.max_output_bytes = 8 * 1024 * 1024;
    return options;
}
struct Evaluated {
    d::DataValue value;
    Json inspected;
};
Evaluated evaluate(const std::shared_ptr<d::Library> &lib, const d::DataValue &source,
                   Json temporal = nullptr, Json roles = nullptr, bool count = false,
                   const std::string &backend = "cpu.sequential") {
    auto expression = d::literal(lib, source);
    const Json attrs = temporal.is_null() ? Json::object() : Json{{"temporal", temporal}};
    expression = d::apply(lib, rel::project_operation, {expression}, attrs, call_options(backend));
    if (!roles.is_null())
        expression =
            d::apply(lib, rel::roles_operation, {expression}, {{"roles", roles}}, call_options(backend));
    if (count)
        expression = d::apply(lib, rel::count_operation, {expression}, Json::object(), call_options(backend));
    auto program = d::compile({expression});
    gr::runtime::RuntimeConfig config;
    config.workers = 2;
    gr::runtime::Runtime runtime(config);
    auto result = runtime.run(program.plan());
    if (result.status != gr::runtime::JobStatus::succeeded)
        throw std::runtime_error("Qualified execution failed: " + result.error.dump());
    auto values = program.materialize(result);
    check(values.size() == 1 && !values[0].provenance.empty(), "Materialized value carries native lineage");
    auto inspected = rel::inspect(values[0]);
    return {std::move(values[0]), std::move(inspected)};
}
const Json &record(const Json &qualified, const std::string &id, const char *field = "records") {
    for (const auto &r : qualified.at("source").at(field))
        if (r.at("document").at("id") == id)
            return r;
    throw std::runtime_error("Missing test record");
}
void structural_history() {
    Fixture f;
    auto a = doc("a"), b = doc("b");
    const auto old_a = gr::core::construct(a);
    auto r = relation("r", Json::array({participant("a", "actor"), participant("b", "witness"),
                                        participant("a", "observer"), participant("a", "actor"),
                                        participant("unresolved", "actor", "subject")}));
    r["participants"][0]["orientation"] = "out";
    r["references"] = Json::array({{{"scope", "subject"}, {"id", "evidence:unretrieved"}}});
    auto parallel = relation("parallel", Json::array({participant("a", "actor")}));
    auto higher = relation("higher", Json::array({participant("r", "subject-of-claim")}));
    f.publish(Json::array({a, b, r, parallel, higher}));
    auto original = f.select();
    auto original_json = rel::inspect(original);
    const auto semantic = original.provenance.at("semantic_revision").get<std::string>();
    const auto physical = original.provenance.at("physical_revision").get<std::string>();
    a["payload"]["label"] = "revised label";
    r["participants"][0]["reference"]["version"] = old_a.at("version");
    f.publish(Json::array({a, r}));
    auto current = f.select();
    const auto before = f.store->head("relations");
    auto lib = library();
    const auto result = evaluate(lib, current);
    check(result.inspected.at("incidences").size() == 7,
          "N-ary, repeated and parallel occurrences remain distinct");
    check(record(result.inspected, "r").at("document").at("qualifiers") == r.at("qualifiers"),
          "Claim qualifiers and uncertainty remain in full source wrapper");
    check(record(result.inspected, "a").at("document").at("payload").at("label") == "revised label" &&
              record(result.inspected, "a", "archive").at("version") == old_a.at("version"),
          "Current and pinned archived versions coexist without rebinding");
    bool found_pinned = false, found_higher = false, found_subject = false;
    for (const auto &i : result.inspected.at("incidences")) {
        if (i.at("relation") == "r" && i.at("ordinal") == 0)
            found_pinned = i.at("resolution").at("version") == old_a.at("version") &&
                           i.at("participant").at("orientation") == "out";
        if (i.at("relation") == "higher")
            found_higher = i.at("resolution").at("id") == "r";
        if (i.at("resolution").at("scope") == "subject")
            found_subject = i.at("resolution").at("version").is_null();
    }
    check(found_pinned && found_higher && found_subject,
          "Pinned, higher-order and subject references retain distinct meanings");
    auto selected = evaluate(lib, current, nullptr, Json::array({"actor"}), true);
    auto threaded = evaluate(lib, current, nullptr, Json::array({"actor"}), true, "cpu.threaded");
    check(selected.inspected == threaded.inspected,
          "Sequential and threaded providers preserve exact qualified result");
    check(selected.inspected.at("incidences").size() == 4 &&
              selected.inspected.at("excluded_incidences").size() == 3,
          "Role selection exposes included and excluded occurrence inventory");
    std::size_t counted = 0;
    for (const auto &g : selected.inspected.at("counts")) {
        counted += g.at("count").get<std::size_t>();
        check(g.at("count").get<std::size_t>() == g.at("occurrences").size(),
              "Count preserves exact contributor keys");
    }
    check(counted == 4, "Count sums selected incidences, including repeated occurrences");
    check(rel::inspect(f.select(semantic, physical)) == original_json &&
              rel::inspect(original) == original_json,
          "Pinned old semantic and physical views remain byte-stable after head advances");
    const auto recent_physical_old_semantic = rel::inspect(f.select(semantic));
    check(record(recent_physical_old_semantic, "a").at("version") == old_a.at("version") &&
              recent_physical_old_semantic.at("source").at("provenance").at("physical_revision") == before,
          "Selecting old semantic state through a new physical manifest retains both distinct pins");
    check(f.store->head("relations") == before, "Relational expressions never mutate source storage");
    auto empty = evaluate(lib, current, nullptr, Json::array(), true);
    check(empty.inspected.at("counts").empty() && empty.inspected.at("excluded_incidences").size() == 7,
          "Empty explicit role selection means no selected occurrences, with visible omissions");

    auto tampered = selected.inspected;
    tampered["counts"][0]["count"] = 900;
    auto text = tampered.dump();
    auto forged = selected.value;
    forged.payload = gr::kernel::Value::bytes(rel::representation, {text.begin(), text.end()});
    fails([&] { lib->validate(forged); }, "Forged counts are rejected against retained source occurrences");
    tampered = original_json;
    tampered["source"]["records"][0]["document"]["payload"]["label"] = "forged";
    text = tampered.dump();
    forged = original;
    forged.payload = gr::kernel::Value::bytes(rel::representation, {text.begin(), text.end()});
    fails([&] { rel::inspect(forged); },
          "Modified source record is rejected despite unchanged provenance fields");
    fails([&] { d::apply(lib, rel::count_operation, {d::literal(lib, original)}); },
          "Logical family mismatch fails before execution");
    fails([&] { d::apply(lib, rel::project_operation, {d::literal(lib, original)}, {{"truth", true}}); },
          "Unknown interpretation attributes cannot be silently ignored");
}
void temporal_accounts() {
    Fixture f;
    auto relation_a = relation("a", Json::array({participant("subject:a", "participant", "subject")}));
    relation_a["temporal"] = {{"start", {{"kind", "event"}, {"id", "e0"}}},
                              {"end", {{"kind", "event"}, {"id", "e1"}}}};
    auto unknown = relation("unknown", relation_a["participants"]);
    unknown["temporal"] = {{"start", {{"kind", "unknown"}}}, {"end", {{"kind", "unbounded"}}}};
    auto unbounded = relation("unbounded", relation_a["participants"]);
    unbounded["temporal"] = {{"start", {{"kind", "unbounded"}}}, {"end", {{"kind", "unbounded"}}}};
    auto absent = relation("absent", relation_a["participants"]);
    f.publish(Json::array(
        {doc("e0", "gr.event@1"), doc("e1", "gr.event@1"), relation_a, unknown, unbounded, absent}));
    auto old = f.select();
    auto lib = library();
    const Json cut = {{"after", Json::array({"e0"})}};
    auto prior = evaluate(lib, old, cut);
    check(prior.inspected.at("temporal").at("cut").at("indeterminate").size() == 2,
          "Unestablished event interval and unknown endpoint remain indeterminate");
    f.publish(Json::array({order("retrospective", "e0", "e1")}));
    auto now = evaluate(lib, f.select(), cut);
    const auto &classification = now.inspected.at("temporal").at("cut");
    check(classification.at("active").size() == 2 && classification.at("indeterminate").size() == 1 &&
              std::find(classification.at("unscoped").begin(), classification.at("unscoped").end(),
                        Json("absent")) != classification.at("unscoped").end(),
          "Retrospective order changes only new recording account; unknown, unbounded and absent remain "
          "distinct");
    check(evaluate(lib, old, cut).inspected == prior.inspected,
          "Later recorded order does not alter frozen historical input");
    f.publish(Json::array({order("competing", "e1", "e0")}));
    auto cycle = evaluate(lib, f.select(), cut);
    check(!cycle.inspected.at("temporal").at("consistent").get<bool>() &&
              cycle.inspected.at("temporal").at("cut").is_null() &&
              cycle.inspected.at("incidences").size() == 4,
          "Cyclic competing accounts retain incidence records but establish no temporal cut");
    auto scoped = evaluate(lib, f.select(),
                           {{"order_ids", Json::array({"retrospective"})}, {"after", Json::array({"e0"})}});
    check(scoped.inspected.at("temporal").at("consistent").get<bool>() &&
              scoped.inspected.at("temporal").at("excluded_orders") == Json::array({"competing"}),
          "Explicit order selection exposes excluded competing claim");
    auto opaque = order("opaque", "e0", "e1");
    opaque["extensions"] =
        Json::array({{{"type", "fixture.opaque@1"}, {"required", true}, {"data", Json::object()}}});
    f.publish(Json::array({opaque}));
    auto unsupported =
        evaluate(lib, f.select(), {{"order_ids", Json::array({"opaque"})}, {"after", Json::array({"e0"})}});
    check(!unsupported.inspected.at("temporal").at("interpretation_complete").get<bool>() &&
              unsupported.inspected.at("temporal").at("cut").is_null(),
          "Unknown required order extension blocks temporal interpretation without discarding source");
}
void large_source() {
    Fixture f;
    Json docs = Json::array();
    for (int i = 0; i < 3; ++i) {
        auto value = relation("large:" + std::to_string(i),
                              Json::array({participant("designation", "actor", "subject")}));
        value["payload"]["retained_text"] = std::string(400000, static_cast<char>('a' + i));
        docs.push_back(std::move(value));
    }
    f.publish(docs);
    auto source = f.select();
    check(source.payload.byte_size() > 1024 * 1024,
          "Bytes carrier admits complete qualified source above metadata 1 MiB cap");
    auto result = evaluate(library(), source, nullptr, nullptr, true);
    check(result.inspected.at("incidences").size() == 3 && result.inspected.at("counts")[0].at("count") == 3,
          "Large pinned source executes through common language and runtime");
    auto lib = library();
    auto expr = d::literal(lib, source);
    auto small = call_options("cpu.sequential");
    small.max_input_bytes = 1024;
    fails(
        [&] {
            auto program = d::compile({d::apply(lib, rel::project_operation, {expr}, Json::object(), small)});
            gr::runtime::Runtime runtime;
            auto result = runtime.run(program.plan());
            if (result.status != gr::runtime::JobStatus::succeeded)
                throw gr::Error("bounded", "Runtime rejected source over declared input budget");
        },
        "Declared input byte budget rejects oversized relational execution");
}
void immutable_cache() {
    Fixture f;
    f.publish(Json::array(
        {doc("a"), relation("r", Json::array({participant("a", "actor"), participant("a", "witness")}))}));
    auto source = f.select();
    const auto expected = rel::inspect(source);
    // Disable the independent generic cache to exercise this installation's memo.
    auto lib = std::make_shared<d::Library>(d::ValidationCacheLimits{0, 0, false});
    auto session = rel::install(*lib, {}, {4, 1024 * 1024, true});
    lib->seal();
    check(session->inspect(source) == expected && session->inspect(source) == expected,
          "Scoped inspection preserves independently validated complete source");
    auto stats = session->statistics();
    check(stats.at("full_checks") == 1 && stats.at("hits") == 1 && stats.at("source_checks") == 1,
          "Same immutable owner reuses one successful full check");
    auto alias = source;
    alias.payload.type = "fixture.transport@1";
    check(session->inspect(alias) == expected && session->statistics().at("hits") == 2,
          "Transport retag does not change the immutable qualified representation");
    auto copy = source;
    copy.payload = gr::kernel::Value::bytes(rel::representation, source.payload.as_bytes());
    check(session->inspect(copy) == expected, "New owner of equal bytes remains valid");
    stats = session->statistics();
    check(stats.at("full_checks") == 2 && stats.at("source_hits") == 1,
          "A distinct owner is decoded and checked; only exact validated source bytes are reused");
    auto wrong_family = source;
    wrong_family.spec.type = rel::counts_type();
    fails([&] { session->inspect(wrong_family); }, "Owner reuse cannot certify a different family");
    fails([&] { session->inspect(wrong_family); }, "Failed interpretation is never memoized");
    check(session->statistics().at("failures") == 2, "Failure statistics count each rejected attempt");
    auto forged = source;
    auto altered = expected;
    altered["source"]["records"][0]["document"]["payload"]["label"] = "changed";
    auto wire = altered.dump();
    forged.payload = gr::kernel::Value::bytes(rel::representation, {wire.begin(), wire.end()});
    fails([&] { session->inspect(forged); },
          "Warm provenance lookup cannot certify changed source bytes with unchanged revision claims");
    wire = expected.dump();
    wire.insert(1, "\"kind\":\"records\",");
    forged.payload = gr::kernel::Value::bytes(rel::representation, {wire.begin(), wire.end()});
    fails([&] { session->inspect(forged); }, "Warm owner cache does not bypass duplicate-key parsing");

    auto off_lib = std::make_shared<d::Library>(d::ValidationCacheLimits{0, 0, false});
    auto off = rel::install(*off_lib, {}, {0, 0, false});
    off_lib->seal();
    off->inspect(source);
    off->inspect(source);
    check(off->statistics().at("full_checks") == 2 && off->statistics().at("hits") == 0 &&
              off->statistics().at("timing_ns").at("decode") == 0,
          "Disabled cache performs fresh validation and disabled timing does not read the clock");
    auto on_value = evaluate(lib, source, nullptr, Json::array({"actor"}), true);
    auto off_value = evaluate(off_lib, source, nullptr, Json::array({"actor"}), true);
    check(on_value.value.payload.as_bytes() == off_value.value.payload.as_bytes(),
          "Memoized and uncached operator pipelines emit identical v1 qualified bytes");
    auto repeated = d::apply(lib, rel::project_operation, {d::literal(lib, source)});
    repeated = d::apply(lib, rel::roles_operation, {repeated}, {{"roles", {"actor"}}});
    repeated = d::apply(lib, rel::roles_operation, {repeated}, {{"roles", {"witness", "actor"}}});
    repeated = d::apply(lib, rel::count_operation, {repeated});
    auto program = d::compile({repeated});
    gr::runtime::Runtime runtime;
    auto run = runtime.run(program.plan());
    check(run.status == gr::runtime::JobStatus::succeeded, "Repeated role intersection executes");
    check(program.materialize(run).at(0).payload.as_bytes() == on_value.value.payload.as_bytes(),
          "Repeated selections retain earlier exclusions and canonical occurrence order");

    d::Library limited;
    auto small = rel::install(limited, {}, {1, 1024 * 1024, false});
    small->inspect(source);
    small->inspect(copy);
    auto bounded = small->statistics();
    check(bounded.at("entries") == 1 && bounded.at("evictions") == 1 &&
              bounded.at("charged_bytes").get<std::size_t>() <= 1024 * 1024,
          "LRU bounds entries and conservative serialized-state charges");
    small->clear_cache();
    check(small->statistics().at("entries") == 0 && small->statistics().at("charged_bytes") == 0,
          "Explicit clear releases memo entries and charges");
    {
        auto temporary = source;
        temporary.payload = gr::kernel::Value::bytes(rel::representation, source.payload.as_bytes());
        small->inspect(temporary);
    }
    check(small->statistics().at("entries") == 0 && small->statistics().at("expired") == 1,
          "Memo does not extend immutable payload lifetime or permit recycled-address reuse");
    d::Library no_room;
    auto tiny = rel::install(no_room, {}, {4, 1, false});
    tiny->inspect(source);
    tiny->inspect(source);
    check(tiny->statistics().at("entries") == 0 && tiny->statistics().at("full_checks") == 2,
          "An entry above the byte budget is checked without retention");

    session->clear_cache();
    std::atomic<bool> valid{true};
    std::vector<std::jthread> readers;
    for (int t = 0; t < 4; ++t)
        readers.emplace_back([&] {
            try {
                for (int i = 0; i < 8; ++i) {
                    if (session->inspect(source) != expected)
                        valid = false;
                    if (!(i % 3))
                        session->clear_cache();
                }
            } catch (...) {
                valid = false;
            }
        });
    readers.clear();
    check(valid && session->statistics().at("entries").get<std::size_t>() <= 4,
          "Concurrent immutable readers and clear remain correct and bounded");

    // The same owner is interpreted independently by distinct pinned registries.
    d::Library rejects;
    auto custom = std::make_shared<gr::core::TypeRegistry>();
    custom->register_extension(
        "fixture.opaque@1", [](const Json &) { throw gr::Error("fixture_reject", "Reject test extension"); });
    auto separate = rel::install(rejects, custom, {4, 1024 * 1024, false});
    check(separate->inspect(source) == expected && separate->statistics().at("full_checks") == 1,
          "Separate installations never inherit another pinned registry's owner certificate");
    auto extended = doc("qualified");
    extended["extensions"] =
        Json::array({{{"type", "fixture.opaque@1"}, {"required", true}, {"data", Json::object()}}});
    f.publish(Json::array({extended}));
    auto qualified = f.select();
    session->inspect(qualified); // Unknown extension is preserved without interpretation.
    fails([&] { separate->inspect(qualified); },
          "A different installed profile validates the same owner independently");
}
void owning_inspection_view() {
    rel::QualifiedView retained;
    std::weak_ptr<const void> owner;
    {
        Fixture fixture;
        fixture.publish(Json::array({doc("view:entity")}));
        auto source = fixture.select();
        owner = source.payload.owner_token();
        auto lib = std::make_shared<d::Library>();
        auto session = rel::install(*lib);
        retained = session->view(source);
        auto second = session->view(source);
        check(&retained.source() == &second.source() && &retained.derivation() == &second.derivation(),
              "Owning inspections share immutable parsed source and derivation");
        source.provenance["caller_mutation"] = true;
        check(!retained.value().provenance.contains("caller_mutation"),
              "View pins computational provenance independently of caller metadata");
        session->clear_cache();
        check(retained.source().at("records").size() == 1 && retained.derivation().at("kind") == "records",
              "Explicit cache clear does not invalidate a live inspection lease");
        source.provenance["invalid_number"] = 1.25;
        fails([&] { session->view(source); }, "Inspection rejects invalid computational metadata");
        source.provenance = Json::array();
        fails([&] { session->view(source); }, "Inspection requires object-shaped computational provenance");
    }
    check(!owner.expired() && retained.source().at("records").size() == 1,
          "View retains source after input, Session, Library and store destruction");
    retained = {};
    check(owner.expired(), "Final inspection lease release ends raw owner lifetime");
    fails([&] { retained.source(); }, "Empty inspection view fails coherently");
}
} // namespace
int main() {
    try {
        structural_history();
        temporal_accounts();
        large_source();
        immutable_cache();
        owning_inspection_view();
        std::cout << checks << " qualified relational/history checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
