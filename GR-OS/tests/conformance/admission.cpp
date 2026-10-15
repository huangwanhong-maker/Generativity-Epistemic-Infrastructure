#include "gr/runtime.hpp"
#include "gr/runtime/transaction.hpp"
#include "storage_support.hpp"
#include <future>

using namespace gr::runtime;
namespace {
Json record(const std::string &id, int value,
            const gr::core::TypeRegistry &registry = gr::core::default_types(),
            const std::string &type = "gr.assertion@1") {
    return gr::core::construct({{"format", "gr.record/1"},
                                {"id", id},
                                {"type", type},
                                {"payload", {{"value", value}}},
                                {"attribution", {{"agent", "test"}, {"method", "synthetic"}}}},
                               registry);
}
TransactionOptions options(const std::string &id, bool serial = false) {
    TransactionOptions result;
    result.workspace = "test";
    result.transaction_id = id;
    result.actor = "test";
    result.reason = "Admission conformance";
    if (serial)
        result.admission = AdmissionPolicy::serialized;
    return result;
}
void validation_reuse() {
    gr::core::TypeRegistry registry;
    int calls = 0;
    registry.register_type("test.counted@1", [&](const Json &) { ++calls; });
    Json records = Json::array();
    for (int i = 0; i < 32; ++i)
        records.push_back(record(std::to_string(i), 0, registry, "test.counted@1"));
    auto update = record("0", 1, registry, "test.counted@1");
    calls = 0;
    gr::core::ValidationSession session(registry);
    Json base = {{"revision", "0"}, {"records", records}};
    check(session.validate_snapshot(base, true).at("valid") && calls == 32,
          "initial verification once per wrapper");
    auto candidate = session.apply(base, Json::array({update}));
    check(calls == 33 && candidate.at("archive").size() == 1,
          "candidate retains complete checks and reuses immutable verification");
    auto forged = update;
    forged["document"]["payload"]["value"] = 8;
    check(!session.verify(forged).at("valid").get<bool>(),
          "memo cannot bless changed bytes under the same version");
    auto floating = update;
    floating["document"]["payload"]["value"] = 1.0;
    check(floating == update, "regression fixture exercises JSON numeric equality across representations");
    check(!session.verify(floating).at("valid").get<bool>(),
          "memo rejects numerically equal forbidden floating metadata");
    check(!session.validate_snapshot({{"revision", "0"}, {"records", Json::array({floating})}}, true)
               .at("valid")
               .get<bool>(),
          "cached byte count cannot admit a floating wrapper");
    auto parsed = Json::parse(update.dump());
    check(session.verify(parsed).at("valid").get<bool>(),
          "signed/unsigned parse normalization preserves valid memo reuse");
    auto dependent = record("dependent", 1).at("document");
    dependent["references"] = Json::array({{{"id", "absent"}, {"scope", "record"}}});
    error("admission", [&] { session.apply(candidate, Json::array({gr::core::construct(dependent)})); });
    auto required = record("opaque", 1).at("document");
    required["type"] = "test.later@1";
    required["required"] = true;
    auto opaque = gr::core::construct(required);
    registry.register_type("test.later@1", [](const Json &) {});
    check(!session.validate_snapshot({{"revision", "0"}, {"records", Json::array({opaque})}}, true)
               .at("valid")
               .get<bool>(),
          "session owns its registry snapshot and cannot acquire later interpretation");

    auto store = std::make_shared<MemoryStore>();
    store->create_workspace("test");
    auto shared_registry = std::make_shared<gr::core::TypeRegistry>(registry);
    TransactionCoordinator coordinator(store, shared_registry);
    coordinator.commit(options("seed"), {Json::array({records[0]}), {}, {}});
    calls = 0;
    auto tx = coordinator.begin(options("reuse"));
    tx->stage({Json::array({update}), {}, {}});
    tx->snapshot();
    tx->snapshot();
    auto result = tx->commit();
    check(calls == 2 && result.diagnostics.at("validation").at("verified_wrappers") == 2,
          "begin/stage/preview/commit verify each source and update exactly once");
}
void serialized(const std::shared_ptr<IStore> &a, const std::shared_ptr<IStore> &b) {
    a->create_workspace("test");
    TransactionCoordinator first(a), second(b);
    first.commit(options("seed"), {Json::array({record("counter", 0)}), {}, {}});
    auto holder = first.begin(options("holder", true));
    check(second.snapshot("test").at("records")[0].at("document").at("payload").at("value") == 0,
          "logical writer admission does not block snapshot readers");
    auto timeout = std::async(std::launch::async, [&] {
        error("transaction_deadline", [&] {
            second.begin(options("deadline", true),
                         {{}, std::chrono::steady_clock::now() + std::chrono::milliseconds(25)});
        });
    });
    timeout.get();
    std::stop_source cancellation;
    auto cancelled = std::async(std::launch::async, [&] {
        error("transaction_cancelled",
              [&] { second.begin(options("cancelled", true), {cancellation.get_token(), {}}); });
    });
    cancellation.request_stop();
    cancelled.get();
    holder->stage({Json::array({record("counter", 1)}), {}, {}});
    // Begin and release occur on different threads; a std::mutex ownership lease would be invalid.
    std::async(std::launch::async, [&] { holder->commit(); }).get();
    auto rollback = second.begin(options("rollback", true));
    rollback->rollback();
    auto abandoned = first.begin(options("abandoned", true));
    abandoned.reset();
    auto invalid = options("explicit-base", true);
    invalid.expected_revision = a->head("test");
    error("invalid_argument", [&] { first.begin(invalid); });

    std::barrier start(5);
    std::atomic<int> failures = 0;
    std::vector<std::thread> threads;
    for (int writer = 0; writer < 4; ++writer)
        threads.emplace_back([&, writer] {
            start.arrive_and_wait();
            try {
                TransactionCoordinator coordinator(writer % 2 ? a : b);
                for (int step = 0; step < 6; ++step) {
                    auto tx = coordinator.begin(
                        options("writer:" + std::to_string(writer) + ":" + std::to_string(step), true),
                        {{}, std::chrono::steady_clock::now() + std::chrono::seconds(20)});
                    const auto current =
                        tx->snapshot().at("records")[0].at("document").at("payload").at("value").get<int>();
                    tx->stage({Json::array({record("counter", current + 1)}), {}, {}});
                    tx->commit();
                }
            } catch (...) {
                ++failures;
            }
        });
    start.arrive_and_wait();
    for (auto &thread : threads)
        thread.join();
    check(failures == 0 &&
              first.snapshot("test").at("records")[0].at("document").at("payload").at("value") == 25,
          "cooperating concurrent read-modify-write transactions serialize without lost updates or retries");
    check(a->history("test").entries.size() == 26, "only completed admitted writers publish");
}
void stale_and_replay() {
    auto store = std::make_shared<MemoryStore>();
    store->create_workspace("test");
    TransactionCoordinator coordinator(store);
    auto seed_options = options("seed");
    seed_options.expected_revision = "0";
    const auto seed = record("a", 1);
    const auto first = coordinator.commit(seed_options, {Json::array({seed}), {}, {}});
    auto stale = coordinator.begin(options("stale"));
    coordinator.commit(options("newer"), {Json::array({record("a", 2)}), {}, {}});
    stale->stage({Json::array({record("b", 1)}), {}, {}});
    error("conflict", [&] { stale->commit(); });
    check(stale->state() == TransactionState::conflicted && stale->result().request_digest.empty(),
          "preflight stale rejection avoids semantic preparation and has no uncertain outcome");
    auto replay = coordinator.commit(seed_options, {Json::array({seed}), {}, {}});
    check(replay.publication->replayed && replay.publication->revision == first.publication->revision,
          "stale preflight preserves legitimate exact replay from old base");
    auto admitted = coordinator.begin(options("cooperating", true));
    coordinator.commit(options("outside-policy"), {Json::array({record("a", 3)}), {}, {}});
    error("conflict", [&] { admitted->commit(); });
    auto after = coordinator.begin(options("after-conflict", true));
    after->rollback();
    check(store->history("test").entries.size() == 3, "noncooperating writer still enforced by physical CAS");
}
void scheduler_admission() {
    auto store = std::make_shared<MemoryStore>();
    store->create_workspace("test");
    auto coordinator = std::make_shared<TransactionCoordinator>(store);
    auto holder = coordinator->begin(options("held", true));
    auto catalog = std::make_shared<Catalog>();
    catalog->seal();
    Graph graph;
    graph.nodes.push_back({"identity", "gr.bytes.identity@1", {gr::kernel::Value::bytes("gr.bytes@1", {1})}});
    graph.outputs.push_back({"identity", 0});
    auto plan = Planner(catalog).compile(graph);
    Publication publication;
    publication.coordinator = coordinator;
    publication.options = options("job", true);
    publication.options.max_staged_bytes = 1024;
    RuntimeConfig config;
    config.workers = 1;
    config.policy.writable_workspaces.insert("test");
    Runtime runtime(config);
    JobOptions job_options;
    job_options.max_retained_bytes = 1024;
    job_options.timeout = std::chrono::milliseconds(25);
    error("transaction_deadline", [&] { runtime.submit(plan, job_options, publication); });
    check(runtime.diagnostics().at("live_jobs") == 0, "failed queued admission releases runtime quota");
    job_options.timeout.reset();
    auto pending = std::async(std::launch::async, [&] {
        error("transaction_cancelled", [&] { runtime.submit(plan, job_options, publication); });
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (runtime.diagnostics().at("live_jobs") == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const bool queued = runtime.diagnostics().at("live_jobs") == 1;
    runtime.shutdown();
    pending.get();
    check(queued && runtime.diagnostics().at("live_jobs") == 0,
          "shutdown cancels serialized admission before a job handle is returned");
    holder->rollback();
}
} // namespace
int main() {
    const auto folder = temporary_folder("admission");
    try {
        validation_reuse();
        stale_and_replay();
        scheduler_admission();
        auto memory = std::make_shared<MemoryStore>();
        serialized(memory, memory);
        {
            auto a = std::make_shared<SQLiteStore>((folder / "writers.sqlite").string());
            auto b = std::make_shared<SQLiteStore>((folder / "writers.sqlite").string());
            serialized(a, b);
        }
        cleanup(folder);
        std::cout << "Validation reuse, immutable memo guards, serialized admission, cancellation, readers, "
                     "cross-thread leases and exact replay passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << folder << '\n';
        return 1;
    }
}
