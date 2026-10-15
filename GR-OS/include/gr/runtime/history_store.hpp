#pragma once
#include "gr/kernel/compact_history.hpp"
#include "gr/storage/contracts.hpp"
#include <map>
#include <optional>

namespace gr::runtime {
inline constexpr const char *runtime_history_id = "gr:runtime:history";
inline constexpr const char *runtime_history_type_v1 = "gr.runtime.history-manifest@1";
inline constexpr const char *runtime_history_type_v2 = "gr.runtime.history-manifest@2";
using HistoryPolicy = history::CompactPolicy;
struct HistoryState {
    Json manifest;
    std::string head;
    Json snapshot;
    Json diagnostics = Json::object();
};
struct HistoryWrite {
    Json manifest; // Complete reserved record wrapper; no write has occurred yet.
    Json selected_snapshot;
    std::vector<storage::Blob> blobs;
    std::vector<std::string> required_blob_digests;
    // Caller verifies these against staged bytes or existing workspace objects.
    std::map<std::string, std::uint64_t> artifact_lengths;
    Json diagnostics = Json::object();
};
bool history_manifest_type_supported(const std::string &);
void register_history_profiles(core::TypeRegistry &); // Explicit collision rejection.
Json history_manifest_info(const Json &wrapper);
class HistoryStore {
  public:
    HistoryStore(std::shared_ptr<storage::IStore>, std::string workspace,
                 std::shared_ptr<const core::TypeRegistry> = {});
    HistoryState read(const Json &manifest, const std::string &selected_revision = "") const;
    HistoryWrite append(const std::optional<Json> &manifest, const Json &candidate, const Json &attribution,
                        HistoryPolicy = {}, bool upgrade = false) const;
    // Explicit v1 imports use legacy encoding unless upgrading or extending an
    // already compact manifest. Existing retained nodes cannot be dropped.
    HistoryWrite import_v1(const std::optional<Json> &manifest, const Json &bundle, const std::string &head,
                           const Json &attribution, HistoryPolicy = {}, bool upgrade = false) const;
    Json export_v1(const Json &manifest) const;
    HistoryWrite upgrade_v1(const Json &manifest, const Json &attribution, HistoryPolicy = {}) const;

  private:
    std::shared_ptr<storage::IStore> store_;
    std::string workspace_;
    std::shared_ptr<const core::TypeRegistry> registry_;
};
} // namespace gr::runtime
