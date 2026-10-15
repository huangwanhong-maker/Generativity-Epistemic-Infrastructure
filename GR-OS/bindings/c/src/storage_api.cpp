#include "gr/c_api.h"
#include "gr/core.hpp"
#include "gr/storage.hpp"
#include "internal.hpp"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stop_token>
namespace gr::binding {
namespace {
std::mutex stores_mutex;
std::map<std::string, std::shared_ptr<gr::storage::IStore>> stores;
std::atomic<std::uint64_t> next_handle{1};
struct ReaderState {
    std::unique_ptr<gr::storage::BlobReader> reader;
    std::stop_source cancellation;
    std::mutex mutex;
};
std::map<std::string, std::shared_ptr<ReaderState>> readers;
std::size_t pending_stores = 0, pending_readers = 0;
// Capacity reservation without holding the registry mutex during database I/O.
struct PendingHandle {
    std::size_t &counter;
    bool active = true;
    ~PendingHandle() {
        if (active) {
            std::lock_guard lock(stores_mutex);
            --counter;
        }
    }
    void published_under_lock() {
        --counter;
        active = false;
    }
};
std::shared_ptr<ReaderState> reader_for(const std::string &handle) {
    std::lock_guard lock(stores_mutex);
    auto found = readers.find(handle);
    if (found == readers.end())
        throw gr::Error("invalid_handle", "Blob reader is closed or unknown");
    return found->second;
}
Json record_json(const gr::storage::StoredRecord &record) {
    return Json{{"document", record.data}, {"version", record.version}};
}
} // namespace
std::shared_ptr<gr::storage::IStore> store_for(const Json &request) {
    const auto handle = text(request, "handle");
    std::lock_guard lock(stores_mutex);
    auto found = stores.find(handle);
    if (found == stores.end())
        throw gr::Error("invalid_handle", "Storage handle is closed or unknown");
    return found->second; // Keeps an in-flight operation alive after close.
}
std::optional<Json> storage_call(const Json &request, const std::string &op) {
    if (op == "blob.close") {
        std::lock_guard lock(stores_mutex);
        if (!readers.erase(text(request, "handle")))
            throw gr::Error("invalid_handle", "Blob reader is closed or unknown");
        return Json{{"closed", true}};
    }
    if (op == "blob.cancel") {
        reader_for(text(request, "handle"))->cancellation.request_stop();
        return Json{{"cancelled", true}};
    }
    if (op == "store.open") {
        // Reserve capacity while constructing so rejected opens create no file.
        {
            std::lock_guard lock(stores_mutex);
            if (stores.size() + pending_stores >= 1024)
                throw gr::Error("resource_limit", "At most 1024 open storage handles");
            ++pending_stores;
        }
        PendingHandle reservation{pending_stores};
        std::shared_ptr<gr::storage::IStore> store;
        const auto backend = text(request, "backend");
        if (backend == "memory") {
            if (request.value("read_only", false))
                throw gr::Error("invalid_request", "Memory backend does not implement read-only handles");
            store = std::make_shared<gr::storage::MemoryStore>();
        } else if (backend == "sqlite")
            store = std::make_shared<gr::storage::SQLiteStore>(text(request, "path"),
                                                               request.value("read_only", false));
        else
            throw gr::Error("unsupported_backend", "Storage backend must be memory or sqlite");
        const auto handle = std::to_string(next_handle.fetch_add(1));
        auto capabilities = store->capabilities();
        {
            std::lock_guard lock(stores_mutex);
            stores.emplace(handle, std::move(store));
            reservation.published_under_lock();
        }
        return Json{{"handle", handle}, {"capabilities", capabilities}};
    }
    if (op == "store.close") {
        std::lock_guard lock(stores_mutex);
        if (!stores.erase(text(request, "handle")))
            throw gr::Error("invalid_handle", "Storage handle is closed or unknown");
        return Json{{"closed", true}};
    }
    if (op.rfind("store.", 0) == 0) {
        auto store = store_for(request);
        if (op == "store.capabilities")
            return store->capabilities();
        if (op == "store.transaction_capabilities")
            return runtime::TransactionCoordinator(store).capabilities();
        const auto workspace = text(request, "workspace");
        if (op == "store.blob.info") {
            const auto info = store->blob_info(workspace, text(request, "digest"));
            return Json{
                {"digest", info.digest}, {"byte_length", info.byte_length}, {"integrity", info.integrity}};
        }
        if (op == "store.blob.open") {
            gr::storage::BlobReadOptions options;
            options.chunk_bytes = count(request, "chunk_bytes", 65536);
            if (options.chunk_bytes == 0 || options.chunk_bytes > 1024 * 1024)
                throw gr::Error("invalid_request", "Reader chunk size must be within 1..1048576");
            auto state = std::make_shared<ReaderState>();
            {
                std::lock_guard lock(stores_mutex);
                if (readers.size() + pending_readers >= 1024)
                    throw gr::Error("resource_limit", "At most 1024 open blob readers");
                ++pending_readers;
            }
            PendingHandle reservation{pending_readers};
            state->reader = store->open_blob(workspace, text(request, "digest"), options);
            const auto info = state->reader->metadata();
            const auto handle = "reader:" + std::to_string(next_handle.fetch_add(1));
            {
                std::lock_guard lock(stores_mutex);
                readers.emplace(handle, state);
                reservation.published_under_lock();
            }
            return Json{{"handle", handle},
                        {"digest", info.digest},
                        {"byte_length", info.byte_length},
                        {"integrity", info.integrity},
                        {"chunk_bytes", options.chunk_bytes}};
        }
        if (op == "store.receipt") {
            auto receipt = store->receipt(workspace, text(request, "transaction_id"));
            if (!receipt)
                return Json(nullptr);
            const auto &e = receipt->entry;
            return Json{{"revision", e.revision},
                        {"parent", e.parent},
                        {"transaction_id", e.transaction_id},
                        {"actor", e.actor},
                        {"reason", e.reason},
                        {"request_digest", e.request_digest},
                        {"current_revision", receipt->current_revision}};
        }
        if (op == "store.create") {
            store->create_workspace(workspace);
            return Json{{"workspace", workspace}, {"revision", store->head(workspace)}};
        }
        if (op == "store.head")
            return Json{{"revision", store->head(workspace)}};
        if (op == "store.snapshot") {
            return runtime::TransactionCoordinator(store).snapshot(workspace,
                                                                   request.value("revision", std::string{}));
        }
        if (op == "store.history_snapshot")
            return runtime::TransactionCoordinator(store).history_snapshot(
                workspace, request.value("semantic_revision", std::string{}),
                request.value("physical_revision", std::string{}));
        if (op == "store.history_export_v1")
            return runtime::TransactionCoordinator(store).history_export_v1(
                workspace, request.value("physical_revision", std::string{}));
        if (op == "store.record")
            return record_json(
                store->record_version(workspace, text(request, "id"), text(request, "version")));
        if (op == "store.blob")
            return Json{{"digest", text(request, "digest")},
                        {"data_base64", encode(store->blob(workspace, text(request, "digest")))}};
        if (op == "store.history") {
            auto page = store->history(workspace, request.value("before", std::string{}),
                                       count(request, "limit", 100));
            Json entries = Json::array();
            for (const auto &e : page.entries)
                entries.push_back({{"revision", e.revision},
                                   {"parent", e.parent},
                                   {"transaction_id", e.transaction_id},
                                   {"actor", e.actor},
                                   {"reason", e.reason},
                                   {"request_digest", e.request_digest}});
            return Json{{"entries", entries}, {"next_cursor", page.next_cursor}};
        }
        if (op == "store.commit") {
            const auto outcome = runtime::TransactionCoordinator(store).commit(transaction_options(request),
                                                                               transaction_stage(request));
            if (!outcome.publication)
                throw Error("transaction_outcome", "Publication did not return an accepted receipt",
                            outcome.to_json());
            const auto &result = *outcome.publication;
            return Json{{"revision", result.revision},
                        {"parent", result.parent},
                        {"current_revision", result.current_revision},
                        {"replayed", result.replayed}};
        }
    }
    return std::nullopt;
}
} // namespace gr::binding
extern "C" int gr_blob_read(const char *handle, size_t handle_size, uint64_t offset, uint8_t *destination,
                            size_t capacity, char **response, size_t *response_size) {
    try {
        return gr::binding::respond(
            [&]() -> gr::Json {
                if (!handle || handle_size > 128 || (!destination && capacity))
                    throw gr::Error("invalid_request", "Invalid reader handle or destination");
                auto reader = gr::binding::reader_for(std::string(handle, handle_size));
                std::lock_guard lock(reader->mutex);
                auto read = reader->reader->read(offset, std::span<std::uint8_t>(destination, capacity),
                                                 reader->cancellation.get_token());
                return gr::Json{{"bytes_read", read}};
            },
            response, response_size);

    } catch (...) {
        if (response)
            *response = nullptr;
        if (response_size)
            *response_size = 0;
        return 2;
    }
}
