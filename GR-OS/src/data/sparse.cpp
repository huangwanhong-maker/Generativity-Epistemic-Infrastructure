#include "gr/data/sparse.hpp"
#include "gr/kernel/numerical.hpp"
#include "gr/memory/access.hpp"
#include <array>
#include <bit>
#include <limits>

namespace gr::data {
namespace {
constexpr const char *representation = "gr.data.csr-i64@1";
constexpr const char *tag = "gr.data.payload@1";
void need(bool c, const char *code, const char *message) {
    if (!c)
        throw Error(code, message);
}
void active(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Sparse operation cancelled.");
}
void options_valid(const StandardOptions &o) {
    need(o.max_elements > 0 && o.max_elements <= 16 * 1024 * 1024 && bool(o.allocation_budget),
         "data_options", "Sparse operations need a bounded element limit and allocation budget.");
}
memory::ElementDescriptor encoding() {
    return {"gr.int64@1", 8, 8, memory::ByteOrder::Little, Json::object()};
}
std::int64_t read(const memory::ElementRange &v, std::size_t index) {
    return std::bit_cast<std::int64_t>(memory::load_little<std::uint64_t>(v.at(index)));
}
void write(std::span<std::uint8_t> out, std::int64_t v) {
    memory::store_little(out, std::bit_cast<std::uint64_t>(v));
}
std::size_t dimension(const Json &meta, const char *name, std::size_t limit) {
    need(meta.contains(name) && meta.at(name).is_number_integer() && meta.at(name) >= 0 &&
             meta.at(name).get<std::uint64_t>() <= limit,
         "sparse_shape", "Sparse dimension is missing or exceeds its bound.");
    return meta.at(name).get<std::size_t>();
}
void validate(const kernel::Value &value, const StandardOptions &o) {
    const auto &set = value.as_buffers();
    const auto &meta = set.metadata();
    need(meta.is_object() && meta.size() == 2 && set.buffers().size() == 3, "sparse_layout",
         "CSR requires exactly indptr, indices, values and rows/columns metadata.");
    const auto rows = dimension(meta, "rows", o.max_elements),
               cols = dimension(meta, "columns", o.max_elements);
    const auto &ptr = set.at("indptr"), &idx = set.at("indices"), &val = set.at("values");
    for (const auto *v : {&ptr, &idx, &val})
        need(v->element() == encoding() && v->shape().size() == 1, "sparse_layout",
             "CSR uses rank-one exact little-endian int64 buffers.");
    const auto nnz = val.elements();
    need(nnz <= o.max_elements && idx.elements() == nnz && ptr.elements() == rows + 1, "sparse_shape",
         "CSR lengths are inconsistent or over the element limit.");
    const memory::ElementRange pointers(ptr), columns(idx);
    need(read(pointers, 0) == 0 && read(pointers, rows) == static_cast<std::int64_t>(nnz), "sparse_indptr",
         "CSR row pointers must begin at zero and end at nnz.");
    for (std::size_t row = 0; row < rows; ++row) {
        const auto begin = read(pointers, row), end = read(pointers, row + 1);
        need(begin >= 0 && end >= begin && static_cast<std::uint64_t>(end) <= nnz, "sparse_indptr",
             "CSR row pointers must be nonnegative and monotone.");
        std::int64_t previous = -1;
        for (auto p = begin; p < end; ++p) {
            const auto column = read(columns, static_cast<std::size_t>(p));
            need(column > previous && column >= 0 && static_cast<std::uint64_t>(column) < cols,
                 "sparse_indices",
                 "CSR column indices must be in range, strictly sorted and unique within each row.");
            previous = column;
        }
    }
}
memory::BufferView encoded(std::span<const std::int64_t> values, const StandardOptions &o) {
    auto owner = memory::MutableBuffer::allocate(values.size() * 8, 64, o.allocation_budget);
    {
        auto lease = owner.borrow();
        for (std::size_t i = 0; i < values.size(); ++i)
            write(lease.bytes().subspan(i * 8, 8), values[i]);
    }
    return {std::move(owner).freeze(), encoding(), {values.size()}};
}
execution::ImplementationDescriptor provider(std::string id) {
    execution::ImplementationDescriptor p;
    p.id = std::move(id);
    p.backend_ids = {"cpu.sequential", "cpu.threaded"};
    p.device_kinds = {"cpu"};
    p.memory_domains = {"host"};
    p.thread_safe = true;
    return p;
}
kernel::Value output(memory::MutableBuffer buffer, std::size_t rows) {
    return kernel::Value::buffers(
        tag,
        memory::BufferSet({{"values", memory::BufferView(std::move(buffer).freeze(), encoding(), {rows})}}));
}
} // namespace
DataValue csr_i64(std::size_t rows, std::size_t columns, std::span<const std::int64_t> indptr,
                  std::span<const std::int64_t> indices, std::span<const std::int64_t> values,
                  StandardOptions o) {
    options_valid(o);
    need(rows <= o.max_elements && columns <= o.max_elements && values.size() <= o.max_elements &&
             indices.size() == values.size() && indptr.size() == rows + 1,
         "sparse_shape", "CSR dimensions and buffer lengths exceed bounds or disagree.");
    auto value = kernel::Value::buffers(tag, memory::BufferSet({{"indptr", encoded(indptr, o)},
                                                                {"indices", encoded(indices, o)},
                                                                {"values", encoded(values, o)}},
                                                               {{"rows", rows}, {"columns", columns}}));
    validate(value, o);
    return {{matrix_type(int64_type()), representation}, std::move(value), Json::object()};
}
void install_sparse(Library &library, StandardOptions o) {
    options_valid(o);
    TypeFamily matrix;
    matrix.id = "gr.data.matrix@1";
    matrix.meaning = "Finite rectangular mathematical matrix; dimensions and physical dense/sparse "
                     "representation are separate. Installed by the optional sparse mathematical module.";
    matrix.min_arguments = matrix.max_arguments = 1;
    matrix.validate = [](const Type &t) {
        need(t.parameters == Json::object(), "data_type", "Matrix parameters must be empty.");
    };
    library.register_type(std::move(matrix));
    Representation r;
    r.id = representation;
    r.meaning = "Signed-int64 CSR matrix: sorted unique columns per row, missing entries exactly zero, "
                "explicit stored zeros allowed; no missing values.";
    r.metadata = {{"max_rows", o.max_elements},
                  {"max_columns", o.max_elements},
                  {"max_stored_entries", o.max_elements},
                  {"duplicate_policy", "reject"}};
    r.supports = [](const Type &t) { return t == matrix_type(int64_type()); };
    r.validate = [o](const Type &, const kernel::Value &v) { validate(v, o); };
    library.register_representation(std::move(r));
    Operator op;
    op.id = "gr.sparse.matvec@1";
    op.meaning = "Exact signed-int64 matrix-vector product; checked product and checked left-to-right sum in "
                 "ascending stored column order for each row.";
    op.metadata = {{"module", "gr.sparse@1"},
                   {"implicit_entries", "exact zero"},
                   {"arithmetic", "overflow is an error, no reassociation"}};
    op.infer = [](const std::vector<ValueSpec> &in, const Json &attributes) {
        need(attributes == Json::object(), "data_attributes", "Matvec takes no attributes.");
        need(in.size() == 2 && in[0] == ValueSpec{matrix_type(int64_type()), representation} &&
                 in[1] == ValueSpec{column_type(int64_type(), false), "gr.data.column-buffers@1"},
             "data_type", "Matvec requires CSR<int64> and a nonnullable int64 column.");
        return std::vector<ValueSpec>{in[1]};
    };
    op.specialize = [o](const auto &, const auto &, const Json &) {
        Specialization s;
        s.contract.meaning =
            "One dense nonnullable little-endian result column. Rows and each ascending-column prefix use "
            "checked int64 multiply/add; empty rows produce zero.";
        kernel::NumericalContract numeric;
        numeric.storage_format = numeric.accumulation_format = "gr.int64@1";
        numeric.integer_overflow = kernel::IntegerOverflow::Error;
        numeric.reduction_order = kernel::ReductionOrder::InputOrder;
        s.contract.numerical = numeric;
        s.validate_inputs = [o](const kernel::Invocation &in) {
            need(in[1].as_buffers().at("values").elements() ==
                     dimension(in[0].as_buffers().metadata(), "columns", o.max_elements),
                 "data_shape", "Matvec column count and vector length differ.");
        };
        s.providers.push_back(
            {provider("gr.sparse.cpu.matvec-direct@1"),
             [o](const kernel::Invocation &in, std::stop_token stop) {
                 active(stop);
                 const auto &set = in[0].as_buffers();
                 const auto n = dimension(set.metadata(), "rows", o.max_elements);
                 const memory::ElementRange ptr(set.at("indptr")), idx(set.at("indices")),
                     val(set.at("values")), x(in[1].as_buffers().at("values"));
                 auto result = memory::MutableBuffer::allocate(n * 8, 64, o.allocation_budget);
                 {
                     auto lease = result.borrow();
                     for (std::size_t row = 0; row < n; ++row) {
                         active(stop);
                         std::int64_t total = 0;
                         const auto end = read(ptr, row + 1);
                         for (auto p = read(ptr, row); p < end; ++p) {
                             if (!(p % 1024))
                                 active(stop);
                             const auto i = static_cast<std::size_t>(p);
                             total = kernel::checked_add(
                                 total, kernel::checked_multiply(
                                            read(val, i), read(x, static_cast<std::size_t>(read(idx, i)))));
                         }
                         write(lease.bytes().subspan(row * 8, 8), total);
                     }
                 }
                 return kernel::Invocation{output(std::move(result), n)};
             }});
        s.providers.push_back(
            {provider("gr.sparse.cpu.matvec-decoded@1"),
             [o](const kernel::Invocation &in, std::stop_token stop) {
                 active(stop);
                 const auto &set = in[0].as_buffers();
                 const auto n = dimension(set.metadata(), "rows", o.max_elements);
                 std::vector<std::vector<std::int64_t>> decoded;
                 for (const auto *view : {&set.at("indptr"), &set.at("indices"), &set.at("values"),
                                          &in[1].as_buffers().at("values")}) {
                     std::vector<std::int64_t> current;
                     current.reserve(view->elements());
                     const memory::ElementRange range(*view);
                     for (std::size_t i = 0; i < view->elements(); ++i) {
                         if (!(i % 1024))
                             active(stop);
                         current.push_back(read(range, i));
                     }
                     decoded.push_back(std::move(current));
                 }
                 auto result = memory::MutableBuffer::allocate(n * 8, 64, o.allocation_budget);
                 {
                     auto lease = result.borrow();
                     for (std::size_t row = 0; row < n; ++row) {
                         active(stop);
                         std::int64_t total = 0;
                         for (auto p = decoded[0][row]; p < decoded[0][row + 1]; ++p) {
                             if (!(p % 1024))
                                 active(stop);
                             const auto i = static_cast<std::size_t>(p);
                             const auto product = kernel::checked_multiply(
                                 decoded[2][i], decoded[3][static_cast<std::size_t>(decoded[1][i])]);
                             total = kernel::checked_add(total, product);
                         }
                         write(lease.bytes().subspan(row * 8, 8), total);
                     }
                 }
                 return kernel::Invocation{output(std::move(result), n)};
             }});
        return s;
    };
    library.register_operator(std::move(op));
}
} // namespace gr::data
