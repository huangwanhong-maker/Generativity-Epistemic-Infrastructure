#pragma once
#include <cstdint>
#include <gr_relational_analysis/analysis.hpp>
#include <memory>
#include <optional>
#include <stop_token>

namespace gr_visualization {
inline constexpr const char *scene_format = "gr.visualization.incidence-scene/1";
enum class Layout { incidence_columns, temporal_bands };
std::string to_string(Layout);
struct ProjectionOptions {
    Layout layout = Layout::incidence_columns;
    // Filters select occurrences only. All source vertices remain visible.
    std::optional<std::vector<std::string>> roles;
    std::optional<std::vector<std::string>> temporal_classes;
    std::uint32_t canvas_width = 1440;
    std::uint32_t row_spacing = 72;
    std::uint32_t margin = 40;
    std::uint32_t max_label_codepoints = 36;
    bool occurrence_labels = true;
    std::string title = "Recorded participation";
    gr::Json to_json() const;
    static ProjectionOptions from_json(const gr::Json &);
};
struct Limits {
    std::size_t max_vertices = 4096;
    std::size_t max_occurrences = 16384;
    std::size_t max_scene_bytes = 16 * 1024 * 1024;
    std::size_t max_svg_bytes = 32 * 1024 * 1024;
};
class Scene {
  public:
    Scene() = default;
    const gr::Json &document() const;
    gr::Json to_json() const;
    std::string encode() const;
    const std::string &id() const;
    explicit operator bool() const noexcept;

  private:
    struct Impl;
    explicit Scene(std::shared_ptr<const Impl>);
    std::shared_ptr<const Impl> impl_;
    friend Scene project(const gr_relational_analysis::NetworkSnapshot &, ProjectionOptions, Limits,
                         std::stop_token);
};
Scene project(const gr_relational_analysis::NetworkSnapshot &, ProjectionOptions = {}, Limits = {},
              std::stop_token = {});
// Fixed SVG subset: no raw markup, hyperlinks, scripts, or external resources.
// The returned string is caller-owned; this function performs no I/O.
std::string svg(const Scene &, Limits = {}, std::stop_token = {});
} // namespace gr_visualization
