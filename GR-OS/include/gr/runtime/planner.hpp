#pragma once
#include "gr/runtime/catalog.hpp"
#include <map>
#include <mutex>
#include <variant>

namespace gr::runtime {
struct OutputRef {
    std::string node;
    std::size_t output = 0;
};
using Operand = std::variant<kernel::Value, OutputRef>;
struct Node {
    std::string id;
    std::string operation;
    std::vector<Operand> inputs;
    std::vector<std::string> after;
    Selection selection;
    std::size_t max_output_bytes = 64 * 1024 * 1024;
    std::size_t max_input_bytes = 64 * 1024 * 1024;
};
struct Graph {
    std::vector<Node> nodes;
    std::vector<OutputRef> outputs;
    // Optional bounded caller context pins provenance/recipe identity without
    // adding execution nodes. Empty preserves all v0.3/v0.4 plan identities.
    Json context = Json::object();
};
struct PlannerLimits {
    std::size_t max_nodes = 4096;
    std::size_t max_edges = 65536;
    std::size_t max_literal_bytes = 64 * 1024 * 1024;
    std::size_t max_plan_bytes = 64 * 1024 * 1024;
    std::size_t cache_entries = 16;
    std::size_t cache_payload_bytes = 64 * 1024 * 1024;
};
struct IndexedOutput {
    std::size_t node = 0;
    std::size_t output = 0;
};
struct PreparedNode {
    std::string id;
    std::string operation;
    std::vector<std::size_t> dependencies;
    std::size_t cpu_slots = 1;
    std::size_t max_output_bytes = 0;
    std::size_t max_input_bytes = 0;
    Json selection;
};
class PreparedPlan {
  public:
    const std::string &id() const noexcept;
    const std::vector<PreparedNode> &nodes() const noexcept;
    const std::vector<IndexedOutput> &outputs() const noexcept;
    std::size_t literal_bytes() const noexcept;
    Json describe() const;
    kernel::Invocation
    assemble_inputs(std::size_t node,
                    const std::vector<std::shared_ptr<const kernel::Invocation>> &results) const;
    kernel::Invocation execute_node(std::size_t node, const kernel::Invocation &, std::stop_token,
                                    std::size_t output_limit) const;

  private:
    friend class Planner;
    struct BoundNode {
        std::vector<std::variant<kernel::Value, IndexedOutput>> inputs;
        std::shared_ptr<const execution::PreparedExecutor> executable;
    };
    std::string id_;
    std::vector<PreparedNode> nodes_;
    std::vector<BoundNode> bound_;
    std::vector<IndexedOutput> outputs_;
    std::size_t literal_bytes_ = 0;
    std::size_t cache_charge_ = 0;
    Json context_ = Json::object();
};
// Compiles the complete graph, including nodes not selected as outputs. There
// are no implicit coercions, effectful callbacks or algebraic rewrites.
class Planner {
  public:
    explicit Planner(std::shared_ptr<const Catalog>, PlannerLimits = {});
    std::shared_ptr<const PreparedPlan> compile(const Graph &) const;
    Json cache_info() const;

  private:
    std::shared_ptr<const Catalog> catalog_;
    PlannerLimits limits_;
    mutable std::mutex cache_mutex_;
    mutable std::map<std::string, std::shared_ptr<const PreparedPlan>> cache_;
    mutable std::vector<std::string> cache_order_;
    mutable std::size_t cache_bytes_ = 0;
};
} // namespace gr::runtime
