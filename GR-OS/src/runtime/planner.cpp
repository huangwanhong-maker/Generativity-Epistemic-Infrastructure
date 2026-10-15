#include "gr/runtime/planner.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace gr::runtime {
namespace {
void need(bool condition, const char *code, const char *message) {
    if (!condition)
        throw Error(code, message);
}
void charge(std::size_t amount, std::size_t &total, std::size_t limit) {
    need(total <= limit && amount <= limit - total, "plan_limit",
         "Plan exceeds its configured payload/structure budget.");
    total += amount;
}
void identifier(const std::string &id) {
    need(!id.empty() && id.size() <= 256 && id.find('\0') == std::string::npos, "node_id",
         "Node identity must be a bounded nonempty string.");
    canonical(Json(id));
}
Json reference_json(const OutputRef &reference) {
    return {{"node", reference.node}, {"output", reference.output}};
}
Json selection_json(const Selection &selection) {
    return {{"implementation", selection.implementation},
            {"backend", selection.backend},
            {"device", selection.device},
            {"prefer_implementations", selection.prefer_implementations}};
}
Json literal_summary(const kernel::Value &value) {
    // Preserve v0.3 JSON/octet graph hashes; buffer identity includes physical
    // descriptors and immutable backing contents, never process addresses.
    const auto bytes = value.representation() == kernel::Representation::Buffers
                           ? value.as_buffers().content_digest()
                           : (value.representation() == kernel::Representation::Bytes
                                  ? sha256(std::span<const std::uint8_t>(value.as_bytes()))
                                  : sha256(canonical(value.as_json())));
    return {{"type", value.type},
            {"representation", kernel::to_string(value.representation())},
            {"payload_sha256", bytes},
            {"payload_bytes", value.byte_size()}};
}
} // namespace
const std::string &PreparedPlan::id() const noexcept {
    return id_;
}
const std::vector<PreparedNode> &PreparedPlan::nodes() const noexcept {
    return nodes_;
}
const std::vector<IndexedOutput> &PreparedPlan::outputs() const noexcept {
    return outputs_;
}
std::size_t PreparedPlan::literal_bytes() const noexcept {
    return literal_bytes_;
}
Json PreparedPlan::describe() const {
    Json nodes = Json::array(), outputs = Json::array();
    for (const auto &node : nodes_)
        nodes.push_back({{"id", node.id},
                         {"operation", node.operation},
                         {"dependencies", node.dependencies},
                         {"cpu_slots", node.cpu_slots},
                         {"max_input_bytes", node.max_input_bytes},
                         {"max_output_bytes", node.max_output_bytes},
                         {"selection", node.selection}});
    for (const auto &output : outputs_)
        outputs.push_back({{"node", nodes_.at(output.node).id}, {"output", output.output}});
    Json result = {{"format", "gr.runtime-plan/1"},
                   {"id", id_},
                   {"nodes", nodes},
                   {"outputs", outputs},
                   {"literal_bytes", literal_bytes_},
                   {"accounted_plan_bytes", cache_charge_},
                   {"effects", "pure"},
                   {"determinism", "exact results; scheduler trace ordering is separate"},
                   {"transformations", Json::array()},
                   {"cache_scope", "this sealed catalog and planner"}};
    if (!context_.empty())
        result["context"] = context_;
    return result;
}
kernel::Invocation
PreparedPlan::assemble_inputs(std::size_t node,
                              const std::vector<std::shared_ptr<const kernel::Invocation>> &results) const {
    need(node < bound_.size(), "plan_node", "Unknown prepared node index.");
    kernel::Invocation inputs;
    inputs.reserve(bound_[node].inputs.size());
    for (const auto &input : bound_[node].inputs) {
        if (const auto *value = std::get_if<kernel::Value>(&input))
            inputs.push_back(*value);
        else {
            const auto &reference = std::get<IndexedOutput>(input);
            need(reference.node < results.size() && bool(results[reference.node]) &&
                     reference.output < results[reference.node]->size(),
                 "dependency_unavailable", "Prepared node dependency output is unavailable.");
            inputs.push_back(results[reference.node]->at(reference.output));
        }
    }
    return inputs;
}
kernel::Invocation PreparedPlan::execute_node(std::size_t node, const kernel::Invocation &inputs,
                                              std::stop_token stop, std::size_t output_limit) const {
    need(node < nodes_.size(), "plan_node", "Unknown prepared node index.");
    return bound_[node].executable->invoke(inputs, stop, nodes_[node].max_input_bytes,
                                           std::min(output_limit, nodes_[node].max_output_bytes));
}
Planner::Planner(std::shared_ptr<const Catalog> catalog, PlannerLimits limits)
    : catalog_(std::move(catalog)), limits_(limits) {
    need(bool(catalog_) && catalog_->sealed(), "catalog_unsealed", "Planner needs an owned sealed catalog.");
    need(limits_.max_nodes <= 65536 && limits_.max_edges <= 1048576 && limits_.cache_entries <= 1024,
         "plan_limit", "Planner structural/cache limits exceed this IR edition's supported range.");
}
std::shared_ptr<const PreparedPlan> Planner::compile(const Graph &graph) const {
    need(graph.nodes.size() <= limits_.max_nodes && graph.outputs.size() <= limits_.max_edges, "plan_limit",
         "Graph exceeds node/output limits.");
    std::string logical = "gr.runtime-graph/1\n";
    std::size_t literals = 0, structural_edges = graph.outputs.size(), accounted = 0;
    need(graph.context.is_object(), "plan_context", "Graph context must be an object.");
    if (!graph.context.empty()) {
        const auto context = canonical(graph.context);
        charge(context.size(), accounted, limits_.max_plan_bytes);
        logical += "context\n" + sha256(context) + "\n";
    }
    std::map<std::string, std::size_t> positions;
    std::vector<kernel::OperationDescriptor> operations;
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        const auto &node = graph.nodes[index];
        identifier(node.id);
        need(positions.emplace(node.id, index).second, "duplicate_node", "Graph repeats a node identity.");
        need(node.inputs.size() <= 64 && node.after.size() <= limits_.max_edges, "plan_limit",
             "Node exceeds argument/dependency bounds.");
        for (const auto *choice :
             {&node.selection.implementation, &node.selection.backend, &node.selection.device})
            need(choice->size() <= 256 && choice->find('\0') == std::string::npos, "selection",
                 "Selection identity is invalid.");
        need(node.selection.prefer_implementations.size() <= 256, "selection",
             "Too many implementation preferences.");
        for (const auto &preferred : node.selection.prefer_implementations)
            kernel::validate_qualified_id(preferred);
        auto operation = catalog_->operation(node.operation);
        need(operation.effect == kernel::Effect::Pure, "unsupported_effect", "Graph nodes must be pure.");
        need(operation.determinism == kernel::Determinism::Exact, "unsupported_determinism",
             "Graph requires exact results.");
        need(node.inputs.size() == operation.inputs.size(), "input_type",
             "Node argument count differs from its semantic signature.");
        Json inputs = Json::array();
        for (std::size_t i = 0; i < node.inputs.size(); ++i) {
            if (const auto *value = std::get_if<kernel::Value>(&node.inputs[i])) {
                need(value->type == operation.inputs[i], "input_type",
                     "Literal type differs from its semantic signature.");
                charge(value->byte_size(), literals, limits_.max_literal_bytes);
                catalog_->validate(*value);
                inputs.push_back({{"literal", literal_summary(*value)}});
            } else {
                const auto &reference = std::get<OutputRef>(node.inputs[i]);
                identifier(reference.node);
                inputs.push_back({{"ref", reference_json(reference)}});
                charge(1, structural_edges, limits_.max_edges);
            }
        }
        for (const auto &dependency : node.after)
            identifier(dependency);
        charge(node.after.size(), structural_edges, limits_.max_edges);
        const auto summary = canonical(Json{{"id", node.id},
                                            {"operation", node.operation},
                                            {"inputs", inputs},
                                            {"after", node.after},
                                            {"selection", selection_json(node.selection)},
                                            {"max_input_bytes", node.max_input_bytes},
                                            {"max_output_bytes", node.max_output_bytes}});
        charge(summary.size(), accounted, limits_.max_plan_bytes);
        logical += sha256(summary) + "\n";
        operations.push_back(std::move(operation));
    }
    charge(literals, accounted, limits_.max_plan_bytes);
    logical += "outputs\n";
    for (const auto &output : graph.outputs) {
        identifier(output.node);
        const auto summary = canonical(reference_json(output));
        charge(summary.size(), accounted, limits_.max_plan_bytes);
        logical += sha256(summary) + "\n";
    }
    const auto key = sha256(logical);
    {
        std::lock_guard lock(cache_mutex_);
        const auto found = cache_.find(key);
        if (found != cache_.end())
            return found->second;
    }
    auto plan = std::make_shared<PreparedPlan>();
    plan->context_ = graph.context;
    plan->literal_bytes_ = literals;
    plan->nodes_.resize(graph.nodes.size());
    plan->bound_.resize(graph.nodes.size());
    auto resolve = [&](const OutputRef &reference) {
        const auto found = positions.find(reference.node);
        need(found != positions.end(), "graph_reference", "Graph references an unknown node.");
        need(reference.output < operations[found->second].outputs.size(), "graph_reference",
             "Graph references an unavailable output slot.");
        return IndexedOutput{found->second, reference.output};
    };
    std::vector<std::vector<std::size_t>> outgoing(graph.nodes.size());
    std::vector<std::size_t> degree(graph.nodes.size());
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        const auto &node = graph.nodes[index];
        std::set<std::size_t> dependencies;
        for (std::size_t i = 0; i < node.inputs.size(); ++i) {
            const auto &input = node.inputs[i];
            if (const auto *value = std::get_if<kernel::Value>(&input))
                plan->bound_[index].inputs.push_back(*value);
            else {
                const auto reference = resolve(std::get<OutputRef>(input));
                need(operations[reference.node].outputs[reference.output] == operations[index].inputs[i],
                     "input_type", "Dependency result type differs from the consumer signature.");
                dependencies.insert(reference.node);
                plan->bound_[index].inputs.push_back(reference);
            }
        }
        for (const auto &dependency : node.after) {
            const auto found = positions.find(dependency);
            need(found != positions.end(), "graph_reference", "Control dependency names an unknown node.");
            dependencies.insert(found->second);
        }
        need(!dependencies.contains(index), "graph_cycle", "Node depends on itself.");
        for (const auto predecessor : dependencies)
            outgoing[predecessor].push_back(index);
        degree[index] = dependencies.size();
        plan->nodes_[index] = {node.id,
                               node.operation,
                               {dependencies.begin(), dependencies.end()},
                               1,
                               node.max_output_bytes,
                               node.max_input_bytes,
                               Json::object()};
    }
    for (const auto &output : graph.outputs)
        plan->outputs_.push_back(resolve(output));
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < degree.size(); ++i)
        if (!degree[i])
            ready.push_back(i);
    for (std::size_t i = 0; i < ready.size(); ++i)
        for (const auto next : outgoing[ready[i]])
            if (--degree[next] == 0)
                ready.push_back(next);
    need(ready.size() == graph.nodes.size(), "graph_cycle", "Graph contains a dependency cycle.");
    std::string physical = "gr.runtime-plan/1\n" + key + "\n";
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        auto selected = catalog_->prepare(graph.nodes[index].operation, graph.nodes[index].selection);
        auto description = canonical(selected.selection);
        charge(description.size(), accounted, limits_.max_plan_bytes);
        plan->nodes_[index].cpu_slots = selected.executable->cpu_slots();
        plan->nodes_[index].selection = std::move(selected.selection);
        plan->bound_[index].executable = std::move(selected.executable);
        physical += operations[index].fingerprint() + "\n" + sha256(description) + "\n";
    }
    plan->id_ = "sha256:" + sha256(physical);
    plan->cache_charge_ = accounted;
    if (limits_.cache_entries && accounted <= limits_.cache_payload_bytes) {
        std::lock_guard lock(cache_mutex_);
        if (const auto found = cache_.find(key); found != cache_.end())
            return found->second;
        while (!cache_order_.empty() && (cache_.size() >= limits_.cache_entries ||
                                         accounted > limits_.cache_payload_bytes - cache_bytes_)) {
            cache_bytes_ -= cache_.at(cache_order_.front())->cache_charge_;
            cache_.erase(cache_order_.front());
            cache_order_.erase(cache_order_.begin());
        }
        cache_.emplace(key, plan);
        cache_order_.push_back(key);
        cache_bytes_ += accounted;
    }
    return plan;
}
Json Planner::cache_info() const {
    std::lock_guard lock(cache_mutex_);
    return {{"entries", cache_.size()},
            {"accounted_payload_bytes", cache_bytes_},
            {"max_entries", limits_.cache_entries},
            {"max_payload_bytes", limits_.cache_payload_bytes},
            {"eviction", "FIFO"}};
}
} // namespace gr::runtime
