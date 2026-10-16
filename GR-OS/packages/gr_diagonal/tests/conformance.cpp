#include <array>
#include <gr_diagonal/diagonal.hpp>
#include <iostream>
#include <limits>
#include <random>

int main() {
    try {
        auto library = std::make_shared<gr::data::Library>();
        gr::data::install_standard(*library);
        gr_diagonal::install(*library);
        library->seal();
        const gr::data::ValueSpec column{gr::data::column_type(gr::data::int64_type()),
                                         "gr.data.column-buffers@1"};
        auto matrix = gr::data::input(library, "diagonal", gr_diagonal::spec());
        auto vector = gr::data::input(library, "vector", column);
        auto multiplied = gr::data::apply(library, "example.diagonal.apply@1", {matrix, vector});
        gr::data::Function function({multiplied});
        gr::runtime::Runtime runtime({.workers = 2});
        std::mt19937 generator(1027);
        for (std::size_t size = 0; size < 32; ++size) {
            std::vector<std::int64_t> coefficients(size), input(size), expected(size);
            for (std::size_t i = 0; i < size; ++i) {
                coefficients[i] = static_cast<int>(generator() % 21) - 10;
                input[i] = static_cast<int>(generator() % 21) - 10;
                expected[i] = coefficients[i] * input[i];
            }
            auto program = function.bind(
                {{"diagonal", gr_diagonal::make(coefficients)}, {"vector", gr::data::column_i64(input)}});
            auto output = program.materialize(runtime.run(program.plan()));
            auto items = gr::data::to_i64(output.at(0));
            if (items.size() != expected.size())
                throw std::runtime_error("length");
            for (std::size_t i = 0; i < size; ++i)
                if (items[i] != expected[i])
                    throw std::runtime_error("independent diagonal oracle");
        }
        auto a = gr::data::literal(library, gr_diagonal::make(std::array<std::int64_t, 2>{2, 3}));
        auto b = gr::data::literal(library, gr::data::column_i64(std::array<std::int64_t, 2>{4, 5}));
        auto instance = function.instantiate({{"diagonal", a}, {"vector", b}});
        auto sum = gr::data::apply(library, "gr.data.sum@1", instance, {{"nulls", "skip"}, {"min_count", 1}});
        auto composed = gr::data::compile({sum});
        if (gr::data::get_optional_i64(composed.materialize(runtime.run(composed.plan())).at(0)) != 23)
            throw std::runtime_error("function composition");
        auto overflow = function.bind({{"diagonal", gr_diagonal::make(std::array<std::int64_t, 1>{
                                                        std::numeric_limits<std::int64_t>::max()})},
                                       {"vector", gr::data::column_i64(std::array<std::int64_t, 1>{2})}});
        const auto overflow_result = runtime.run(overflow.plan());
        if (overflow_result.status != gr::runtime::JobStatus::failed ||
            overflow_result.error.at("code") != "numeric_overflow")
            throw std::runtime_error("overflow accepted");
        auto mismatch = function.bind({{"diagonal", gr_diagonal::make(std::array<std::int64_t, 2>{2, 3})},
                                       {"vector", gr::data::column_i64(std::array<std::int64_t, 1>{2})}});
        const auto mismatch_result = runtime.run(mismatch.plan());
        if (mismatch_result.status != gr::runtime::JobStatus::failed ||
            mismatch_result.error.at("code") != "diagonal_contract")
            throw std::runtime_error("shape mismatch accepted");
        gr::runtime::JobOptions expired;
        expired.timeout = std::chrono::milliseconds(0);
        if (runtime.run(composed.plan(), expired).status != gr::runtime::JobStatus::deadline_exceeded)
            throw std::runtime_error("deadline ignored");
        if (gr_diagonal::manifest().at("requires").at(0).at("version") != "0.7.0")
            throw std::runtime_error("SDK pin");
        gr::data::Library invalid;
        try {
            gr_diagonal::install(invalid, {.max_elements = 0});
            throw std::runtime_error("invalid options accepted");
        } catch (const gr::Error &error) {
            if (error.code != "diagonal_contract")
                throw;
        }
        auto restricted = std::make_shared<gr::data::Library>();
        gr::data::install_standard(*restricted);
        gr_diagonal::install(*restricted, {.max_elements = 16});
        restricted->seal();
        if (library->binding_fingerprint(gr_diagonal::spec()) ==
            restricted->binding_fingerprint(gr_diagonal::spec()))
            throw std::runtime_error("representation limit is not pinned");
        std::cout << "Independent extension: 32 oracle sizes, Function composition, overflow, shape and "
                     "deadline contracts passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
