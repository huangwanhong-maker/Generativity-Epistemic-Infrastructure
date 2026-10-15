#include "gr/runtime/transaction.hpp"
#include "gr/storage/memory_store.hpp"
#include "gr/storage/sqlite_store.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

// Compile this unchanged against the preserved v0.3 and current installed SDKs.
// Timers wrap the public storage boundary; they do not estimate CPU attribution.
using namespace gr;
using namespace gr::storage;
using Clock = std::chrono::steady_clock;
double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
class MeasuredStore final : public IStore {
  public:
    std::shared_ptr<IStore> delegate;
    std::map<std::string, double> times;
    std::map<std::string, std::size_t> counts;
    std::map<std::string, std::size_t> retained;
    explicit MeasuredStore(std::shared_ptr<IStore> store) : delegate(std::move(store)) {}
    template <class F> auto timed(const char *label, F &&function) {
        auto start = Clock::now();
        auto value = function();
        times[label] += milliseconds(start);
        ++counts[label];
        return value;
    }
    StorageCapabilities describe() const override { return delegate->describe(); }
    void create_workspace(const std::string &w) override { delegate->create_workspace(w); }
    std::string head(const std::string &w) override { return timed("head", [&] { return delegate->head(w); }); }
    Snapshot snapshot(const std::string &w, const std::string &r = "") override {
        return timed("snapshot", [&] { return delegate->snapshot(w, r); });
    }
    StoredRecord record_version(const std::string &w, const std::string &i, const std::string &v) override {
        return timed("record_version", [&] { return delegate->record_version(w, i, v); });
    }
    Result commit(const Commit &c) override {
        auto result = timed("commit", [&] { return delegate->commit(c); });
        for (const auto &b : c.blobs) retained.emplace(b.digest, b.bytes.size());
        return result;
    }
    HistoryPage history(const std::string &w, const std::string &b = "", std::size_t n = 100) override {
        return timed("history", [&] { return delegate->history(w, b, n); });
    }
    std::optional<TransactionReceipt> receipt(const std::string &w, const std::string &t) override {
        return timed("receipt", [&] { return delegate->receipt(w, t); });
    }
    BlobMetadata blob_info(const std::string &w, const std::string &d) override {
        return timed("blob_info", [&] { return delegate->blob_info(w, d); });
    }
    std::unique_ptr<BlobReader> open_blob(const std::string &w, const std::string &d, BlobReadOptions o = {}) override {
        return timed("open_blob", [&] { return delegate->open_blob(w, d, o); });
    }
};
Json item(int id, int value) {
    return core::construct({{"format", "gr.record/1"}, {"id", "item:" + std::to_string(id)},
                            {"type", "gr.assertion@1"},
                            {"payload", {{"value", value}, {"padding", std::string(256, 'x')}}},
                            {"attribution", {{"agent", "benchmark"}, {"method", "synthetic"}}}});
}
int main(int argc, char **argv) {
    try {
        if (argc != 2) throw Error("arguments", "Supply a new benchmark directory");
        std::filesystem::path directory(argv[1]);
        if (std::filesystem::exists(directory)) throw Error("arguments", "Output directory must not exist");
        std::filesystem::create_directories(directory);
        Json rows = Json::array();
        for (const bool sqlite : {false, true}) for (const bool tracked : {false, true}) {
            std::shared_ptr<IStore> backend;
            if (sqlite) backend = std::make_shared<SQLiteStore>((directory / (tracked ? "tracked.sqlite" : "plain.sqlite")).string());
            else backend = std::make_shared<MemoryStore>();
            auto store = std::make_shared<MeasuredStore>(backend);
            store->create_workspace("bench");
            runtime::TransactionCoordinator coordinator(store);
            runtime::TransactionOptions options;
            options.workspace = "bench"; options.actor = "benchmark"; options.reason = "Fixed admission profile";
            options.transaction_id = "seed"; options.track_history = tracked;
            Json records = Json::array();
            for (int i = 0; i < 128; ++i) records.push_back(item(i, 0));
            coordinator.commit(options, {records, {}, {}});
            store->times.clear(); store->counts.clear();
            double begin = 0, stage = 0, commit = 0;
            for (int i = 0; i < 32; ++i) {
                options.transaction_id = "change:" + std::to_string(i);
                auto update = item(i, i + 1); // Construction deliberately outside admission timings.
                auto start = Clock::now();
                auto tx = coordinator.begin(options); begin += milliseconds(start);
                start = Clock::now(); tx->stage({Json::array({update}), {}, {}}); stage += milliseconds(start);
                start = Clock::now(); tx->commit(); commit += milliseconds(start);
            }
            auto boundary_times = store->times;
            auto boundary_counts = store->counts;
            auto start = Clock::now(); auto selected = coordinator.snapshot("bench");
            auto selected_ms = milliseconds(start);
            if (selected.at("records").size() != 128 || store->history("bench").entries.size() != 33)
                throw Error("verification", "Incorrect final inventory or receipt count");
            std::size_t retained_bytes = 0;
            for (const auto &[digest, size] : store->retained) { (void)digest; retained_bytes += size; }
            rows.push_back({{"backend", sqlite ? "sqlite" : "memory"}, {"tracked", tracked},
                            {"records", 128}, {"commits", 32}, {"begin_ms", begin}, {"stage_ms", stage},
                            {"commit_ms", commit}, {"storage_boundary_ms", boundary_times},
                            {"storage_calls", boundary_counts}, {"selected_snapshot_ms", selected_ms},
                            {"retained_blob_bytes", retained_bytes}, {"retained_blob_count", store->retained.size()},
                            {"verification", "128 current records; 33 physical publications"}});
        }
        std::cout << Json({{"suite", "gr.admission.profile/1"}, {"rows", rows}}).dump(2) << '\n';
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
