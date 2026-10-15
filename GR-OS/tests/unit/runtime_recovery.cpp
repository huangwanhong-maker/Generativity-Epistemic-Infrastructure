#include "gr/runtime.hpp"
#include "gr/storage/memory_store.hpp"
#include <atomic>
#include <iostream>

using namespace gr;
using namespace gr::runtime;
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
// Faults surround the real adapter commit. They model acknowledgment uncertainty,
// while the separate SQLite subprocess suite tests actual crash recovery.
class UncertainStore final : public storage::IStore {
  public:
    enum class Failure { none, before, after };
    std::shared_ptr<storage::IStore> source = std::make_shared<storage::MemoryStore>();
    std::atomic<Failure> failure{Failure::after};
    storage::StorageCapabilities describe() const override { return source->describe(); }
    void create_workspace(const std::string &workspace) override { source->create_workspace(workspace); }
    std::string head(const std::string &workspace) override { return source->head(workspace); }
    storage::Snapshot snapshot(const std::string &workspace, const std::string &revision = "") override {
        return source->snapshot(workspace, revision);
    }
    storage::StoredRecord record_version(const std::string &workspace, const std::string &id,
                                         const std::string &version) override {
        return source->record_version(workspace, id, version);
    }
    storage::Result commit(const storage::Commit &request) override {
        if (failure == Failure::before)
            throw Error("backend_unavailable", "Before acceptance");
        auto result = source->commit(request);
        if (failure == Failure::after)
            throw Error("acknowledgment_lost", "After acceptance");
        return result;
    }
    storage::HistoryPage history(const std::string &workspace, const std::string &before = "",
                                 std::size_t limit = 100) override {
        return source->history(workspace, before, limit);
    }
    std::optional<storage::TransactionReceipt> receipt(const std::string &workspace,
                                                       const std::string &id) override {
        return source->receipt(workspace, id);
    }
    storage::BlobMetadata blob_info(const std::string &workspace, const std::string &digest) override {
        return source->blob_info(workspace, digest);
    }
    std::unique_ptr<storage::BlobReader> open_blob(const std::string &workspace, const std::string &digest,
                                                   storage::BlobReadOptions options = {}) override {
        return source->open_blob(workspace, digest, options);
    }
};
std::shared_ptr<const PreparedPlan> counted_plan(std::atomic<int> &calls) {
    auto catalog = std::make_shared<Catalog>();
    kernel::OperationDescriptor operation;
    operation.id = "test.recovery.identity@1";
    operation.inputs = operation.outputs = {"gr.bytes@1"};
    operation.meaning = "Return exact input; external counter observes dispatch only";
    catalog->pin_types(operation);
    catalog->register_operation(operation);
    catalog->register_implementation({"test.recovery.provider@1",
                                      operation.id,
                                      operation.fingerprint(),
                                      {"cpu.sequential"},
                                      {"cpu"},
                                      {"host"},
                                      kernel::Determinism::Exact,
                                      true,
                                      Json::object()},
                                     [&](const kernel::Invocation &input, std::stop_token) {
                                         ++calls;
                                         return input;
                                     });
    catalog->seal();
    Graph graph;
    graph.nodes.push_back({"artifact", operation.id, {kernel::Value::bytes("gr.bytes@1", {1, 2, 3})}});
    graph.outputs.push_back({"artifact", 0});
    return Planner(catalog).compile(graph);
}
void exercise(UncertainStore::Failure failure, bool retry) {
    auto store = std::make_shared<UncertainStore>();
    store->failure = failure;
    store->create_workspace("test");
    auto coordinator = std::make_shared<TransactionCoordinator>(store);
    Publication publication;
    publication.coordinator = coordinator;
    publication.options.workspace = "test";
    publication.options.expected_revision = "0";
    publication.options.transaction_id = "uncertain-job";
    publication.options.actor = "recorder:test";
    publication.options.reason = "Unknown synchronous publication recovery";
    publication.options.track_history = true;
    publication.options.max_staged_bytes = 1024 * 1024;
    publication.artifacts = {{{"artifact", 0},
                              {{"format", "gr.record/1"},
                               {"id", "result"},
                               {"type", "gr.assertion@1"},
                               {"payload", Json::object()},
                               {"attribution", {{"agent", "test"}, {"method", "synthetic"}}}},
                              "application/octet-stream"}};
    std::atomic<int> calls = 0;
    auto plan = counted_plan(calls);
    JobResult unknown;
    {
        RuntimeConfig config;
        config.workers = 1;
        config.policy.writable_workspaces.insert("test");
        Runtime runtime(config);
        JobOptions options;
        options.max_retained_bytes = 1024;
        unknown = runtime.run(plan, options, publication);
        check(unknown.status == JobStatus::outcome_unknown && unknown.outputs.empty(),
              "Uncertain publication exposes no unconfirmed successful job output");
        check(bool(unknown.recovery) && calls == 1,
              "Synchronous result retains an owning recovery handle without rerunning computation");
    }
    auto recovery = unknown.recovery;
    const auto frozen_digest = unknown.publication.at("request_digest").get<std::string>();
    unknown = {}; // Retain only the explicit control lease, after runtime destruction.
    if (failure == UncertainStore::Failure::before) {
        auto absent = recovery->recover_publication();
        check(absent.at("state") == "not_observed" && recovery->wait().status == JobStatus::outcome_unknown,
              "Receipt absence remains uncertain and preserves the frozen job");
    }
    store->failure = UncertainStore::Failure::none;
    const auto recovered = recovery->recover_publication(retry);
    check(recovered.at("state") == "committed" && recovered.at("request_digest") == frozen_digest,
          "Recovery accepts only the original frozen transaction request identity");
    auto confirmed = recovery->wait();
    check(confirmed.status == JobStatus::succeeded && confirmed.outputs.size() == 1 &&
              confirmed.outputs[0].as_bytes() == kernel::Bytes({1, 2, 3}) && calls == 1,
          "Recovery restores original computed outputs without executing provider again");
    check(!confirmed.recovery, "Successful result needs no uncertain recovery handle");
    const auto receipt = store->receipt("test", "uncertain-job");
    check(receipt && receipt->entry.request_digest == frozen_digest &&
              store->history("test").entries.size() == 1,
          "Recovery produces one accepted receipt with exact original digest");
    check(coordinator->snapshot("test").at("records").size() == 1,
          "Recovered physical record and semantic history remain coherent");
    recovery->release();
    recovery.reset();
    confirmed = {};
    check(store->receipt("test", "uncertain-job").has_value(),
          "Releasing recovery/result handles does not remove the accepted receipt");
}
} // namespace
int main() {
    try {
        exercise(UncertainStore::Failure::after, false);
        exercise(UncertainStore::Failure::after, true);
        exercise(UncertainStore::Failure::before, true);
        std::cout
            << "Runtime synchronous uncertain-outcome leases, receipt recovery and exact retry passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Runtime recovery failure: " << error.what() << '\n';
        return 1;
    }
}
