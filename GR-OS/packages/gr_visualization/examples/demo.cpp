#include "fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: example_visualization OUTPUT_DIRECTORY\n";
            return 2;
        }
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        const auto snapshot = visualization_fixture::make().snapshot();
        for (const auto layout :
             {gr_visualization::Layout::incidence_columns, gr_visualization::Layout::temporal_bands}) {
            gr_visualization::ProjectionOptions options;
            options.layout = layout;
            options.title = "Synthetic qualified participation claims";
            const auto scene = gr_visualization::project(snapshot, options);
            const auto stem = gr_visualization::to_string(layout);
            std::ofstream json(output / (stem + ".scene.json"), std::ios::binary);
            std::ofstream svg(output / (stem + ".svg"), std::ios::binary);
            json.exceptions(std::ios::badbit | std::ios::failbit);
            svg.exceptions(std::ios::badbit | std::ios::failbit);
            json << scene.encode();
            svg << gr_visualization::svg(scene);
            std::cout << stem << ": " << scene.id() << '\n';
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
