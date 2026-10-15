#include "gr/storage/memory_store.hpp"
#include "internal.hpp"
#include <algorithm>
#include <map>
#include <mutex>

namespace gr::storage {
using namespace detail;
namespace {
class MemoryBlobReader final : public BlobReader {
  public:
    MemoryBlobReader(std::shared_ptr<const std::vector<std::uint8_t>> bytes, std::string digest,
                     BlobReadOptions options)
        : bytes_(std::move(bytes)), info_{std::move(digest), bytes_->size(), "unverified"},
          options_(options) {
        validate_options(options_);
        verify_reader(*this, options_.stop);
        info_.integrity = "sha256-verified";
    }
    const BlobMetadata &metadata() const noexcept override { return info_; }
    std::size_t read(std::uint64_t offset, std::span<std::uint8_t> destination,
                     std::stop_token stop) override {
        check_cancelled(stop);
        check_cancelled(options_.stop);
        const auto n = read_size(info_, offset, destination.size(), options_.chunk_bytes);
        if (n)
            std::copy_n(bytes_->data() + static_cast<std::size_t>(offset), n, destination.data());
        return n;
    }

  private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    BlobMetadata info_;
    BlobReadOptions options_;
};
} // namespace
struct MemoryStore::Impl {
    struct Workspace {
        std::string head = "0";
        std::map<std::string, std::shared_ptr<const Snapshot>> snapshots;
        std::map<std::pair<std::string, std::string>, std::shared_ptr<const StoredRecord>> versions;
        std::map<std::string, std::shared_ptr<const std::vector<std::uint8_t>>> blobs;
        std::vector<HistoryEntry> history;
        std::map<std::string, HistoryEntry> transactions;
    };
    StorageLimits limits;
    std::mutex mutex;
    std::map<std::string, Workspace> workspaces;
    Workspace &get(const std::string &id) {
        identifier(id, "Workspace");
        const auto it = workspaces.find(id);
        if (it == workspaces.end())
            throw Error("not_found", "Workspace does not exist");
        return it->second;
    }
};
MemoryStore::MemoryStore(StorageLimits limits) : impl_(std::make_unique<Impl>()) {
    validate_limits(limits);
    impl_->limits = limits;
}
MemoryStore::~MemoryStore() = default;
StorageCapabilities MemoryStore::describe() const {
    return capabilities_for("memory", false, false, impl_->limits);
}
void MemoryStore::create_workspace(const std::string &workspace) {
    identifier(workspace, "Workspace");
    std::lock_guard lock(impl_->mutex);
    require(!impl_->workspaces.contains(workspace), "conflict", "Workspace already exists");
    impl_->workspaces.emplace(workspace, Impl::Workspace{});
}
std::string MemoryStore::head(const std::string &workspace) {
    std::lock_guard lock(impl_->mutex);
    return impl_->get(workspace).head;
}
Snapshot MemoryStore::snapshot(const std::string &workspace, const std::string &revision) {
    std::lock_guard lock(impl_->mutex);
    auto &w = impl_->get(workspace);
    auto selected = revision.empty() ? w.head : revision;
    if (selected == "0")
        return {workspace, "0", {}};
    auto it = w.snapshots.find(selected);
    require(it != w.snapshots.end(), "not_found", "Revision does not belong to this workspace");
    verify_snapshot(*it->second, impl_->limits);
    return *it->second;
}
StoredRecord MemoryStore::record_version(const std::string &workspace, const std::string &id,
                                         const std::string &version) {
    identifier(id, "Record id");
    digest_id(version);
    std::lock_guard lock(impl_->mutex);
    auto &w = impl_->get(workspace);
    auto it = w.versions.find({id, version});
    require(it != w.versions.end(), "not_found",
            "Record version does not belong to this workspace and identity");
    verify_record(*it->second, impl_->limits);
    return *it->second;
}
Result MemoryStore::commit(const Commit &request) {
    const auto prepared = prepare(request, impl_->limits);
    std::lock_guard lock(impl_->mutex);
    auto &current = impl_->get(request.workspace);
    if (auto it = current.transactions.find(request.transaction_id); it != current.transactions.end()) {
        verify_entry(request.workspace, it->second);
        require(it->second.request_digest == prepared.request_digest && it->second.actor == request.actor,
                "transaction_conflict", "Transaction id was already used by a different request");
        return {it->second.revision, it->second.parent, current.head, true};
    }
    require(current.head == request.expected_revision, "conflict", "Expected revision is stale");
    auto candidate = current; // Shares immutable old data; publish only by final swap.
    std::map<std::string, StoredRecord> records;
    if (current.head != "0")
        for (const auto &r : current.snapshots.at(current.head)->records)
            records.emplace(r.id, r);
    for (const auto &r : prepared.records)
        records.insert_or_assign(r.id, r);
    Snapshot next{request.workspace, "", {}};
    for (const auto &[id, r] : records) {
        (void)id;
        next.records.push_back(r);
    }
    verify_snapshot(next, impl_->limits);
    for (const auto &b : request.blobs) {
        auto it = candidate.blobs.find(b.digest);
        if (it != candidate.blobs.end())
            require(*it->second == b.bytes, "integrity_error", "Existing blob content differs");
        else
            candidate.blobs.emplace(b.digest, std::make_shared<const std::vector<std::uint8_t>>(b.bytes));
    }
    std::size_t total = 0;
    for (const auto &[digest, bytes] : candidate.blobs) {
        (void)digest;
        add_size(total, bytes->size(), impl_->limits.blob_bytes_per_workspace,
                 "Retained blob budget exceeded");
    }
    for (const auto &digest : request.required_blob_digests) {
        auto it = candidate.blobs.find(digest);
        require(it != candidate.blobs.end(), "not_found",
                "Required blob has not been admitted in this workspace");
        require(bytes_digest(*it->second) == digest, "integrity_error",
                "Required blob failed integrity verification");
    }
    auto entry = entry_for(request, prepared);
    next.revision = entry.revision;
    for (const auto &r : prepared.records)
        candidate.versions.emplace(std::make_pair(r.id, r.version), std::make_shared<const StoredRecord>(r));
    candidate.snapshots.emplace(entry.revision, std::make_shared<const Snapshot>(std::move(next)));
    candidate.history.push_back(entry);
    candidate.transactions.emplace(request.transaction_id, entry);
    candidate.head = entry.revision;
    std::swap(current, candidate);
    return {entry.revision, entry.parent, entry.revision, false};
}
HistoryPage MemoryStore::history(const std::string &workspace, const std::string &before, std::size_t limit) {
    history_limit(limit);
    std::lock_guard lock(impl_->mutex);
    auto &w = impl_->get(workspace);
    std::size_t end = w.history.size();
    if (!before.empty()) {
        auto it = std::find_if(w.history.begin(), w.history.end(),
                               [&](const auto &e) { return e.revision == before; });
        require(it != w.history.end(), "not_found", "History cursor does not belong to this workspace");
        end = static_cast<std::size_t>(it - w.history.begin());
    }
    HistoryPage page;
    while (end > 0 && page.entries.size() < limit) {
        const auto &entry = w.history[--end];
        verify_entry(workspace, entry);
        page.entries.push_back(entry);
    }
    if (end > 0)
        page.next_cursor = page.entries.back().revision;
    return page;
}

std::optional<TransactionReceipt> MemoryStore::receipt(const std::string &workspace,
                                                       const std::string &transaction_id) {
    identifier(transaction_id, "Transaction id");
    std::lock_guard lock(impl_->mutex);
    auto &w = impl_->get(workspace);
    const auto it = w.transactions.find(transaction_id);
    if (it == w.transactions.end())
        return std::nullopt;
    verify_entry(workspace, it->second);
    return TransactionReceipt{it->second, w.head};
}
BlobMetadata MemoryStore::blob_info(const std::string &workspace, const std::string &digest) {
    digest_id(digest);
    std::lock_guard lock(impl_->mutex);
    auto &w = impl_->get(workspace);
    const auto it = w.blobs.find(digest);
    require(it != w.blobs.end(), "not_found", "Blob does not belong to this workspace");
    require(it->second->size() <= impl_->limits.blob_bytes, "storage_limit",
            "Stored blob exceeds reader byte limit");
    return {digest, it->second->size(), "unverified"};
}
std::unique_ptr<BlobReader> MemoryStore::open_blob(const std::string &workspace, const std::string &digest,
                                                   BlobReadOptions options) {
    digest_id(digest);
    validate_options(options);
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
    {
        std::lock_guard lock(impl_->mutex);
        auto &w = impl_->get(workspace);
        const auto it = w.blobs.find(digest);
        require(it != w.blobs.end(), "not_found", "Blob does not belong to this workspace");
        require(it->second->size() <= impl_->limits.blob_bytes, "storage_limit",
                "Stored blob exceeds reader byte limit");
        bytes = it->second;
    }
    return std::make_unique<MemoryBlobReader>(std::move(bytes), digest, options);
}

} // namespace gr::storage
