#include <filesystem>
#include <fstream>
#include <gr/data/standard.hpp>
#include <gr/storage/memory_store.hpp>
#include <gr_diagonal/diagonal.hpp>
#include <gr_relational_analysis/analysis.hpp>
#include <gr_visualization/visualization.hpp>
#include <iostream>

namespace data = gr::data;
namespace rel = gr::data::relational;
namespace analysis = gr_relational_analysis;
namespace visual = gr_visualization;
using gr::Json;
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
Json document(const char *id, const char *type = "gr.entity@1") {
    return {{"format", "gr.record/1"},
            {"id", id},
            {"type", type},
            {"payload", Json::object()},
            {"attribution", {{"agent", "example:observer"}, {"method", "synthetic"}}}};
}
Json participant(const char *id, const char *role) {
    return {{"role", role}, {"reference", {{"scope", "record"}, {"id", id}}}};
}
void write(const std::filesystem::path &path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output)
        throw std::runtime_error("Cannot write consumer artifact");
}
} // namespace

int main(int argc, char **argv) {
    try {
        auto library = std::make_shared<data::Library>();
        data::install_standard(*library);
        auto session = rel::install(*library);
        analysis::install(*library, session);
        gr_diagonal::install(*library);
        library->seal();

        auto store = std::make_shared<gr::storage::MemoryStore>();
        store->create_workspace("module-demo");
        gr::runtime::TransactionCoordinator coordinator(store);
        auto first = document("claim:one", "gr.relation@1");
        first["participants"] =
            Json::array({participant("person:a", "actor"), participant("person:a", "actor"),
                         participant("person:b", "witness")});
        first["qualifiers"] = {{"status", "contested"}, {"uncertainty", "unquantified"}};
        auto second = document("claim:two", "gr.relation@1");
        second["participants"] =
            Json::array({participant("person:a", "actor"), participant("person:c", "witness")});
        second["qualifiers"] = {{"status", "attributed report"}};
        gr::runtime::Stage stage;
        for (const auto &record :
             Json::array({document("person:a"), document("person:b"), document("person:c"), first, second}))
            stage.records.push_back(gr::core::construct(record));
        gr::runtime::TransactionOptions options;
        options.workspace = "module-demo";
        options.transaction_id = "seed";
        options.actor = "example:recorder";
        options.reason = "Independent installed-module composition fixture";
        options.track_history = true;
        coordinator.commit(options, stage);
        const auto head = store->head("module-demo");

        const auto historical = rel::select_history(coordinator, {"module-demo", "", ""});
        auto incidence_expr =
            data::apply(library, rel::project_operation, {data::literal(library, historical)});
        auto report_expr = data::apply(library, analysis::summary_operation, {incidence_expr});
        data::Function pipeline({incidence_expr, report_expr});
        auto program = pipeline.bind();
        gr::runtime::Runtime runtime({.workers = 2});
        auto job = runtime.submit(program.plan());
        auto values = program.materialize(job.wait());
        const auto scheduled = analysis::inspect_summary(values.at(1));
        check(scheduled.at("occurrences") == 5, "scheduled summary occurrence count");
        check(!values.at(1).provenance.empty(), "materialization lineage absent");

        auto network = analysis::analyze(values.at(0), session);
        const auto snapshot = network.snapshot();
        const auto summary = analysis::summarize(network);
        check(summary.at("owners") == 2 && summary.at("targets") == 3 && summary.at("occurrences") == 5 &&
                  summary.at("components").size() == 1,
              "independent bipartite count/component oracle");
        check(analysis::compare(network, network).at("records").at("changed").empty(),
              "self-comparison changed records");

        // Explicit modeling boundary: one integer per target vertex, ordered by
        // the snapshot's stable vertex key. This is an illustrative weighting
        // of recorded occurrences, not a measure of real social importance.
        std::vector<std::int64_t> degrees, weights;
        Json mapping = Json::array();
        for (std::size_t i = 0; i < snapshot.vertices().size(); ++i) {
            const auto &vertex = snapshot.vertices()[i];
            if (vertex.kind != analysis::VertexKind::participant_target)
                continue;
            std::int64_t degree = 0;
            for (const auto &occurrence : snapshot.occurrences())
                degree += occurrence.target == i;
            mapping.push_back({{"index", degrees.size()},
                               {"vertex", vertex.key},
                               {"scope", vertex.scope},
                               {"id", vertex.id},
                               {"version", vertex.version ? Json(*vertex.version) : Json(nullptr)},
                               {"occurrence_degree", degree},
                               {"weight", 2}});
            degrees.push_back(degree);
            weights.push_back(2);
        }
        auto vector = data::input(library, "vector", data::column_i64(degrees).spec);
        auto diagonal = data::input(library, "diagonal", gr_diagonal::spec());
        data::Function weighting({data::apply(library, "example.diagonal.apply@1", {diagonal, vector})});
        auto weighted =
            weighting.instantiate({{"vector", data::literal(library, data::column_i64(degrees))},
                                   {"diagonal", data::literal(library, gr_diagonal::make(weights))}});
        auto reduction = data::compile(
            {data::apply(library, "gr.data.sum@1", weighted, {{"nulls", "skip"}, {"min_count", 1}})});
        auto result = reduction.materialize(runtime.run(reduction.plan()));
        check(data::get_optional_i64(result.at(0)) == 10, "explicit degree weighting oracle");

        auto scene = visual::project(snapshot);
        auto svg = visual::svg(scene);
        check(svg.find("<svg") != std::string::npos, "SVG export missing root");
        const auto portable = snapshot.encode();
        auto imported = analysis::NetworkSnapshot::decode(
            {reinterpret_cast<const std::uint8_t *>(portable.data()), portable.size()});
        check(imported.id() == snapshot.id() && imported.validation_basis() == "structural_only",
              "portable import asserted omitted source validation");
        auto imported_scene = visual::project(imported);
        check(visual::project(snapshot).encode() == scene.encode(), "scene is not deterministic");
        check(store->head("module-demo") == head, "pure modules mutated history");

        // Clear the cache and source owners; a copied snapshot and source lease
        // remain independently valid. No cross-engine native handles are used.
        auto lease = network.source();
        session->clear_cache();
        network = {};
        check(lease.source().at("records").size() == 5 && snapshot.occurrences().size() == 5,
              "inspection/snapshot ownership lost");

        if (argc == 2) {
            const std::filesystem::path directory(argv[1]);
            std::filesystem::create_directories(directory);
            write(directory / "snapshot.json", portable);
            write(directory / "summary.json", summary.dump(2));
            write(directory / "scheduled-summary.json", scheduled.dump(2));
            write(directory / "scene.json", scene.encode());
            write(directory / "scene.svg", svg);
            write(directory / "imported-scene.json", imported_scene.encode());
            write(directory / "imported-scene.svg", visual::svg(imported_scene));
            write(directory / "degree-model.json",
                  Json{{"source_value_id", snapshot.identity().at("source_value_id")},
                       {"snapshot_id", snapshot.id()},
                       {"mapping", mapping},
                       {"weighted_sum", 10},
                       {"assumptions",
                        "Uniform weight 2 on recorded target occurrence degree; illustrative only"}}
                      .dump(2));
            write(
                directory / "incidence-meta.json",
                Json{{"spec", values.at(0).spec.to_json()}, {"provenance", values.at(0).provenance}}.dump());
            const auto &bytes = values.at(0).payload.as_bytes();
            write(directory / "incidence.bin", {reinterpret_cast<const char *>(bytes.data()), bytes.size()});
        }
        std::cout << "Installed modules: 2 owners, 3 targets, 5 occurrences, 1 component; "
                     "weighted degree sum 10; native/portable scenes and source custody passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
