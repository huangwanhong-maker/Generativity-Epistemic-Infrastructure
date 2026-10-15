#pragma once
#include <gr/data/standard.hpp>
namespace gr_diagonal {
gr::Json manifest();
gr::data::Type type();
gr::data::ValueSpec spec();
void install(gr::data::Library &, gr::data::StandardOptions = {});
gr::data::DataValue make(std::span<const std::int64_t>, gr::data::StandardOptions = {});
} // namespace gr_diagonal
