#include "manifest.hpp"
#include <gr_visualization/package.hpp>
namespace gr_visualization {
gr::Json manifest() {
    return gr::parse_metadata(gr_module_manifest);
}
} // namespace gr_visualization
