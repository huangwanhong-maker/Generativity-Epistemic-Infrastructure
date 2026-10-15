#include <array>
#include <gr/data/sparse.hpp>
#include <gr/data/standard.hpp>
#include <iostream>

int main() {
    auto library = std::make_shared<gr::data::Library>();
    gr::data::install_standard(*library);
    gr::data::install_sparse(*library); // An independently installed representation/operator module.
    library->seal();
    auto prototype = gr::data::column_i64(std::array<std::int64_t, 3>{1, 2, 3});
    auto source = gr::data::input(library, "chunk", prototype.spec);
    auto total = gr::data::apply(library, "gr.data.sum@1", {source}, {{"nulls", "skip"}, {"min_count", 1}});
    gr::runtime::Runtime runtime;
    for (auto items : {std::array<std::int64_t, 3>{1, 2, 3}, std::array<std::int64_t, 3>{4, 5, 6}}) {
        auto program = gr::data::compile({total}, {{"chunk", gr::data::column_i64(items)}});
        auto result = program.materialize(runtime.run(program.plan()));
        std::cout << gr::Json{{"sum", *gr::data::get_optional_i64(result.at(0))},
                              {"lineage", result.at(0).provenance}}
                         .dump()
                  << '\n';
    }
    return 0;
}
