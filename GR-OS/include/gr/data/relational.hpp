#pragma once
#include "gr/data/language.hpp"

namespace gr::data::relational {
inline constexpr const char *records_family = "gr.data.qualified-records@1";
inline constexpr const char *incidence_family = "gr.data.qualified-incidence@1";
inline constexpr const char *counts_family = "gr.data.incidence-counts@1";
inline constexpr const char *representation = "gr.data.qualified-json-bytes@1";
inline constexpr const char *project_operation = "gr.data.incidence@1";
inline constexpr const char *roles_operation = "gr.data.select-incidence-roles@1";
inline constexpr const char *count_operation = "gr.data.count-incidences@1";
inline constexpr std::size_t maximum_payload_bytes = 64 * 1024 * 1024;
inline constexpr std::size_t maximum_incidences = 100000;

struct HistoricalSelection {
    std::string workspace;
    std::string semantic_revision; // Empty selects the pinned physical view's semantic head.
    std::string physical_revision; // Empty pins the physical head once, during this eager read.
};
Type records_type();
Type incidence_type();
Type counts_type();
struct CacheOptions {
    std::size_t max_entries = 64;
    std::size_t max_bytes = 16 * 1024 * 1024;
    bool profile_timing = false;
};
// An explicit owning inspection lease. Const references remain valid while any
// copy of this view lives, even after cache clear or Session/library destruction.
// Retains the immutable payload and decoded state; this is outside cache quotas.
class QualifiedView {
  public:
    QualifiedView() = default;
    const DataValue &value() const;
    const Json &source() const;
    const Json &derivation() const; // Qualified container excluding its source.
    explicit operator bool() const noexcept;

  private:
    struct Impl;
    explicit QualifiedView(std::shared_ptr<const Impl>);
    std::shared_ptr<const Impl> impl_;
    friend class Session;
};
// One installation's pinned record profiles and bounded immutable parsed-state
// memo. Wire-byte charges bound retention, not JSON allocator overhead or RSS.
class Session {
  public:
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Json inspect(const DataValue &) const;
    QualifiedView view(const DataValue &) const;
    Json statistics() const;
    void clear_cache();

  private:
    struct Impl;
    explicit Session(std::shared_ptr<Impl>);
    std::shared_ptr<Impl> impl_;
    friend std::shared_ptr<Session> install(Library &, std::shared_ptr<const core::TypeRegistry>,
                                            CacheOptions);
};
// Copies the registry; later caller changes cannot reinterpret this installation.
// Existing callers may ignore the returned diagnostic/inspection session.
std::shared_ptr<Session> install(Library &, std::shared_ptr<const core::TypeRegistry> = {},
                                 CacheOptions = {});
// Eager, effectful read outside expressions. Subsequent operators are pure and
// never consult the coordinator or a moving storage head. Full selected wrappers,
// direct archive, history attribution and physical/semantic pins are retained.
DataValue select_history(runtime::TransactionCoordinator &, const HistoricalSelection &);
// Decode and validate the complete qualified value, including derived incidence
// selections/counts. The registry must match any custom profiles used by install.
Json inspect(const DataValue &, const core::TypeRegistry & = core::default_types());

// incidence attributes: {} or {"temporal": {"order_ids": [...], "after": [...]}}.
// select-incidence-roles attributes: {"roles": ["role", ...]} (exact role strings).
// count-incidences attributes: {}. Counts group scope/id/resolved version/role/
// temporal class and retain occurrence keys. No epistemic or causal inference.
} // namespace gr::data::relational
