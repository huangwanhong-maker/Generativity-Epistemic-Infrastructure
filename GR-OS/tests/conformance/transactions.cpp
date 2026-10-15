#include "gr/kernel/history.hpp"
#include "gr/runtime/transaction.hpp"
#include "storage_support.hpp"
using namespace gr::runtime;
namespace {
Json record(const std::string &id, int value) {
    return gr::core::construct({{"format", "gr.record/1"},
                                {"id", id},
                                {"type", "gr.assertion@1"},
                                {"payload", {{"value", value}}},
                                {"attribution", {{"agent", "test"}, {"method", "synthetic"}}}});
}
TransactionOptions options(const std::string &id, const std::string &base = "", bool history = false) {
    TransactionOptions out;
    out.workspace = "test";
    out.transaction_id = id;
    out.actor = "recorder:test";
    out.reason = "Runtime conformance";
    out.expected_revision = base;
    out.track_history = history;
    return out;
}
class FaultStore final : public IStore {
  public:
    enum class Fault { none, before, after, late_cancel };
    std::shared_ptr<IStore> source;
    Fault fault = Fault::none;
    bool fail_blob_reads = false;
    std::stop_source *stop = nullptr;
    explicit FaultStore(std::shared_ptr<IStore> value) : source(std::move(value)) {}
    StorageCapabilities describe() const override { return source->describe(); }
    void create_workspace(const std::string &w) override { source->create_workspace(w); }
    std::string head(const std::string &w) override { return source->head(w); }
    Snapshot snapshot(const std::string &w, const std::string &r = "") override {
        return source->snapshot(w, r);
    }
    StoredRecord record_version(const std::string &w, const std::string &i, const std::string &v) override {
        return source->record_version(w, i, v);
    }
    Result commit(const Commit &c) override {
        if (fault == Fault::before)
            throw gr::Error("storage_error", "Injected before delegate");
        auto result = source->commit(c);
        if (fault == Fault::after)
            throw gr::Error("storage_error", "Injected lost acknowledgment");
        if (fault == Fault::late_cancel && stop)
            stop->request_stop();
        return result;
    }
    HistoryPage history(const std::string &w, const std::string &b = "", std::size_t n = 100) override {
        return source->history(w, b, n);
    }
    std::optional<TransactionReceipt> receipt(const std::string &w, const std::string &id) override {
        return source->receipt(w, id);
    }
    BlobMetadata blob_info(const std::string &w, const std::string &d) override {
        return source->blob_info(w, d);
    }
    std::unique_ptr<BlobReader> open_blob(const std::string &w, const std::string &d,
                                          BlobReadOptions o = {}) override {
        if (fail_blob_reads)
            throw gr::Error("storage_unavailable", "Injected artifact reader failure");
        return source->open_blob(w, d, o);
    }
};
void conformance(const std::shared_ptr<IStore> &store) {
    store->create_workspace("test");
    TransactionCoordinator coordinator(store);
    auto transaction = coordinator.begin(options("first"));
    check(transaction->options().expected_revision == "0", "begin pins head before caller computation");
    auto relation = gr::core::construct(
        {{"format", "gr.record/1"},
         {"id", "relation"},
         {"type", "gr.relation@1"},
         {"payload", Json::object()},
         {"attribution", {{"agent", "test"}, {"method", "synthetic"}}},
         {"participants",
          Json::array({{{"role", "subject"}, {"reference", {{"id", "subject"}, {"scope", "record"}}}}})}});
    transaction->stage({Json::array({relation}), {}, {}});
    error("admission", [&] { (void)transaction->snapshot(); });
    transaction->stage({Json::array({record("subject", 1)}), {}, {}});
    check(transaction->snapshot()["records"].size() == 2 && store->head("test") == "0",
          "stage resolves forward reference without publication");
    error("duplicate_update", [&] { transaction->stage({Json::array({record("subject", 2)}), {}, {}}); });
    const auto first = transaction->commit();
    check(first.state == TransactionState::committed && first.publication &&
              coordinator.receipt("test", "first"),
          "commit and durable receipt");
    error("transaction_state", [&] { transaction->rollback(); });
    check(transaction->retry().publication->replayed, "same frozen request replays");
    auto rollback = coordinator.begin(options("rollback"));
    rollback->stage({Json::array({record("subject", 9)}), {}, {}});
    rollback->rollback();
    rollback->rollback();
    check(rollback->state() == TransactionState::rolled_back && !coordinator.receipt("test", "rollback"),
          "rollback has no physical receipt");
    auto cancelled = coordinator.begin(options("cancelled"));
    std::stop_source stop;
    stop.request_stop();
    error("transaction_cancelled", [&] { cancelled->commit({stop.get_token(), {}}); });
    check(cancelled->state() == TransactionState::failed && !coordinator.receipt("test", "cancelled"),
          "prepublication cancellation definitely has no effect");
    cancelled->rollback();
    auto expired = coordinator.begin(options("expired"));
    error("transaction_deadline", [&] { expired->commit({{}, std::chrono::steady_clock::now()}); });
    auto readonly_options = options("readonly");
    readonly_options.read_only = true;
    auto readonly = coordinator.begin(readonly_options);
    auto pinned = readonly->snapshot();
    auto winner = coordinator.begin(options("winner")), loser = coordinator.begin(options("loser"));
    winner->stage({Json::array({record("subject", 2)}), {}, {}});
    winner->commit();
    loser->stage({Json::array({record("unrelated", 3)}), {}, {}});
    error("conflict", [&] { loser->commit(); });
    check(loser->state() == TransactionState::conflicted && !coordinator.receipt("test", "loser"),
          "whole-workspace CAS rejects even unrelated write");
    check(readonly->snapshot() == pinned && !readonly->commit().publication,
          "read-only transaction pins its original view and publishes nothing");
    error("read_only",
          [&] { coordinator.begin(readonly_options)->stage({Json::array({record("a", 1)}), {}, {}}); });
    auto limited_options = options("limited");
    limited_options.max_staged_bytes = 1;
    auto limited = coordinator.begin(limited_options);
    error("transaction_limit", [&] { limited->stage({Json::array({record("too-large", 1)}), {}, {}}); });
    auto unsupported = options("unsupported");
    unsupported.isolation = static_cast<Isolation>(99);
    error("unsupported_isolation", [&] { coordinator.begin(unsupported); });
    auto wrong_capability = options("dag");
    wrong_capability.requirements.history_model = HistoryModel::dag;
    error("unsupported_storage_capability", [&] { coordinator.begin(wrong_capability); });
}
void history_contract(const std::shared_ptr<IStore> &store) {
    store->create_workspace("test");
    TransactionCoordinator coordinator(store);
    auto first = coordinator.commit(options("history-1", "", true), {Json::array({record("a", 1)}), {}, {}});
    const auto physical = store->snapshot("test");
    check(physical.records.size() == 2 && coordinator.snapshot("test")["records"].size() == 1,
          "technical manifest is separate from user candidate");
    auto second = coordinator.commit(options("history-2"), {Json::array({record("a", 2)}), {}, {}});
    check(!second.history.empty(), "retained history automatically advances on later ordinary commit");
    auto bundle = coordinator.history_export_v1("test");
    check(bundle["nodes"].size() == 2, "two semantic nodes retained atomically");
    auto latest = gr::history::reconstruct(bundle, second.history.at("head"));
    check(latest["records"][0]["document"]["payload"]["value"] == 2,
          "semantic checkpoint matches physical current records");
    check(gr::history::reconstruct(
              bundle, first.history.at("head"))["records"][0]["document"]["payload"]["value"] == 1,
          "historical value retained");
    auto fork_state = gr::core::apply(gr::history::reconstruct(bundle, first.history.at("head")),
                                      Json::array({record("a", 7)}));
    auto forked =
        gr::history::append(bundle, fork_state, Json::array({first.history.at("head")}),
                            {{"agent", "test"}, {"method", "explicit branch"}, {"reason", "alternative"}});
    const auto fork = forked["nodes"].back()["version"].get<std::string>();
    auto publication = coordinator.begin(options("fork"));
    publication->stage({Json::array({record("a", 7)}), {}, {}});
    publication->stage_history({forked, fork});
    const auto accepted = publication->commit();
    check(accepted.history["head"] == fork && store->history("test").entries.size() == 3,
          "semantic branch selection remains physical linear publication");
    auto mismatch = coordinator.begin(options("wrong-head"));
    mismatch->stage({Json::array({record("a", 10)}), {}, {}});
    mismatch->stage_history({forked, fork});
    error("history_state", [&] { mismatch->commit(); });
    check(!coordinator.receipt("test", "wrong-head"), "mismatched history never publishes");
    auto reserved = coordinator.begin(options("reserved"));
    error("reserved_record",
          [&] { reserved->stage({Json::array({record(history_manifest_id, 1)}), {}, {}}); });
}
void uncertainty() {
    auto store = std::make_shared<MemoryStore>();
    auto fault = std::make_shared<FaultStore>(store);
    store->create_workspace("test");
    TransactionCoordinator coordinator(fault);
    auto transaction = coordinator.begin(options("uncertain"));
    transaction->stage({Json::array({record("a", 1)}), {}, {}});
    fault->fault = FaultStore::Fault::before;
    error("transaction_outcome_unknown", [&] { transaction->commit(); });
    check(transaction->recover().state == TransactionState::not_observed,
          "absent receipt is not rollback proof");
    error("transaction_state", [&] { transaction->rollback(); });
    fault->fault = FaultStore::Fault::after;
    error("transaction_outcome_unknown", [&] { transaction->retry(); });
    check(transaction->recover().state == TransactionState::committed,
          "lost acknowledgment recovered through exact receipt digest");
    fault->fault = FaultStore::Fault::none;
    check(transaction->retry().publication->replayed && store->history("test").entries.size() == 1,
          "uncertain retry publishes only once");
    fault->fault = FaultStore::Fault::before;
    error("storage_error", [&] { transaction->retry(); });
    check(transaction->state() == TransactionState::committed,
          "Later backend unavailability cannot erase acknowledged commit knowledge");
    std::stop_source stop;
    fault->fault = FaultStore::Fault::late_cancel;
    fault->stop = &stop;
    auto late = coordinator.begin(options("late-cancel"));
    late->stage({Json::array({record("a", 2)}), {}, {}});
    check(late->commit({stop.get_token(), {}}).state == TransactionState::committed && stop.stop_requested(),
          "successful publication wins late cancellation");
}
void legacy_identity() {
    auto direct = std::make_shared<MemoryStore>(), adapted = std::make_shared<MemoryStore>();
    direct->create_workspace("test");
    adapted->create_workspace("test");
    auto r = record("a", 1);
    auto c = request("compat");
    c.reason = "Runtime conformance";
    c.records = {{"a", r["version"], r["document"]}};
    const auto expected = direct->commit(c);
    TransactionCoordinator coordinator(adapted);
    const auto actual = coordinator.commit(options("compat", "0"), {Json::array({r}), {}, {}});
    check(actual.publication->revision == expected.revision,
          "untracked legacy request identity is unchanged");
}
void semantic_guards() {
    auto store = std::make_shared<MemoryStore>();
    store->create_workspace("test");
    auto opaque = record("opaque", 1).at("document");
    opaque["type"] = "example.unknown@1";
    opaque["required"] = true;
    auto commit = request("opaque");
    commit.records = {{"opaque", "", opaque}};
    store->commit(commit);
    TransactionCoordinator coordinator(store);
    auto view = options("read-unknown");
    view.read_only = true;
    check(coordinator.begin(view)->snapshot()["records"][0]["document"] == opaque,
          "Unknown mandatory records remain readable and preserved");
    error("admission", [&] { coordinator.begin(options("write-unknown")); });
    auto malicious = std::make_shared<MemoryStore>();
    malicious->create_workspace("test");
    auto fake = record(history_manifest_id, 1);
    auto raw = request("fake");
    raw.records = {{history_manifest_id, fake["version"], fake["document"]}};
    malicious->commit(raw);
    error("history_manifest", [&] { TransactionCoordinator(malicious).snapshot("test"); });

    auto frozen_store = std::make_shared<MemoryStore>();
    frozen_store->create_workspace("test");
    auto fault = std::make_shared<FaultStore>(frozen_store);
    auto registry = std::make_shared<gr::core::TypeRegistry>();
    auto valid_profile = std::make_shared<bool>(true);
    registry->register_type("example.switch@1", [valid_profile](const Json &) {
        if (!*valid_profile)
            throw gr::Error("profile_changed", "Test validator is no longer available");
    });
    auto document = record("custom", 1).at("document");
    document["type"] = "example.switch@1";
    auto item = gr::core::construct(document, *registry);
    TransactionCoordinator frozen_coordinator(fault, registry);
    auto tx = frozen_coordinator.begin(options("frozen"));
    tx->stage({Json::array({item}), {}, {}});
    fault->fault = FaultStore::Fault::after;
    error("transaction_outcome_unknown", [&] { tx->commit(); });
    *valid_profile = false;
    fault->fault = FaultStore::Fault::none;
    check(tx->retry().publication->replayed, "Frozen retry does not rerun changed semantic interpretation");

    auto history_store = std::make_shared<MemoryStore>();
    history_store->create_workspace("test");
    auto history_fault = std::make_shared<FaultStore>(history_store);
    TransactionCoordinator history_coordinator(history_fault);
    auto historical = history_coordinator.begin(options("verified-history-snapshot", "", true));
    historical->stage({Json::array({record("a", 1)}), {}, {}});
    historical->commit();
    history_fault->fail_blob_reads = true;
    error("storage_unavailable", [&] { historical->snapshot(); });
    error("storage_unavailable", [&] { history_coordinator.snapshot("test"); });
}
void explicit_history_budget_and_material() {
    auto store = std::make_shared<MemoryStore>();
    store->create_workspace("test");
    TransactionCoordinator coordinator(store);
    const auto item = record("a", 1);
    const Json selected = {{"revision", "0"}, {"records", Json::array({item})}};
    const auto bundle =
        gr::history::append({{"format", "gr.history/1"}, {"nodes", Json::array()}}, selected, Json::array(),
                            {{"agent", "test"}, {"method", "explicit"}, {"reason", "budget"}});
    auto exact = options("exact-history-budget");
    exact.max_staged_bytes = item.dump().size() + bundle.dump().size();
    auto tx = coordinator.begin(exact);
    tx->stage({Json::array({item}), {}, {}});
    tx->stage_history({bundle, bundle["nodes"].back()["version"]});
    check(tx->commit().state == TransactionState::committed,
          "Explicit history bytes are charged exactly once");

    auto other = std::make_shared<MemoryStore>();
    other->create_workspace("test");
    auto material_record = record("material", 1).at("document");
    const auto blob = material({1, 2, 3});
    material_record["artifacts"] = Json::array(
        {{{"sha256", blob.digest}, {"byte_length", 3}, {"media_type", "application/octet-stream"}}});
    auto first = gr::history::append(
        {{"format", "gr.history/1"}, {"nodes", Json::array()}},
        {{"revision", "0"}, {"records", Json::array({gr::core::construct(material_record)})}}, Json::array(),
        {{"agent", "test"}, {"method", "source"}, {"reason", "old artifact"}});
    const auto replacement = record("material", 2);
    auto second = gr::history::append(first, {{"revision", "0"}, {"records", Json::array({replacement})}},
                                      Json::array({first["nodes"].back()["version"]}),
                                      {{"agent", "test"}, {"method", "revision"}, {"reason", "new record"}});
    auto missing = TransactionCoordinator(other).begin(options("missing-history-artifact"));
    missing->stage({Json::array({replacement}), {}, {}});
    missing->stage_history({second, second["nodes"].back()["version"]});
    error("not_found", [&] { missing->commit(); });
    check(other->head("test") == "0" && !other->receipt("test", "missing-history-artifact"),
          "Unselected historical material remains required before publication");
}
void simultaneous(const std::shared_ptr<IStore> &a, const std::shared_ptr<IStore> &b) {
    a->create_workspace("test");
    TransactionCoordinator ca(a), cb(b);
    auto first = ca.begin(options("race-a")), second = cb.begin(options("race-b"));
    first->stage({Json::array({record("a", 1)}), {}, {}});
    second->stage({Json::array({record("b", 2)}), {}, {}});
    std::barrier start(3);
    std::atomic<int> committed = 0, conflicts = 0, unexpected = 0;
    auto run = [&](const std::shared_ptr<Transaction> &tx) {
        start.arrive_and_wait();
        try {
            tx->commit();
            ++committed;
        } catch (const gr::Error &e) {
            if (e.code == "conflict")
                ++conflicts;
            else
                ++unexpected;
        } catch (...) {
            ++unexpected;
        }
    };
    std::thread one(run, first), two(run, second);
    start.arrive_and_wait();
    one.join();
    two.join();
    check(committed == 1 && conflicts == 1 && unexpected == 0 && a->history("test").entries.size() == 1,
          "Concurrent runtime coordinators retain exactly one workspace CAS winner");
}
void exhausted(const std::shared_ptr<IStore> &store) {
    store->create_workspace("test");
    TransactionCoordinator coordinator(store);
    const auto blob = material({1, 2, 3, 4});
    auto transaction = coordinator.begin(options("exhausted"));
    transaction->stage({Json::array({record("a", 1)}), {blob}, {blob.digest}});
    error("transaction_outcome_unknown", [&] { transaction->commit(); });
    check(transaction->recover().state == TransactionState::not_observed && store->head("test") == "0",
          "Backend retained-quota failure leaves no accepted receipt or record publication");
    error("not_found", [&] { store->blob_info("test", blob.digest); });
}
} // namespace
int main() {
    const auto folder = temporary_folder("transactions");
    try {
        conformance(std::make_shared<MemoryStore>());
        conformance(std::make_shared<SQLiteStore>((folder / "conformance.sqlite").string()));
        history_contract(std::make_shared<MemoryStore>());
        history_contract(std::make_shared<SQLiteStore>((folder / "history.sqlite").string()));
        uncertainty();
        legacy_identity();
        semantic_guards();
        explicit_history_budget_and_material();
        auto shared = std::make_shared<MemoryStore>();
        simultaneous(shared, shared);
        auto race_a = std::make_shared<SQLiteStore>((folder / "race.sqlite").string());
        auto race_b = std::make_shared<SQLiteStore>((folder / "race.sqlite").string());
        simultaneous(race_a, race_b);
        race_a.reset();
        race_b.reset();
        StorageLimits limits;
        limits.blob_bytes_per_workspace = 3;
        exhausted(std::make_shared<MemoryStore>(limits));
        exhausted(std::make_shared<SQLiteStore>((folder / "exhausted.sqlite").string(), false, limits));
        cleanup(folder);
        std::cout << "Runtime transaction conformance: staging, isolation, rollback, history, limits, "
                     "cancellation, unknown outcomes and exact recovery passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Transaction conformance failed: " << e.what() << "\nArtifacts: " << folder << '\n';
        return 1;
    }
}
