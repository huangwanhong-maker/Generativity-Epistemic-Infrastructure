#include "gr/runtime/transaction.hpp"
#include "gr/kernel/history.hpp"
#include "gr/runtime/history_store.hpp"
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>

namespace gr::runtime {
namespace {
constexpr std::size_t snapshot_budget = 32 * 1024 * 1024;
constexpr std::size_t max_history_bytes = 16 * 1024 * 1024;
void require(bool condition, const char *code, const std::string &message) {
    if (!condition)
        throw Error(code, message);
}
void account(std::size_t &total, std::size_t amount, std::size_t limit) {
    require(amount <= limit && total <= limit - amount, "transaction_limit",
            "Transaction resource budget exceeded");
    total += amount;
}
void control(const TransactionControl &c) {
    require(!c.stop.stop_requested(), "transaction_cancelled", "Transaction cancelled before publication");
    require(!c.deadline || std::chrono::steady_clock::now() < *c.deadline, "transaction_deadline",
            "Transaction deadline elapsed before publication");
}
using Clock = std::chrono::steady_clock;
double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
struct WriterGate {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::shared_ptr<int>> waiting;
    bool occupied = false;
};
class WriterLease {
  public:
    explicit WriterLease(std::shared_ptr<WriterGate> gate) : gate_(std::move(gate)) {}
    ~WriterLease() {
        std::lock_guard lock(gate_->mutex);
        gate_->occupied = false;
        gate_->changed.notify_all();
    }
    WriterLease(const WriterLease &) = delete;
    WriterLease &operator=(const WriterLease &) = delete;

  private:
    std::shared_ptr<WriterGate> gate_;
};
std::unique_ptr<WriterLease> writer_lease(storage::IStore &store, const std::string &workspace,
                                          TransactionControl c) {
    static std::mutex domains_mutex;
    static std::map<std::pair<std::string, std::string>, std::weak_ptr<WriterGate>> domains;
    auto identity = store.coordination_identity();
    if (identity.empty())
        identity = "object:" + std::to_string(reinterpret_cast<std::uintptr_t>(&store));
    std::shared_ptr<WriterGate> gate;
    {
        std::lock_guard lock(domains_mutex);
        for (auto it = domains.begin(); it != domains.end();)
            if (it->second.expired())
                it = domains.erase(it);
            else
                ++it;
        auto &entry = domains[{identity, workspace}];
        gate = entry.lock();
        if (!gate)
            entry = gate = std::make_shared<WriterGate>();
    }
    auto ticket = std::make_shared<int>(0);
    std::unique_lock lock(gate->mutex);
    gate->waiting.push_back(ticket);
    try {
        for (;;) {
            control(c);
            if (!gate->occupied && gate->waiting.front() == ticket)
                break;
            auto until = Clock::now() + std::chrono::milliseconds(5);
            if (c.deadline)
                until = std::min(until, *c.deadline);
            gate->changed.wait_until(lock, until);
        }
        // Allocate before changing gate state, so allocation failure cannot
        // strand the domain. The lease may be released by any thread.
        auto lease = std::make_unique<WriterLease>(gate);
        gate->occupied = true;
        gate->waiting.pop_front();
        return lease;
    } catch (...) {
        std::erase(gate->waiting, ticket);
        gate->changed.notify_all();
        throw;
    }
}
Json wrapper(const storage::StoredRecord &r) {
    require(r.data.is_object() && r.data.value("id", std::string{}) == r.id, "runtime_record",
            "Physical record identity differs from its semantic envelope");
    return {{"document", r.data}, {"version", r.version}};
}
struct Loaded {
    Json snapshot;
    std::optional<Json> manifest;
};
Loaded load(storage::IStore &store, const std::string &workspace, const std::string &revision) {
    const auto s = store.snapshot(workspace, revision);
    Loaded out{{{"workspace", workspace}, {"revision", s.revision}, {"records", Json::array()}}, {}};
    for (const auto &r : s.records) {
        auto value = wrapper(r);
        if (r.id == history_manifest_id) {
            require(r.data.contains("type") && r.data.at("type").is_string() &&
                        history_manifest_type_supported(r.data.at("type").get<std::string>()),
                    "history_manifest", "Reserved runtime history identity has the wrong record type");
            out.manifest = std::move(value);
        } else
            out.snapshot["records"].push_back(std::move(value));
    }
    return out;
}
void hydrate(Json &snapshot, const Json &updates, storage::IStore &store, const std::string &workspace) {
    std::size_t bytes = 0;
    std::vector<const Json *> pending;
    std::set<std::pair<std::string, std::string>> known;
    for (const auto *group : {static_cast<const Json *>(&snapshot.at("records")), &updates}) {
        for (const auto &r : *group) {
            account(bytes, r.dump().size(), snapshot_budget);
            pending.push_back(&r);
            known.emplace(r.at("document").at("id").get<std::string>(), r.at("version").get<std::string>());
        }
    }
    require(pending.size() <= 11000, "transaction_limit", "Too many source/update records");
    Json archive = Json::array();
    for (const auto *r : pending) {
        const auto &document = r->at("document");
        for (const char *field : {"references", "participants"}) {
            if (!document.contains(field))
                continue;
            for (const auto &entry : document.at(field)) {
                const auto &ref = std::string_view(field) == "participants" ? entry.at("reference") : entry;
                if (ref.at("scope") != "record" || !ref.contains("version"))
                    continue;
                const auto id = ref.at("id").get<std::string>(),
                           version = ref.at("version").get<std::string>();
                require(id != history_manifest_id, "runtime_reference",
                        "User records cannot reference runtime control records");
                if (!known.emplace(id, version).second)
                    continue;
                auto retained = wrapper(store.record_version(workspace, id, version));
                account(bytes, retained.dump().size(), snapshot_budget);
                archive.push_back(std::move(retained));
            }
        }
    }
    if (!archive.empty())
        snapshot["archive"] = std::move(archive);
}
void valid(const Json &snapshot, bool mutation, const core::TypeRegistry &registry) {
    const auto report = core::validate_snapshot(snapshot, mutation, registry);
    require(report.at("valid").get<bool>(), "admission", report.at("errors").dump());
}
Json history_info(const Json &manifest) {
    return history_manifest_info(manifest);
}
std::map<std::string, std::string> inventory(const Json &snapshot) {
    std::map<std::string, std::string> result;
    for (const auto &r : snapshot.at("records"))
        result.emplace(r.at("document").at("id"), r.at("version"));
    return result;
}
void check_retained_history(const std::shared_ptr<storage::IStore> &store, const std::string &workspace,
                            const Json &manifest, const Json &snapshot,
                            const std::shared_ptr<const core::TypeRegistry> &registry) {
    const auto selected = HistoryStore(store, workspace, registry).read(manifest);
    require(inventory(selected.snapshot) == inventory(snapshot), "history_state",
            "Retained history head differs from the pinned physical user snapshot");
}
} // namespace

std::string to_string(TransactionState state) {
    switch (state) {
    case TransactionState::active:
        return "active";
    case TransactionState::preparing:
        return "preparing";
    case TransactionState::publishing:
        return "publishing";
    case TransactionState::committed:
        return "committed";
    case TransactionState::rolled_back:
        return "rolled_back";
    case TransactionState::conflicted:
        return "conflicted";
    case TransactionState::failed:
        return "failed";
    case TransactionState::outcome_unknown:
        return "outcome_unknown";
    case TransactionState::not_observed:
        return "not_observed";
    }
    throw Error("transaction_state", "Unknown transaction state");
}
Json TransactionResult::to_json() const {
    Json result = {{"state", to_string(state)},
                   {"workspace", workspace},
                   {"transaction_id", transaction_id},
                   {"base_revision", base_revision},
                   {"request_digest", request_digest},
                   {"history", history},
                   {"diagnostics", diagnostics},
                   {"recovery_observation", recovery_observation},
                   {"publication", nullptr}};
    if (publication)
        result["publication"] = {{"revision", publication->revision},
                                 {"parent", publication->parent},
                                 {"current_revision", publication->current_revision},
                                 {"replayed", publication->replayed}};
    return result;
}
struct Transaction::Impl {
    std::shared_ptr<storage::IStore> store;
    std::shared_ptr<const core::TypeRegistry> registry;
    std::unique_ptr<core::ValidationSession> validation;
    std::unique_ptr<WriterLease> writer;
    TransactionOptions options;
    Json base;
    std::optional<Json> manifest;
    Stage staged;
    std::optional<HistoryPublication> supplied_history;
    std::optional<storage::Commit> prepared;
    TransactionResult result;
    std::size_t staged_bytes = 0;
    mutable std::mutex mutex;

    void active() const {
        require(result.state == TransactionState::active, "transaction_state",
                "Transaction is not open for staging");
    }
    Json candidate() const {
        auto input = base;
        hydrate(input, staged.records, *store, options.workspace);
        return validation->apply(input, staged.records);
    }
    void prepare(TransactionControl c) {
        control(c);
        const auto admission_start = Clock::now();
        auto selected = candidate();
        result.diagnostics["candidate_admission_ms"] = elapsed_ms(admission_start);
        const auto preparation_start = Clock::now();
        storage::Commit request{options.workspace,      options.expected_revision,
                                options.transaction_id, options.actor,
                                options.reason,         {},
                                staged.blobs,           {}};
        for (const auto &r : staged.records)
            request.records.push_back({r.at("document").at("id"), r.at("version"), r.at("document")});
        std::set<std::string> required(staged.required_blob_digests.begin(),
                                       staged.required_blob_digests.end());
        std::map<std::string, std::size_t> lengths;
        for (const auto &b : request.blobs)
            lengths.emplace(b.digest, b.bytes.size());
        auto artifacts = [&](const Json &records) {
            for (const auto &r : records)
                for (const auto &a : r.at("document").value("artifacts", Json::array())) {
                    const auto digest = a.at("sha256").get<std::string>();
                    required.insert(digest);
                    auto it = lengths.find(digest);
                    if (it == lengths.end())
                        it = lengths.emplace(digest, store->blob_info(options.workspace, digest).byte_length)
                                 .first;
                    require(it->second == a.at("byte_length").get<std::size_t>(), "artifact_size",
                            "Artifact byte length does not match its descriptor");
                }
        };
        artifacts(selected.at("records"));
        if (options.track_history || options.upgrade_history || manifest || supplied_history) {
            HistoryStore history(store, options.workspace, registry);
            const Json attribution = {{"agent", options.actor},
                                      {"method", "gr.runtime.transaction@1"},
                                      {"reason", options.reason},
                                      {"transaction_id", options.transaction_id},
                                      {"base_revision", options.expected_revision}};
            auto write = supplied_history
                             ? history.import_v1(manifest, supplied_history->bundle, supplied_history->head,
                                                 attribution, {}, options.upgrade_history)
                             : history.append(manifest, selected, attribution, {}, options.upgrade_history);
            require(inventory(write.selected_snapshot) == inventory(selected), "history_state",
                    "Selected history checkpoint differs from the exact candidate record inventory");
            const auto admitted = validation->validate_snapshot(write.selected_snapshot, true);
            require(admitted.at("valid").get<bool>(), "admission", admitted.at("errors").dump());
            for (const auto &[digest, length] : write.artifact_lengths) {
                auto found = lengths.find(digest);
                const auto size = found == lengths.end()
                                      ? store->blob_info(options.workspace, digest).byte_length
                                      : found->second;
                require(size == length, "artifact_size",
                        "Historical artifact byte length does not match its descriptor");
                required.insert(digest);
            }
            auto total = staged_bytes;
            for (auto &blob : write.blobs) {
                if (!supplied_history)
                    account(total, blob.bytes.size(), options.max_staged_bytes);
                const auto found = std::find_if(request.blobs.begin(), request.blobs.end(),
                                                [&](const auto &b) { return b.digest == blob.digest; });
                if (found == request.blobs.end())
                    request.blobs.push_back(std::move(blob));
                else
                    require(found->bytes == blob.bytes, "integrity_error",
                            "Staged blob disagrees with encoded history object");
            }
            required.insert(write.required_blob_digests.begin(), write.required_blob_digests.end());
            const auto &record = write.manifest;
            request.records.push_back({history_manifest_id, record.at("version"), record.at("document")});
            result.history = history_info(record);
            result.diagnostics["history"] = write.diagnostics;
        }
        request.required_blob_digests.assign(required.begin(), required.end());
        control(c);
        result.request_digest = storage::request_digest(request, store->describe().limits);
        prepared = std::move(request);
        result.diagnostics["publication_preparation_ms"] = elapsed_ms(preparation_start);
        result.diagnostics["validation"] = validation->statistics();
    }
    TransactionResult publish(TransactionControl c) {
        try {
            control(c);
        } catch (...) {
            if (result.state == TransactionState::preparing)
                result.state = TransactionState::failed;
            writer.reset();
            throw;
        }
        const bool acknowledged = result.publication.has_value();
        result.state = TransactionState::publishing;
        const auto physical_start = Clock::now();
        try {
            const auto published = store->commit(*prepared);
            result.diagnostics["physical_commit_ms"] = elapsed_ms(physical_start);
            writer.reset();
            result.publication = published;
            result.state = TransactionState::committed;
            result.recovery_observation.clear();
            return result; // A completed commit wins a cancellation arriving after publication.
        } catch (const Error &e) {
            result.diagnostics["physical_commit_ms"] = elapsed_ms(physical_start);
            writer.reset();
            if (acknowledged) {
                result.state = TransactionState::committed;
                throw;
            }
            if (e.code == "conflict" || e.code == "transaction_conflict" || e.code == "read_only") {
                result.state = TransactionState::conflicted;
                throw;
            }
            result.state = TransactionState::outcome_unknown;
            throw Error("transaction_outcome_unknown",
                        "Backend publication failed without an acknowledged outcome; recover or retry the "
                        "same request",
                        {{"cause", e.code}, {"transaction_id", options.transaction_id}});
        } catch (...) {
            result.diagnostics["physical_commit_ms"] = elapsed_ms(physical_start);
            writer.reset();
            if (acknowledged) {
                result.state = TransactionState::committed;
                throw;
            }
            result.state = TransactionState::outcome_unknown;
            throw Error("transaction_outcome_unknown",
                        "Backend publication outcome is unknown; recover or retry the same request");
        }
    }
};

Transaction::Transaction(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Transaction::~Transaction() = default;
TransactionState Transaction::state() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->result.state;
}
TransactionOptions Transaction::options() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->options;
}
TransactionResult Transaction::result() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->result;
}
Json Transaction::snapshot() const {
    std::lock_guard lock(impl_->mutex);
    require(impl_->result.state != TransactionState::rolled_back, "transaction_state",
            "Transaction was rolled back");
    if (impl_->options.read_only) {
        auto result = impl_->base;
        if (impl_->manifest)
            result["runtime_history"] = history_info(*impl_->manifest);
        return result;
    }
    if (impl_->result.publication) {
        auto retained = load(*impl_->store, impl_->options.workspace, impl_->result.publication->revision);
        hydrate(retained.snapshot, Json::array(), *impl_->store, impl_->options.workspace);
        valid(retained.snapshot, false, *impl_->registry);
        if (retained.manifest) {
            check_retained_history(impl_->store, impl_->options.workspace, *retained.manifest,
                                   retained.snapshot, impl_->registry);
            retained.snapshot["runtime_history"] = history_info(*retained.manifest);
        }
        return retained.snapshot;
    }
    return impl_->candidate();
}
void Transaction::stage(const Stage &incoming) {
    std::lock_guard lock(impl_->mutex);
    impl_->active();
    require(!impl_->options.read_only, "read_only", "Cannot stage into a read-only transaction");
    require(incoming.records.is_array(), "updates", "Staged records must be an array");
    const auto limits = impl_->store->describe().limits;
    require(incoming.records.size() <=
                limits.records_per_commit - std::min(limits.records_per_commit, impl_->staged.records.size()),
            "transaction_limit", "Too many staged record updates");
    require(incoming.blobs.size() <=
                limits.blobs_per_commit - std::min(limits.blobs_per_commit, impl_->staged.blobs.size()),
            "transaction_limit", "Too many staged blobs");
    std::set<std::string> ids, blobs, required;
    for (const auto &r : impl_->staged.records)
        ids.insert(r.at("document").at("id"));
    for (const auto &b : impl_->staged.blobs)
        blobs.insert(b.digest);
    required.insert(impl_->staged.required_blob_digests.begin(), impl_->staged.required_blob_digests.end());
    auto bytes = impl_->staged_bytes;
    for (const auto &r : incoming.records) {
        const auto report = impl_->validation->verify(r);
        require(report.at("valid").get<bool>(), "invalid_record", report.at("errors").dump());
        const auto id = r.at("document").at("id").get<std::string>();
        require(id != history_manifest_id, "reserved_record",
                "Runtime history manifest cannot be staged as a user record");
        require(ids.insert(id).second, "duplicate_update", "Each record identity can be staged only once");
        account(bytes, r.dump().size(), impl_->options.max_staged_bytes);
    }
    for (const auto &b : incoming.blobs) {
        require(blobs.insert(b.digest).second, "duplicate_blob", "A blob digest can be staged only once");
        require(b.bytes.size() <= limits.blob_bytes, "transaction_limit",
                "Staged blob exceeds backend limit");
        account(bytes, b.bytes.size(), impl_->options.max_staged_bytes);
        require(b.digest == "sha256:" + sha256(std::span<const std::uint8_t>(b.bytes)), "integrity_error",
                "Staged blob digest differs from bytes");
    }
    for (const auto &digest : incoming.required_blob_digests) {
        require(digest.size() == 71 && digest.starts_with("sha256:") &&
                    digest.find_first_not_of("0123456789abcdef", 7) == std::string::npos,
                "invalid_argument", "Required blob digest is malformed");
        if (required.insert(digest).second)
            account(bytes, digest.size(), impl_->options.max_staged_bytes);
    }
    require(required.size() <= 10000, "transaction_limit", "Too many required artifacts");
    auto next = impl_->staged;
    for (const auto &r : incoming.records)
        next.records.push_back(r);
    next.blobs.insert(next.blobs.end(), incoming.blobs.begin(), incoming.blobs.end());
    next.required_blob_digests.assign(required.begin(), required.end());
    impl_->staged = std::move(next);
    impl_->staged_bytes = bytes;
}
void Transaction::stage_history(const HistoryPublication &history) {
    std::lock_guard lock(impl_->mutex);
    impl_->active();
    require(!impl_->options.read_only, "read_only", "Cannot stage history into a read-only transaction");
    require(!impl_->supplied_history, "duplicate_history",
            "Historical publication can be supplied only once");
    (void)gr::history::reconstruct(history.bundle, history.head, *impl_->registry);
    const auto size = history.bundle.dump().size();
    require(size <= max_history_bytes, "transaction_limit", "History bundle exceeds 16 MiB");
    auto bytes = impl_->staged_bytes;
    account(bytes, size, impl_->options.max_staged_bytes);
    impl_->supplied_history = history;
    impl_->staged_bytes = bytes;
}
TransactionResult Transaction::commit(TransactionControl c) {
    std::lock_guard lock(impl_->mutex);
    impl_->active();
    impl_->result.state = TransactionState::preparing;
    try {
        control(c);
        if (impl_->options.read_only) {
            valid(impl_->base, false, *impl_->registry);
            impl_->result.state = TransactionState::committed;
            impl_->result.recovery_observation = "read_only_snapshot_completed; no physical publication";
            return impl_->result;
        }
        // An existing receipt may represent a legitimate exact replay from an
        // older base. Do not reject it before freezing/comparing its request.
        if (impl_->store->head(impl_->options.workspace) != impl_->options.expected_revision &&
            !impl_->store->receipt(impl_->options.workspace, impl_->options.transaction_id)) {
            impl_->result.state = TransactionState::conflicted;
            throw Error("conflict", "Expected revision is stale before semantic preparation");
        }
        impl_->prepare(c);
        control(c);
    } catch (...) {
        if (impl_->result.state != TransactionState::conflicted)
            impl_->result.state = TransactionState::failed;
        impl_->writer.reset();
        throw;
    }
    return impl_->publish(c);
}
TransactionResult Transaction::retry(TransactionControl c) {
    std::lock_guard lock(impl_->mutex);
    require(impl_->prepared && impl_->result.state != TransactionState::rolled_back, "transaction_state",
            "No frozen publication request is available");
    return impl_->publish(c);
}
TransactionResult Transaction::recover() {
    std::lock_guard lock(impl_->mutex);
    require(impl_->prepared && impl_->result.state != TransactionState::rolled_back, "transaction_state",
            "No frozen publication request is available");
    const auto receipt = impl_->store->receipt(impl_->options.workspace, impl_->options.transaction_id);
    if (!receipt) {
        require(!impl_->result.publication, "integrity_error",
                "A previously acknowledged transaction receipt is now absent");
        impl_->result.state = TransactionState::not_observed;
        impl_->result.recovery_observation =
            "No receipt observed at this read; this does not exclude a concurrent later publication";
        return impl_->result;
    }
    require(receipt->entry.request_digest == impl_->result.request_digest &&
                receipt->entry.actor == impl_->options.actor,
            "transaction_conflict", "Retained transaction identity belongs to a different request");
    impl_->result.publication =
        storage::Result{receipt->entry.revision, receipt->entry.parent, receipt->current_revision, true};
    impl_->result.state = TransactionState::committed;
    impl_->result.recovery_observation = "matching_committed_receipt";
    return impl_->result;
}
void Transaction::rollback() {
    std::lock_guard lock(impl_->mutex);
    require(impl_->result.state == TransactionState::active ||
                impl_->result.state == TransactionState::failed ||
                impl_->result.state == TransactionState::rolled_back,
            "transaction_state", "Cannot roll back a published, conflicting or uncertain transaction");
    impl_->staged = {};
    impl_->supplied_history.reset();
    impl_->prepared.reset();
    impl_->staged_bytes = 0;
    impl_->writer.reset();
    impl_->result.state = TransactionState::rolled_back;
}

TransactionCoordinator::TransactionCoordinator(std::shared_ptr<storage::IStore> store,
                                               std::shared_ptr<const core::TypeRegistry> registry)
    : store_(std::move(store)) {
    require(bool(store_), "invalid_argument", "Transaction coordinator requires a storage adapter");
    auto copied = std::make_shared<core::TypeRegistry>(registry ? *registry : core::default_types());
    register_history_profiles(*copied);
    registry_ = std::move(copied);
}
std::shared_ptr<Transaction> TransactionCoordinator::begin(TransactionOptions options, TransactionControl c) {
    const auto begin_start = Clock::now();
    control(c);
    require(options.isolation == Isolation::workspace_snapshot_cas, "unsupported_isolation",
            "Only pinned workspace snapshots with whole-workspace CAS are supported");
    require(options.max_staged_bytes > 0 && options.max_staged_bytes <= 128 * 1024 * 1024,
            "transaction_limit", "Staging budget must be within 1..128 MiB");
    auto requirements = options.requirements;
    requirements.consistent_snapshots = requirements.immutable_versions = true;
    if (!options.read_only)
        requirements.writable = requirements.atomic_commit = requirements.conditional_commit = true;
    storage::require_support(store_->describe(), requirements);
    require(options.admission == AdmissionPolicy::optimistic ||
                options.admission == AdmissionPolicy::serialized,
            "invalid_argument", "Unknown writer admission policy");
    require(options.admission != AdmissionPolicy::serialized ||
                (!options.read_only && options.expected_revision.empty()),
            "invalid_argument",
            "Serialized admission requires a writable transaction selecting current head");
    const auto wait_start = Clock::now();
    auto writer = options.admission == AdmissionPolicy::serialized
                      ? writer_lease(*store_, options.workspace, c)
                      : nullptr;
    const auto wait_ms = elapsed_ms(wait_start);
    auto loaded = load(*store_, options.workspace, options.expected_revision);
    options.expected_revision = loaded.snapshot.at("revision");
    // Reuse exact L6 identity validation, without holding a physical transaction.
    (void)storage::request_digest({options.workspace,
                                   options.expected_revision,
                                   options.transaction_id,
                                   options.actor,
                                   options.reason,
                                   {},
                                   {},
                                   {}},
                                  store_->describe().limits);
    auto impl = std::make_unique<Transaction::Impl>();
    impl->store = store_;
    impl->registry = registry_;
    impl->validation = std::make_unique<core::ValidationSession>(*registry_);
    impl->writer = std::move(writer);
    impl->options = std::move(options);
    impl->base = std::move(loaded.snapshot);
    impl->manifest = std::move(loaded.manifest);
    hydrate(impl->base, Json::array(), *store_, impl->options.workspace);
    const auto admission = impl->validation->validate_snapshot(impl->base, !impl->options.read_only);
    require(admission.at("valid").get<bool>(), "admission", admission.at("errors").dump());
    if (impl->manifest) {
        check_retained_history(store_, impl->options.workspace, *impl->manifest, impl->base, registry_);
        impl->result.history = history_info(*impl->manifest);
    }
    impl->result.workspace = impl->options.workspace;
    impl->result.transaction_id = impl->options.transaction_id;
    impl->result.base_revision = impl->options.expected_revision;
    impl->result.diagnostics = {{"begin_ms", elapsed_ms(begin_start)},
                                {"admission_wait_ms", wait_ms},
                                {"admission_policy", impl->options.admission == AdmissionPolicy::serialized
                                                         ? "serialized"
                                                         : "optimistic"}};
    control(c);
    return std::shared_ptr<Transaction>(new Transaction(std::move(impl)));
}
Json TransactionCoordinator::snapshot(const std::string &workspace, const std::string &revision) const {
    auto loaded = load(*store_, workspace, revision);
    hydrate(loaded.snapshot, Json::array(), *store_, workspace);
    valid(loaded.snapshot, false, *registry_);
    if (loaded.manifest) {
        check_retained_history(store_, workspace, *loaded.manifest, loaded.snapshot, registry_);
        loaded.snapshot["runtime_history"] = history_info(*loaded.manifest);
    }
    return loaded.snapshot;
}
Json TransactionCoordinator::history_snapshot(const std::string &workspace,
                                              const std::string &semantic_revision,
                                              const std::string &physical_revision) const {
    auto loaded = load(*store_, workspace, physical_revision);
    require(loaded.manifest.has_value(), "history_unavailable",
            "Selected physical snapshot has no semantic history");
    HistoryStore history(store_, workspace, registry_);
    auto head = history.read(*loaded.manifest);
    require(inventory(head.snapshot) == inventory(loaded.snapshot), "history_state",
            "Retained history head differs from pinned physical user inventory");
    auto selected = semantic_revision.empty() || semantic_revision == head.head
                        ? std::move(head)
                        : history.read(*loaded.manifest, semantic_revision);
    auto result = std::move(selected.snapshot);
    result["provenance"] = {{"workspace", workspace},
                            {"physical_revision", loaded.snapshot.at("revision")},
                            {"semantic_revision", selected.head},
                            {"manifest_version", loaded.manifest->at("version")},
                            {"representation", loaded.manifest->at("document").at("type")}};
    result["history_diagnostics"] = selected.diagnostics;
    return result;
}
Json TransactionCoordinator::history_export_v1(const std::string &workspace,
                                               const std::string &physical_revision) const {
    auto loaded = load(*store_, workspace, physical_revision);
    require(loaded.manifest.has_value(), "history_unavailable",
            "Selected physical snapshot has no semantic history");
    check_retained_history(store_, workspace, *loaded.manifest, loaded.snapshot, registry_);
    return HistoryStore(store_, workspace, registry_).export_v1(*loaded.manifest);
}
TransactionResult TransactionCoordinator::commit(TransactionOptions options, const Stage &stage,
                                                 TransactionControl control) {
    auto transaction = begin(std::move(options), control);
    transaction->stage(stage);
    return transaction->commit(control);
}
std::optional<storage::TransactionReceipt>
TransactionCoordinator::receipt(const std::string &workspace, const std::string &transaction_id) const {
    return store_->receipt(workspace, transaction_id);
}
Json TransactionCoordinator::capabilities() const {
    return {
        {"contract", "gr.runtime.transaction/1"},
        {"scope", "one workspace and one backend"},
        {"isolation", "pinned_snapshot_whole_workspace_cas"},
        {"read_only", true},
        {"nested_transactions", false},
        {"savepoints", false},
        {"cross_store_atomicity", false},
        {"writer_admission", {"optimistic", "process-local FIFO serialized; physical CAS always enforced"}},
        {"history_publication", "content-addressed compact history or legacy bundle atomically published"},
        {"recovery", "matching receipt or frozen exact-request retry; absence is not proof of rollback"},
        {"storage", store_->capabilities()}};
}
} // namespace gr::runtime
