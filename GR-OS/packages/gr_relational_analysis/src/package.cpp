#include "manifest.hpp"
#include <gr_relational_analysis/package.hpp>
namespace gr_relational_analysis {
gr::Json manifest() {
    return gr::parse_metadata(gr_module_manifest);
}
} // namespace gr_relational_analysis
