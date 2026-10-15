#pragma once
#include "gr/memory/buffer.hpp"
#include "gr/runtime.hpp"

namespace gr::experimental::data {
// Optional Layer 3 proof of concept, not a kernel builtin or a dataframe API.
void install(runtime::Catalog &);
struct Selection {
    kernel::Value column;
    Json provenance;
};
Selection select(runtime::TransactionCoordinator &, const std::string &workspace,
                 const std::string &record_id, const std::string &payload_pointer,
                 const std::string &semantic_revision = "", const std::string &physical_revision = "");
runtime::Graph transform_sum(const Selection &, std::int64_t multiplier, std::int64_t addend);
std::vector<std::int64_t> integers(const memory::BufferView &);
} // namespace gr::experimental::data
