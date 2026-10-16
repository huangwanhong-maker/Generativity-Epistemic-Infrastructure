#pragma once
#include <gr/storage/memory_store.hpp>
#include <gr_relational_analysis/analysis.hpp>

namespace analysis_fixture {
using gr::Json;
namespace d = gr::data;
namespace rel = gr::data::relational;
inline Json document(const std::string &id, const std::string &type = "gr.entity@1") {
    return {{"format", "gr.record/1"},
            {"id", id},
            {"type", type},
            {"payload", {{"label", id}}},
            {"attribution", {{"agent", "fixture:observer"}, {"method", "reported"}}}};
}
inline Json participant(const std::string &id, const std::string &role, const std::string &scope = "record") {
    return {{"role", role}, {"reference", {{"scope", scope}, {"id", id}}}};
}
inline Json relation(const std::string &id, Json participants) {
    auto d = document(id, "gr.relation@1");
    d["participants"] = std::move(participants);
    d["qualifiers"] = {{"status", "contested"}, {"uncertainty", "not quantified"}};
    return d;
}
inline Json order(const std::string &id, const std::string &before, const std::string &after) {
    auto d = document(id, "gr.order@1");
    d["order"] = {{"before", before}, {"after", after}};
    return d;
}
struct Fixture {
    std::shared_ptr<gr::storage::MemoryStore> store = std::make_shared<gr::storage::MemoryStore>();
    std::shared_ptr<gr::core::TypeRegistry> registry = [] {
        auto r = std::make_shared<gr::core::TypeRegistry>();
        r->register_extension("fixture.opaque@1", [](const Json &) {});
        return r;
    }();
    gr::runtime::TransactionCoordinator coordinator{store, registry};
    std::shared_ptr<d::Library> library = std::make_shared<d::Library>();
    std::shared_ptr<rel::Session> session = rel::install(*library);
    gr::runtime::Runtime runtime{gr::runtime::RuntimeConfig{1}};
    std::string workspace;
    std::size_t transaction = 0;
    explicit Fixture(std::string name = "analysis:fixture") : workspace(std::move(name)) {
        store->create_workspace(workspace);
        gr_relational_analysis::install(*library, session);
        library->seal();
    }
    void publish(const Json &documents) {
        gr::runtime::Stage stage;
        for (const auto &d : documents)
            stage.records.push_back(gr::core::construct(d));
        gr::runtime::TransactionOptions options;
        options.workspace = workspace;
        options.transaction_id = "fixture:" + std::to_string(++transaction);
        options.actor = "fixture:recorder";
        options.reason = "Synthetic module conformance";
        options.track_history = true;
        coordinator.commit(options, stage);
    }
    d::DataValue selected(const std::string &semantic = "", const std::string &physical = "") {
        return rel::select_history(coordinator, {workspace, semantic, physical});
    }
    d::DataValue incidence(const d::DataValue &records, Json roles = nullptr, Json temporal = nullptr) {
        auto expr = d::apply(library, rel::project_operation, {d::literal(library, records)},
                             temporal.is_null() ? Json::object() : Json{{"temporal", temporal}});
        if (!roles.is_null())
            expr = d::apply(library, rel::roles_operation, {expr}, {{"roles", roles}});
        auto program = d::compile({expr});
        auto result = runtime.run(program.plan());
        if (result.status != gr::runtime::JobStatus::succeeded)
            throw gr::Error("fixture_execution", result.error.dump());
        return program.materialize(result).at(0);
    }
    void small() {
        auto r = relation(
            "r", Json::array({participant("a", "actor"), participant("a", "actor"),
                              participant("b", "witness"), participant("designation", "actor", "subject")}));
        r["participants"][0]["orientation"] = "out";
        r["participants"][0]["reference"]["basis"] = "opaque reference annotation";
        auto opaque = document("opaque", "example.unknown@1");
        opaque["participants"] = Json::array({participant("unresolved", "observer", "subject")});
        auto zero = document("zero");
        zero["participants"] = Json::array();
        publish(Json::array({document("a"), document("b"), r,
                             relation("parallel", Json::array({participant("a", "actor")})),
                             relation("higher", Json::array({participant("r", "observer")})), zero, opaque}));
    }
    void synthetic(std::size_t relations) {
        Json docs = Json::array();
        for (int i = 0; i < 16; ++i)
            docs.push_back(document("entity:" + std::to_string(i)));
        for (std::size_t i = 0; i < relations; ++i) {
            auto a = "entity:" + std::to_string(i % 16), b = "entity:" + std::to_string((i + 1) % 16);
            docs.push_back(relation(
                "relation:" + std::to_string(i),
                Json::array({participant(a, "actor"), participant(b, "witness"), participant(a, "actor")})));
        }
        publish(docs);
    }
};
} // namespace analysis_fixture
