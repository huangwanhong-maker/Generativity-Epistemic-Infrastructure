#include "gr/experimental/data.hpp"
#include "gr/storage/memory_store.hpp"
#include <iostream>

int main() {
    try {
        auto store = std::make_shared<gr::storage::MemoryStore>();
        store->create_workspace("study");
        gr::runtime::TransactionCoordinator coordinator(store);
        auto document = gr::Json{{"format", "gr.record/1"},
                                 {"id", "observations"},
                                 {"type", "gr.entity@1"},
                                 {"payload", {{"values", {1, 2, 3, 4}}}},
                                 {"attribution", {{"agent", "fixture"}, {"method", "synthetic"}}}};
        gr::runtime::TransactionOptions options;
        options.workspace = "study";
        options.transaction_id = "initial";
        options.actor = "test";
        options.reason = "Synthetic historical fixture";
        options.track_history = true;
        gr::runtime::Stage stage;
        stage.records.push_back(gr::core::construct(document));
        coordinator.commit(options, stage);
        const auto first = coordinator.history_snapshot("study");
        const auto revision = first.at("provenance").at("semantic_revision").get<std::string>();
        document["payload"]["values"] = {100};
        stage.records = gr::Json::array({gr::core::construct(document)});
        options.transaction_id = "second";
        coordinator.commit(options, stage);
        auto selected =
            gr::experimental::data::select(coordinator, "study", "observations", "/values", revision);
        if (selected.provenance.at("semantic_revision") != revision)
            return 1;
        auto catalog = std::make_shared<gr::runtime::Catalog>();
        gr::experimental::data::install(*catalog);
        catalog->seal();
        gr::runtime::Planner planner(catalog);
        const auto plan = planner.compile(gr::experimental::data::transform_sum(selected, 3, -1));
        gr::runtime::RuntimeConfig config;
        config.workers = 2;
        gr::runtime::Runtime runtime(config);
        for (bool serial : {true, false}) {
            gr::runtime::JobOptions job;
            job.serial = serial;
            const auto result = runtime.run(plan, job);
            if (result.status != gr::runtime::JobStatus::succeeded) {
                std::cerr << result.error << '\n';
                return 2;
            }
            if (gr::experimental::data::integers(result.outputs[0].as_buffers().at("values")) !=
                std::vector<std::int64_t>{2, 5, 8, 11})
                return 3;
            if (gr::experimental::data::integers(result.outputs[1].as_buffers().at("values")) !=
                std::vector<std::int64_t>{26})
                return 4;
        }
        auto bad = document;
        bad["payload"]["values"] = {"not an integer"};
        options.transaction_id = "invalid-values";
        stage.records = gr::Json::array({gr::core::construct(bad)});
        coordinator.commit(options, stage);
        bool rejected = false;
        try {
            (void)gr::experimental::data::select(coordinator, "study", "observations", "/values");
        } catch (const gr::Error &e) {
            rejected = e.code == "invalid_value";
        }
        if (!rejected)
            return 5;
        std::cout << "Experimental historical selection, qualified provenance and serial/threaded checked "
                     "transform/sum passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 6;
    }
}
