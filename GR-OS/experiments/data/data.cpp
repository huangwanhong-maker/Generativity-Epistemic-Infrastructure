#include "gr/experimental/data.hpp"
#include "gr/kernel/numerical.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <limits>

namespace gr::experimental::data {
namespace {
constexpr const char *column_type = "gr.experimental.int64-column@1";
constexpr const char *scalar_type = "gr.experimental.int64-scalar@1";
constexpr std::size_t maximum_elements = 1024 * 1024;
memory::ElementDescriptor element() {
    return {"gr.int64@1", 8, 8, memory::ByteOrder::Little, Json::object()};
}
void stopped(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Experimental operation cancelled");
}
std::int64_t read(std::span<const std::uint8_t> bytes) {
    std::uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i)
        bits |= std::uint64_t(bytes[i]) << (8 * i);
    return std::bit_cast<std::int64_t>(bits);
}
void write(std::span<std::uint8_t> bytes, std::int64_t value) {
    auto bits = std::bit_cast<std::uint64_t>(value);
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = static_cast<std::uint8_t>(bits >> (8 * i));
}
const memory::BufferView &column(const kernel::Value &v) {
    const auto &set = v.as_buffers();
    if (set.buffers().size() != 1 || !set.metadata().empty())
        throw Error("invalid_value", "Expected one unannotated values buffer");
    const auto &view = set.at("values");
    if (view.shape().size() != 1 || view.element() != element() || view.elements() > maximum_elements)
        throw Error("invalid_value", "Expected bounded one-dimensional little-endian int64 values");
    return view;
}
kernel::Value wrap(memory::BufferView v, bool scalar = false) {
    return kernel::Value::buffers(scalar ? scalar_type : column_type,
                                  memory::BufferSet({{"values", std::move(v)}}));
}
kernel::Value built(std::vector<std::int64_t> values, bool scalar = false, std::stop_token stop = {}) {
    auto builder = memory::MutableBuffer::allocate(values.size() * 8);
    {
        auto lease = builder.borrow();
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!(i % 1024))
                stopped(stop);
            write(lease.bytes().subspan(i * 8, 8), values[i]);
        }
    }
    return wrap(memory::BufferView(std::move(builder).freeze(), element(), {values.size()}), scalar);
}
kernel::NumericalContract numerical(bool reduction) {
    kernel::NumericalContract result;
    result.storage_format = "gr.int64@1";
    result.accumulation_format = "gr.int64@1";
    result.integer_overflow = kernel::IntegerOverflow::Error;
    result.reduction_order =
        reduction ? kernel::ReductionOrder::InputOrder : kernel::ReductionOrder::NotApplicable;
    return result;
}
void add(runtime::Catalog &catalog, kernel::OperationDescriptor descriptor,
         execution::ImplementationFunction function,
         kernel::OperationRegistry::InputValidator validator = {}) {
    catalog.pin_types(descriptor);
    const auto id = descriptor.id;
    catalog.register_operation(descriptor, std::move(validator));
    execution::ImplementationDescriptor implementation;
    implementation.id = "gr.experimental.cpu." + id.substr(std::string("gr.experimental.").size());
    implementation.operation = id;
    implementation.contract_fingerprint = descriptor.fingerprint();
    implementation.backend_ids = {"cpu.sequential", "cpu.threaded"};
    implementation.device_kinds = {"cpu"};
    implementation.memory_domains = {"host"};
    implementation.thread_safe = true;
    catalog.register_implementation(std::move(implementation), std::move(function));
}
} // namespace
static std::vector<std::int64_t> decode_integers(const memory::BufferView &v, std::stop_token stop = {}) {
    if (v.shape().size() != 1 || v.element() != element() || v.elements() > maximum_elements)
        throw Error("invalid_value", "Expected int64 vector");
    std::vector<std::int64_t> result;
    stopped(stop);
    result.reserve(v.elements());
    for (std::size_t i = 0; i < v.elements(); ++i) {
        if (!(i % 1024))
            stopped(stop);
        std::array<std::size_t, 1> at{i};
        result.push_back(read(v.element_bytes(at)));
    }
    return result;
}
std::vector<std::int64_t> integers(const memory::BufferView &v) {
    return decode_integers(v);
}
void install(runtime::Catalog &catalog) {
    catalog.register_module({"gr.experimental.data@1", {}, {{"status", "EXPERIMENTAL"}, {"layer", 3}}});
    catalog.register_type({column_type,
                           kernel::Representation::Buffers,
                           "Finite ordered sequence of signed int64 values; no missing entries or units.",
                           {{"min_buffers", 1},
                            {"max_buffers", 1},
                            {"required_buffers", {"values"}},
                            {"max_logical_bytes", maximum_elements * 8}}},
                          [](const kernel::Value &value) { (void)column(value); });
    catalog.register_type({scalar_type,
                           kernel::Representation::Buffers,
                           "One exact signed int64 result.",
                           {{"min_buffers", 1}, {"max_buffers", 1}, {"required_buffers", {"values"}}}},
                          [](const kernel::Value &value) {
                              if (column(value).elements() != 1)
                                  throw Error("invalid_value", "Scalar needs one element");
                          });
    kernel::OperationDescriptor transform;
    transform.id = "gr.experimental.int64-transform@1";
    transform.inputs = {column_type, "gr.json@1"};
    transform.outputs = {column_type};
    transform.meaning =
        "Compute each checked multiply(value,multiplier), then checked add(addend), in source order.";
    transform.numerical = numerical(false);
    transform.preconditions = {"Parameters contain exactly canonical integer multiplier and addend (each "
                               "within +/-9007199254740991)."};
    add(
        catalog, transform,
        [](const kernel::Invocation &args, std::stop_token stop) {
            const auto &params = args.at(1).as_json();
            const auto multiplier = params.at("multiplier").get<std::int64_t>(),
                       addend = params.at("addend").get<std::int64_t>();
            stopped(stop);
            auto values = decode_integers(column(args.at(0)), stop);
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (!(i % 1024))
                    stopped(stop);
                values[i] = kernel::checked_add(kernel::checked_multiply(values[i], multiplier), addend);
            }
            return kernel::Invocation{built(std::move(values), false, stop)};
        },
        [](const kernel::Invocation &args) {
            const auto &params = args.at(1).as_json();
            if (!params.is_object() || params.size() != 2 || !params.contains("multiplier") ||
                !params.contains("addend") || !params.at("multiplier").is_number_integer() ||
                !params.at("addend").is_number_integer())
                throw Error("invalid_value", "Transform requires integer multiplier and addend only");
            (void)canonical(params);
        });
    kernel::OperationDescriptor concat;
    concat.id = "gr.experimental.int64-concat@1";
    concat.inputs = {column_type, column_type};
    concat.outputs = {column_type};
    concat.meaning = "Concatenate left sequence then right sequence preserving each exact element.";
    add(catalog, concat, [](const kernel::Invocation &args, std::stop_token stop) {
        stopped(stop);
        auto left = decode_integers(column(args.at(0)), stop),
             right = decode_integers(column(args.at(1)), stop);
        if (right.size() > maximum_elements - left.size())
            throw Error("invalid_value", "Concatenation exceeds element limit");
        left.insert(left.end(), right.begin(), right.end());
        stopped(stop);
        return kernel::Invocation{built(std::move(left), false, stop)};
    });
    kernel::OperationDescriptor sum;
    sum.id = "gr.experimental.int64-sum@1";
    sum.inputs = {column_type};
    sum.outputs = {scalar_type};
    sum.meaning =
        "Checked left-to-right int64 sum starting at zero; overflow fails at the first overflowing prefix.";
    sum.numerical = numerical(true);
    add(catalog, sum, [](const kernel::Invocation &args, std::stop_token stop) {
        stopped(stop);
        const auto values = decode_integers(column(args.at(0)), stop);
        std::int64_t total = 0;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!(i % 1024))
                stopped(stop);
            total = kernel::checked_add(total, values[i]);
        }
        return kernel::Invocation{built({total}, true, stop)};
    });
}
Selection select(runtime::TransactionCoordinator &coordinator, const std::string &workspace,
                 const std::string &record_id, const std::string &pointer,
                 const std::string &semantic_revision, const std::string &physical_revision) {
    if (pointer.size() > 1024)
        throw Error("invalid_argument", "Payload pointer exceeds 1024 bytes");
    auto snapshot = coordinator.history_snapshot(workspace, semantic_revision, physical_revision);
    for (const auto &record : snapshot.at("records"))
        if (record.at("document").at("id") == record_id) {
            const auto &values = record.at("document").at("payload").at(Json::json_pointer(pointer));
            if (!values.is_array() || values.size() > maximum_elements)
                throw Error("invalid_value", "Selected payload must be a bounded integer array");
            std::vector<std::int64_t> decoded;
            decoded.reserve(values.size());
            for (const auto &v : values) {
                if (!v.is_number_integer() || (v.is_number_unsigned() && v.get<std::uint64_t>() > INT64_MAX))
                    throw Error("invalid_value", "Selected values must be int64 integers");
                decoded.push_back(v.get<std::int64_t>());
            }
            auto provenance = snapshot.at("provenance");
            provenance["record_id"] = record_id;
            provenance["record_version"] = record.at("version");
            provenance["payload_pointer"] = pointer;
            provenance["derivation"] = "explicit ordered int64 column selection";
            return {built(std::move(decoded)), std::move(provenance)};
        }
    throw Error("not_found", "Selected historical record does not exist");
}
runtime::Graph transform_sum(const Selection &selection, std::int64_t multiplier, std::int64_t addend) {
    const auto &view = column(selection.column);
    const auto split = view.elements() / 2;
    auto params = kernel::Value::json("gr.json@1", {{"multiplier", multiplier}, {"addend", addend}});
    runtime::Graph graph;
    runtime::Node left;
    left.id = "left";
    left.operation = "gr.experimental.int64-transform@1";
    left.inputs = {wrap(view.slice(0, 0, split)), params};
    runtime::Node right;
    right.id = "right";
    right.operation = left.operation;
    right.inputs = {wrap(view.slice(0, split, view.elements() - split)), params};
    runtime::Node joined;
    joined.id = "joined";
    joined.operation = "gr.experimental.int64-concat@1";
    joined.inputs = {runtime::OutputRef{"left", 0}, runtime::OutputRef{"right", 0}};
    runtime::Node total;
    total.id = "sum";
    total.operation = "gr.experimental.int64-sum@1";
    total.inputs = {runtime::OutputRef{"joined", 0}};
    graph.nodes = {std::move(left), std::move(right), std::move(joined), std::move(total)};
    graph.outputs = {{"joined", 0}, {"sum", 0}};
    return graph;
}
} // namespace gr::experimental::data
