#pragma once
#include "gr/kernel/records.hpp"
namespace gr::core {
Json graph(const Json &snapshot, const TypeRegistry &registry = default_types());
// query: order_ids?: [...], after?: [event IDs]. No implicit predecessor closure.
Json temporal(const Json &snapshot, const Json &query = Json::object(),
              const TypeRegistry &registry = default_types());
Json match(const Json &snapshot, const Json &filter = Json::object(),
           const TypeRegistry &registry = default_types());
} // namespace gr::core
