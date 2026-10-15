#include <gr/core.hpp>
#include <gr/execution.hpp>
#ifdef GR_CONSUMER_EXPERIMENTAL
#include <gr/experimental/data.hpp>
#endif
#include <array>
#include <gr/memory/buffer.hpp>
#include <gr/runtime.hpp>
#include <gr/storage/memory_store.hpp>
#ifdef GR_CONSUMER_DATA
#include <gr/data/numeric.hpp>
#include <gr/data/sparse.hpp>
#include <gr/data/standard.hpp>
#endif
int main() {
    auto owner = gr::memory::Buffer::copy(std::array<std::uint8_t, 3>{1, 2, 3});
    gr::memory::BufferView view(owner, {}, {3});
    if (view.slice(0, 2, 3, -1).copy_contiguous().contiguous_bytes()[0] != 3)
        return 5;
#ifdef GR_CONSUMER_EXPERIMENTAL
    gr::runtime::Catalog extension;
    gr::experimental::data::install(extension);
    extension.seal();
#endif
    auto record = gr::core::construct({{"format", "gr.record/1"},
                                       {"id", "consumer:1"},
                                       {"type", "gr.entity@1"},
                                       {"payload", gr::Json::object()},
                                       {"attribution", {{"agent", "consumer"}, {"method", "test"}}}});
    if (!gr::core::verify(record).at("valid").get<bool>())
        return 1;
    gr::storage::MemoryStore store;
    store.create_workspace("installed");
    if (store.head("installed") != "0")
        return 2;
    auto catalog = std::make_shared<gr::runtime::Catalog>();
    catalog->seal();
    gr::runtime::Planner planner(catalog);
    gr::runtime::Graph graph;
    gr::runtime::Node node;
    node.id = "installed";
    node.operation = "gr.bytes.identity@1";
    node.inputs.emplace_back(gr::kernel::Value::bytes("gr.bytes@1", {1, 2, 3}));
    graph.nodes.push_back(node);
    graph.outputs.push_back({"installed", 0});
    gr::runtime::Runtime runtime;
#ifdef GR_CONSUMER_DATA
    auto library = std::make_shared<gr::data::Library>();
    gr::data::install_standard(*library);
    gr::data::numeric::install(*library);
    gr::data::install_sparse(*library);
    library->seal();
    auto column = gr::data::column_i64(std::array<std::int64_t, 3>{1, 2, 3});
    auto source = gr::data::input(library, "chunk", column.spec);
    auto total = gr::data::apply(library, "gr.data.sum@1", {source}, {{"nulls", "skip"}, {"min_count", 1}});
    gr::data::Function function({total});
    auto program = function.bind({{"chunk", column}});
    auto computed = program.materialize(runtime.run(program.plan()));
    if (gr::data::get_optional_i64(computed.at(0)) != 6)
        return 6;
    if (gr::data::numeric::get_optional_u64(gr::data::numeric::scalar_u64(UINT64_MAX)) != UINT64_MAX)
        return 7;
#endif
    auto result = runtime.run(planner.compile(graph));
    if (result.status != gr::runtime::JobStatus::succeeded ||
        result.outputs[0].as_bytes() != gr::kernel::Bytes{1, 2, 3})
        return 3;
    return gr::execution::discover_devices().empty() ? 4 : 0;
}
