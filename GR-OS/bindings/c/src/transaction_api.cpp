#include "internal.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace gr::binding {
namespace {
std::mutex transactions_mutex;
std::map<std::string, std::shared_ptr<runtime::Transaction>> transactions;
std::atomic<std::uint64_t> next_transaction{1};
std::size_t pending_transactions = 0;
void only(const Json &object, std::initializer_list<const char *> allowed) {
    if (!object.is_object())
        throw Error("invalid_request", "Expected an object");
    const std::set<std::string> names(allowed.begin(), allowed.end());
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!names.contains(it.key()))
            throw Error("invalid_request", "Unknown field: " + it.key());
}
struct Reservation {
    bool active = true;
    ~Reservation() {
        if (active) {
            std::lock_guard lock(transactions_mutex);
            --pending_transactions;
        }
    }
};
bool boolean(const Json &object, const char *key, bool fallback = false) {
    if (!object.contains(key))
        return fallback;
    if (!object.at(key).is_boolean())
        throw Error("invalid_request", std::string(key) + " must be a boolean");
    return object.at(key).get<bool>();
}
runtime::TransactionControl control(const Json &request, std::stop_source &stop) {
    runtime::TransactionControl result;
    if (boolean(request, "cancelled"))
        stop.request_stop();
    result.stop = stop.get_token();
    if (request.contains("deadline_ms")) {
        auto ms = count(request, "deadline_ms", 0);
        if (ms > 86400000)
            throw Error("invalid_request", "deadline_ms exceeds one day");
        result.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    }
    return result;
}
} // namespace
std::shared_ptr<runtime::Transaction> transaction_for(const std::string &handle) {
    std::lock_guard lock(transactions_mutex);
    auto found = transactions.find(handle);
    if (found == transactions.end())
        throw Error("invalid_handle", "Transaction is closed or unknown");
    return found->second;
}
runtime::TransactionOptions transaction_options(const Json &request) {
    runtime::TransactionOptions options;
    options.workspace = text(request, "workspace");
    options.expected_revision = text(request, "expected_revision");
    options.transaction_id = text(request, "transaction_id");
    options.actor = text(request, "actor");
    options.reason = text(request, "reason");
    options.read_only = boolean(request, "read_only");
    options.track_history = boolean(request, "track_history");
    options.upgrade_history = boolean(request, "upgrade_history");
    const auto admission = request.value("admission", std::string("optimistic"));
    if (admission == "serialized")
        options.admission = runtime::AdmissionPolicy::serialized;
    else if (admission != "optimistic")
        throw Error("invalid_request", "admission must be optimistic or serialized");
    options.max_staged_bytes = count(request, "max_staged_bytes", options.max_staged_bytes);
    if (request.contains("isolation") && text(request, "isolation") != "workspace_snapshot_cas")
        throw Error("unsupported_isolation", "Only workspace_snapshot_cas is supported");
    if (request.contains("requirements")) {
        const auto &r = request.at("requirements");
        if (!r.is_object())
            throw Error("invalid_request", "requirements must be an object");
        const std::map<std::string, bool storage::StorageRequirements::*> members = {
            {"persistent", &storage::StorageRequirements::persistent},
            {"writable", &storage::StorageRequirements::writable},
            {"atomic_commit", &storage::StorageRequirements::atomic_commit},
            {"consistent_snapshots", &storage::StorageRequirements::consistent_snapshots},
            {"immutable_versions", &storage::StorageRequirements::immutable_versions},
            {"conditional_commit", &storage::StorageRequirements::conditional_commit},
            {"verified_blob_ranges", &storage::StorageRequirements::verified_blob_ranges},
            {"blob_read_cancellation", &storage::StorageRequirements::blob_read_cancellation},
            {"reader_outlives_store", &storage::StorageRequirements::reader_outlives_store}};
        for (auto it = r.begin(); it != r.end(); ++it) {
            if (it.key() == "history_model") {
                const auto model = text(r, "history_model");
                if (model == "linear")
                    options.requirements.history_model = storage::HistoryModel::linear;
                else if (model == "dag")
                    options.requirements.history_model = storage::HistoryModel::dag;
                else
                    throw Error("invalid_request", "history_model must be linear or dag");
            } else {
                auto found = members.find(it.key());
                if (found == members.end())
                    throw Error("invalid_request", "Unknown storage requirement: " + it.key());
                options.requirements.*found->second = boolean(r, it.key().c_str());
            }
        }
    }
    return options;
}
runtime::Stage transaction_stage(const Json &request) {
    runtime::Stage stage;
    if (request.contains("records"))
        stage.records = array(request, "records");
    if (request.contains("required_blob_digests"))
        for (const auto &digest : array(request, "required_blob_digests")) {
            if (!digest.is_string())
                throw Error("invalid_request", "Required digests must be strings");
            stage.required_blob_digests.push_back(digest.get<std::string>());
        }
    std::size_t decoded = 0;
    if (request.contains("blobs"))
        for (const auto &blob : array(request, "blobs")) {
            const auto encoded = text(blob, "data_base64");
            const auto estimate = encoded.size() / 4 * 3;
            if (estimate > 16 * 1024 * 1024 || estimate > 64 * 1024 * 1024 - decoded)
                throw Error("request_limit", "Staged binary input exceeds transfer bounds");
            auto bytes = decode(encoded);
            decoded += bytes.size();
            stage.blobs.push_back({text(blob, "digest"), std::move(bytes)});
        }
    return stage;
}
std::optional<Json> transaction_call(const Json &request, const std::string &op) {
    if (op.rfind("transaction.", 0) != 0)
        return std::nullopt;
    if (op == "transaction.begin") {
        only(request, {"abi", "op", "handle", "workspace", "expected_revision", "transaction_id", "actor",
                       "reason", "read_only", "track_history", "isolation", "requirements",
                       "max_staged_bytes", "admission", "upgrade_history", "cancelled", "deadline_ms"});
        auto store = store_for(request);
        {
            std::lock_guard lock(transactions_mutex);
            if (transactions.size() + pending_transactions >= 1024)
                throw Error("resource_limit", "At most 1024 open transactions");
            ++pending_transactions;
        }
        Reservation reservation;
        std::stop_source stop;
        auto transaction = runtime::TransactionCoordinator(store).begin(transaction_options(request),
                                                                        control(request, stop));
        const auto handle = "transaction:" + std::to_string(next_transaction.fetch_add(1));
        auto result = transaction->result().to_json();
        {
            std::lock_guard lock(transactions_mutex);
            transactions.emplace(handle, transaction);
            --pending_transactions;
            reservation.active = false;
        }
        return Json{{"handle", handle}, {"transaction", result}};
    }
    if (op == "transaction.close") {
        std::shared_ptr<runtime::Transaction> released;
        {
            std::lock_guard lock(transactions_mutex);
            auto found = transactions.find(text(request, "handle"));
            if (found == transactions.end())
                throw Error("invalid_handle", "Transaction is closed or unknown");
            released = std::move(found->second);
            transactions.erase(found);
        }
        return Json{{"closed", true}};
    }
    auto transaction = transaction_for(text(request, "handle"));
    if (op == "transaction.status")
        return transaction->result().to_json();
    if (op == "transaction.snapshot")
        return transaction->snapshot();
    if (op == "transaction.stage") {
        only(request, {"abi", "op", "handle", "records", "blobs", "required_blob_digests"});
        transaction->stage(transaction_stage(request));
        return transaction->result().to_json();
    }
    if (op == "transaction.stage_history") {
        transaction->stage_history({request.at("history"), text(request, "head")});
        return transaction->result().to_json();
    }
    if (op == "transaction.rollback") {
        transaction->rollback();
        return transaction->result().to_json();
    }
    if (op == "transaction.recover")
        return transaction->recover().to_json();
    if (op == "transaction.commit" || op == "transaction.retry") {
        only(request, {"abi", "op", "handle", "cancelled", "deadline_ms"});
        std::stop_source stop;
        auto settings = control(request, stop);
        return (op == "transaction.commit" ? transaction->commit(settings) : transaction->retry(settings))
            .to_json();
    }
    throw Error("unsupported_operation", "Unknown transaction operation", {{"op", op}});
}
} // namespace gr::binding
