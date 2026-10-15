#pragma once
#include "gr/data/standard.hpp"

namespace gr::data {
// Independently installs the logical matrix family plus its CSR representation
// and operators. Missing entries mean exact zero; entries are sorted and unique.
void install_sparse(Library &, StandardOptions = {});
DataValue csr_i64(std::size_t rows, std::size_t columns, std::span<const std::int64_t> indptr,
                  std::span<const std::int64_t> indices, std::span<const std::int64_t> values,
                  StandardOptions = {});
// Representation gr.data.csr-i64@1; operator gr.sparse.matvec@1.
// Matvec checks each multiply and left-to-right row accumulation independently.
} // namespace gr::data
