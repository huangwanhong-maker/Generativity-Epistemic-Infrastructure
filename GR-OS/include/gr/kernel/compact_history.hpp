#pragma once
#include "gr/kernel/history.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>

namespace gr::history {
struct NodeRef {
    std::string revision, object_digest;
    bool operator==(const NodeRef &) const = default;
    Json to_json() const;
    static NodeRef from_json(const Json &);
};
struct HistoryObject {
    std::string digest;
    std::vector<std::uint8_t> bytes;
};
struct CompactPolicy {
    std::size_t checkpoint_interval = 16;
    std::size_t max_delta_depth = 32;
};
struct CompactLimits {
    std::size_t object_bytes = 16 * 1024 * 1024;
    std::size_t read_bytes = 64 * 1024 * 1024;
    std::size_t read_objects = 8192;
    std::size_t nodes = 1024;
    std::size_t delta_depth = 64;
    std::size_t reconstructed_bytes = 64 * 1024 * 1024;
};
// The loader is synchronous and scoped to one read/write operation. It must honor
// the requested byte bound before copying. The reader independently checks hashes.
using ObjectLoader = std::function<std::vector<std::uint8_t>(const std::string &, std::size_t)>;
struct EncodedNode {
    NodeRef node;
    std::vector<HistoryObject> objects; // State payload, then small node header.
    std::size_t delta_depth = 0;
};
struct CompactReadStats {
    std::size_t objects = 0, bytes = 0, reconstructed_nodes = 0;
    Json to_json() const;
};
class CompactReader {
  public:
    explicit CompactReader(ObjectLoader, std::shared_ptr<const core::TypeRegistry> = {}, CompactLimits = {});
    ~CompactReader();
    CompactReader(const CompactReader &) = delete;
    CompactReader &operator=(const CompactReader &) = delete;
    Json header(const NodeRef &);
    Json reconstruct(const NodeRef &);
    bool ancestor(const NodeRef &base, const NodeRef &tip);
    Json export_v1(const std::vector<NodeRef> &); // Complete explicit inventory, not only selected ancestry.
    CompactReadStats stats() const;
    std::vector<std::string> loaded_digests() const;
    const core::TypeRegistry &registry() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Encodes the exact supplied v1 semantic node; representation never changes its identity.
// The optional base is a physical compression choice, not an added semantic parent.
EncodedNode encode_node(const Json &v1_node, const std::vector<NodeRef> &parents, CompactReader &,
                        std::optional<NodeRef> base = {}, CompactPolicy = {});
// Shared v1 identity primitives, also used by the legacy bundle implementation.
Json normalized_snapshot(const Json &snapshot);
std::string snapshot_digest(const Json &normalized);
Json make_node(const Json &snapshot, const Json &parents, const Json &attribution,
               const core::TypeRegistry &registry = core::default_types());
HistoryObject encode_object(const Json &, std::size_t max_bytes = 16 * 1024 * 1024);
Json decode_object(std::span<const std::uint8_t>, std::size_t max_bytes = 16 * 1024 * 1024);
} // namespace gr::history
