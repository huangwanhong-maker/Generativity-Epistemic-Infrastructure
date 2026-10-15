#include "gr/data/sparse.hpp"
#include <iostream>
using namespace gr;
using namespace gr::data;
namespace {
int checks = 0;
void check(bool v, const char *message) {
    ++checks;
    if (!v)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F f) {
    try {
        f();
    } catch (const Error &e) {
        check(e.code == code, (std::string("Expected ") + code + ", got " + e.code).c_str());
        return;
    }
    throw std::runtime_error(std::string("Expected ") + code);
}
DataValue run(const std::shared_ptr<Library> &l, DataValue matrix, DataValue vector,
              const std::string &provider) {
    CallOptions options;
    options.selection.implementation = provider;
    const auto expression =
        apply(l, "gr.sparse.matvec@1", {literal(l, std::move(matrix)), literal(l, std::move(vector))},
              Json::object(), options);
    auto program = compile({expression});
    runtime::Runtime runtime({.workers = 2});
    auto result = runtime.run(program.plan());
    if (result.status != runtime::JobStatus::succeeded)
        throw Error(result.error.value("code", std::string("failed")),
                    result.error.value("message", std::string("Matvec failed")));
    return program.materialize(result)[0];
}
void generated() {
    auto l = std::make_shared<Library>();
    install_standard(*l);
    install_sparse(*l);
    l->seal();
    std::uint32_t state = 1729;
    auto random = [&] {
        state = state * 1664525u + 1013904223u;
        return state;
    };
    for (std::size_t sample = 0; sample < 32; ++sample) {
        const auto rows = sample % 8, cols = (sample * 3) % 9;
        std::vector<std::vector<std::int64_t>> dense(rows, std::vector<std::int64_t>(cols));
        std::vector<std::int64_t> ptr{0}, idx, values, vector(cols);
        for (auto &v : vector)
            v = static_cast<std::int64_t>(random() % 11) - 5;
        for (std::size_t row = 0; row < rows; ++row) {
            for (std::size_t col = 0; col < cols; ++col) {
                const auto n = static_cast<std::int64_t>(random() % 9) - 4;
                if (random() % 3) {
                    dense[row][col] = n;
                    idx.push_back(static_cast<std::int64_t>(col));
                    values.push_back(n);
                }
            }
            ptr.push_back(static_cast<std::int64_t>(values.size()));
        }
        std::vector<std::optional<std::int64_t>> expected;
        for (const auto &row : dense) {
            std::int64_t total = 0;
            for (std::size_t c = 0; c < cols; ++c)
                total += row[c] * vector[c];
            expected.push_back(total);
        }
        const auto matrix = csr_i64(rows, cols, ptr, idx, values), x = column_i64(vector);
        l->validate(matrix);
        auto direct = run(l, matrix, x, "gr.sparse.cpu.matvec-direct@1");
        auto decoded = run(l, matrix, x, "gr.sparse.cpu.matvec-decoded@1");
        check(to_i64(direct) == expected, "CSR result agrees with independently formed dense oracle");
        check(to_i64(decoded) == expected, "Second provider agrees with dense oracle");
        check(direct.payload.content_digest() == decoded.payload.content_digest(),
              "Provider result encoding agrees exactly");
    }
}
void errors() {
    Library standard_only;
    install_standard(standard_only);
    standard_only.seal();
    fails("data_unknown_type", [&] { standard_only.validate(matrix_type(int64_type())); });
    const std::vector<std::int64_t> ptr{0, 2}, duplicates{0, 0}, unsorted{1, 0}, v{1, 2};
    fails("sparse_indices", [&] { csr_i64(1, 2, ptr, duplicates, v); });
    fails("sparse_indices", [&] { csr_i64(1, 2, ptr, unsorted, v); });
    const std::vector<std::int64_t> wrong_ptr{0, 3}, indices{0, 1};
    fails("sparse_indptr", [&] { csr_i64(1, 2, wrong_ptr, indices, v); });
    const std::vector<std::int64_t> outside{0, 2};
    fails("sparse_indices", [&] { csr_i64(1, 2, ptr, outside, v); });
    auto l = std::make_shared<Library>();
    install_standard(*l);
    install_sparse(*l);
    l->seal();
    const std::vector<std::int64_t> one{1};
    fails("data_shape",
          [&] { run(l, csr_i64(1, 2, ptr, indices, v), column_i64(one), "gr.sparse.cpu.matvec-direct@1"); });
    const std::vector<std::int64_t> p3{0, 3}, i3{0, 1, 2}, dangerous{INT64_MAX, 1, -1}, x{1, 1, 1};
    for (const auto *provider : {"gr.sparse.cpu.matvec-direct@1", "gr.sparse.cpu.matvec-decoded@1"})
        fails("numeric_overflow", [&] { run(l, csr_i64(1, 3, p3, i3, dangerous), column_i64(x), provider); });
    const std::vector<std::int64_t> p1{0, 1}, i1{0}, maximum{INT64_MAX}, two{2};
    fails("numeric_overflow",
          [&] { run(l, csr_i64(1, 1, p1, i1, maximum), column_i64(two), "gr.sparse.cpu.matvec-direct@1"); });
    const std::vector<std::int64_t> safe{INT64_MAX, -1, 1};
    check(to_i64(run(l, csr_i64(1, 3, p3, i3, safe), column_i64(x), "gr.sparse.cpu.matvec-direct@1"))[0] ==
              INT64_MAX,
          "Stored column order determines checked reduction behavior");
    const std::vector<std::uint8_t> present{1};
    fails("data_type", [&] {
        run(l, csr_i64(1, 1, p1, i1, one), column_i64(one, std::span<const std::uint8_t>(present)),
            "gr.sparse.cpu.matvec-direct@1");
    });
    StandardOptions limits;
    limits.max_elements = 2;
    fails("sparse_shape", [&] { csr_i64(1, 3, p3, i3, dangerous, limits); });
}
} // namespace
int main() {
    try {
        generated();
        errors();
        std::cout << "data sparse: " << checks << " checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << "data sparse failure: " << e.what() << '\n';
        return 1;
    }
}
