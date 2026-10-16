#pragma once
#include <gr/data/relational.hpp>
#include <optional>
#include <span>
#include <stop_token>

namespace gr_relational_analysis {
inline constexpr const char *snapshot_format = "gr.analysis.incidence-snapshot/1";
inline constexpr const char *summary_operation = "gr.analysis.incidence-summary@1";
inline constexpr const char *summary_family = "gr.analysis.incidence-summary@1";
inline constexpr const char *summary_representation = "gr.analysis.summary-json-bytes@1";
struct Limits {
    std::size_t max_vertices = 110000;
    std::size_t max_occurrences = 100000;
    std::size_t max_snapshot_bytes = 64 * 1024 * 1024;
};
enum class VertexKind { participant_owner, participant_target };
std::string to_string(VertexKind);
struct Vertex {
    std::string key;
    VertexKind kind = VertexKind::participant_owner;
    std::string scope, id;
    std::optional<std::string> version;
};
struct Occurrence {
    std::string key;
    std::size_t owner = 0, target = 0, ordinal = 0;
    std::string relation, relation_version, role, temporal_class;
    gr::Json participant;
};
class NetworkView;
// Immutable shared indexed projection. Portable import checks structure and
// identity, never the omitted qualified source's historical derivation.
class NetworkSnapshot {
  public:
    NetworkSnapshot() = default;
    const std::vector<Vertex> &vertices() const;
    const std::vector<Occurrence> &occurrences() const;
    const gr::Json &context() const;
    const gr::Json &identity() const;
    const std::string &id() const;
    Limits resource_limits() const;
    std::string validation_basis() const; // source_derived | structural_only
    gr::Json to_json() const;
    std::string encode() const;
    static NetworkSnapshot decode(std::span<const std::uint8_t>, Limits = {});
    static NetworkSnapshot from_json(const gr::Json &, Limits = {});
    explicit operator bool() const noexcept;

  private:
    struct Impl;
    explicit NetworkSnapshot(std::shared_ptr<const Impl>);
    std::shared_ptr<const Impl> impl_;
    friend class NetworkView;
    friend NetworkView analyze(const gr::data::DataValue &, std::shared_ptr<gr::data::relational::Session>,
                               Limits, std::stop_token);
};
class NetworkView {
  public:
    NetworkView() = default;
    const gr::data::relational::QualifiedView &source() const;
    NetworkSnapshot snapshot() const; // Shares topology; no encoding or deep copy.
    const gr::Json &identity() const;
    explicit operator bool() const noexcept;

  private:
    struct Impl;
    explicit NetworkView(std::shared_ptr<const Impl>);
    std::shared_ptr<const Impl> impl_;
    friend NetworkView analyze(const gr::data::DataValue &, std::shared_ptr<gr::data::relational::Session>,
                               Limits, std::stop_token);
};
NetworkView analyze(const gr::data::DataValue &, std::shared_ptr<gr::data::relational::Session>, Limits = {},
                    std::stop_token = {});
gr::Json summarize(const NetworkSnapshot &, std::stop_token = {});
gr::Json summarize(const NetworkView &, std::stop_token = {});
// Same workspace, role selection and temporal query required. Record ID alignment
// describes record versions, never inferred real-world identity or occurrence time.
gr::Json compare(const NetworkView &left, const NetworkView &right, std::stop_token = {});

// Installs a pure exact summary operator over existing qualified incidence values.
// Install relational first and pass its Session, then seal the shared Library.
// Portable summary has source carrier digest/selection pins; Program materialize
// adds full input computational lineage. Direct summarize includes snapshot ID.
void install(gr::data::Library &, std::shared_ptr<gr::data::relational::Session>, Limits = {});
gr::data::ValueSpec summary_spec();
gr::Json inspect_summary(const gr::data::DataValue &, Limits = {});
} // namespace gr_relational_analysis
