#include "manifest.hpp"
#include <bit>
#include <gr/kernel/numerical.hpp>
#include <gr/memory/access.hpp>
#include <gr_diagonal/diagonal.hpp>

namespace gr_diagonal {
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw gr::Error("diagonal_contract", message);
}
const gr::memory::BufferView &view(const gr::kernel::Value &value, std::size_t limit) {
    const auto &set = value.as_buffers();
    require(set.buffers().size() == 1, "Diagonal needs exactly one values buffer");
    const auto &v = set.at("values");
    require(v.shape().size() == 1 && v.shape()[0] <= limit, "Diagonal needs a bounded one-dimensional view");
    const auto &e = v.element();
    require(e.format_id == "gr.int64@1" && e.item_size == 8 && e.alignment == 8 &&
                e.byte_order == gr::memory::ByteOrder::Little && e.parameters.empty(),
            "Expected exact little-endian int64 encoding");
    require(set.metadata().is_object() && set.metadata().empty(),
            "No diagonal representation metadata is defined");
    return v;
}
std::int64_t read(const gr::memory::ElementRange &v, std::size_t index) {
    return std::bit_cast<std::int64_t>(gr::memory::load_little<std::uint64_t>(v.at(index)));
}
} // namespace
gr::Json manifest() {
    return gr::Json::parse(package_manifest);
}
gr::data::Type type() {
    return {"example.diagonal.int64@1"};
}
gr::data::ValueSpec spec() {
    return {type(), "example.diagonal.le-buffer@1"};
}
gr::data::DataValue make(std::span<const std::int64_t> values, gr::data::StandardOptions options) {
    auto column = gr::data::column_i64(values, {}, options);
    const auto &v = column.payload.as_buffers().at("values");
    return {spec(),
            gr::kernel::Value::buffers("example.diagonal.payload@1", gr::memory::BufferSet({{"values", v}})),
            {{"package", manifest().at("id")}, {"version", "0.1.0"}}};
}
void install(gr::data::Library &library, gr::data::StandardOptions options) {
    require(options.max_elements > 0 && options.max_elements <= 16 * 1024 * 1024 && options.max_columns > 0 &&
                options.max_columns <= 64 && bool(options.allocation_budget),
            "Invalid package construction limits or allocation budget");
    gr::data::TypeFamily family;
    family.id = type().family;
    family.meaning = "Finite square diagonal linear map over exact signed int64; size belongs to the value";
    family.validate = [](const gr::data::Type &t) {
        require(t.parameters.empty(), "Diagonal type has no static parameters");
    };
    family.metadata = {{"package", manifest()}};
    library.register_type(std::move(family));
    gr::data::Representation representation;
    representation.id = spec().representation;
    representation.metadata = {{"max_elements", options.max_elements}};
    representation.meaning =
        "Owned possibly strided little-endian diagonal coefficients; off-diagonal entries are exact zero";
    representation.supports = [](const gr::data::Type &t) { return t == type(); };
    representation.validate = [options](const gr::data::Type &, const gr::kernel::Value &v) {
        (void)view(v, options.max_elements);
    };
    library.register_representation(std::move(representation));
    gr::data::Operator operation;
    operation.id = "example.diagonal.apply@1";
    operation.meaning =
        "Exact pointwise checked product of diagonal coefficients and a same-length nonnullable int64 vector";
    operation.metadata = {{"package", manifest()}};
    const gr::data::ValueSpec column{gr::data::column_type(gr::data::int64_type()),
                                     "gr.data.column-buffers@1"};
    operation.infer = [column](const std::vector<gr::data::ValueSpec> &inputs, const gr::Json &attributes) {
        require(attributes.is_object() && attributes.empty(), "No attributes accepted");
        require(inputs.size() == 2 && inputs[0] == spec() && inputs[1] == column,
                "Expected diagonal and nonnullable int64 column");
        return std::vector<gr::data::ValueSpec>{column};
    };
    operation.specialize = [options](const auto &, const auto &, const gr::Json &) {
        gr::data::Specialization specialized;
        specialized.contract.meaning = "One exact checked multiply per row; no reduction or reassociation";
        gr::kernel::NumericalContract numerical;
        numerical.storage_format = "gr.int64@1";
        numerical.accumulation_format = "gr.int64@1";
        numerical.integer_overflow = gr::kernel::IntegerOverflow::Error;
        specialized.contract.numerical = numerical;
        specialized.validate_inputs = [options](const gr::kernel::Invocation &inputs) {
            const auto &diagonal = view(inputs[0], options.max_elements);
            const auto &vector = inputs[1].as_buffers().at("values");
            require(diagonal.shape()[0] == vector.shape()[0], "Diagonal and vector lengths differ");
        };
        gr::data::Provider provider;
        provider.descriptor.id = "example.diagonal.cpu.checked@1";
        provider.descriptor.backend_ids = {"cpu.sequential", "cpu.threaded"};
        provider.descriptor.device_kinds = {"cpu"};
        provider.descriptor.memory_domains = {"host"};
        provider.descriptor.thread_safe = true;
        provider.function = [options](const gr::kernel::Invocation &inputs, std::stop_token stop) {
            const auto &diagonal = inputs[0].as_buffers().at("values");
            const auto &vector = inputs[1].as_buffers().at("values");
            const gr::memory::ElementRange coefficients(diagonal), elements(vector);
            std::vector<std::int64_t> result(diagonal.shape()[0]);
            for (std::size_t i = 0; i < result.size(); ++i) {
                if ((i % 4096) == 0 && stop.stop_requested())
                    throw gr::Error("execution_cancelled", "Diagonal computation cancelled");
                result[i] = gr::kernel::checked_multiply(read(coefficients, i), read(elements, i));
            }
            if (stop.stop_requested())
                throw gr::Error("execution_cancelled", "Diagonal computation cancelled");
            return gr::kernel::Invocation{gr::data::column_i64(result, {}, options).payload};
        };
        specialized.providers.push_back(std::move(provider));
        return specialized;
    };
    library.register_operator(std::move(operation));
}
} // namespace gr_diagonal
