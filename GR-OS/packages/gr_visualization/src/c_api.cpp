#include <cstdlib>
#include <cstring>
#include <gr_visualization/c_api.h>
#include <gr_visualization/package.hpp>
#include <gr_visualization/visualization.hpp>

namespace {
int respond(const gr::Json &value, int status, char **response, size_t *size) {
    const auto encoded = value.dump();
    if (encoded.size() > 64 * 1024 * 1024)
        throw gr::Error("response_limit", "Visualization response exceeds 64 MiB");
    auto allocation = static_cast<char *>(std::malloc(encoded.size() + 1));
    if (!allocation)
        return 2;
    std::memcpy(allocation, encoded.c_str(), encoded.size() + 1);
    *response = allocation;
    *size = encoded.size();
    return status;
}
gr::Json dispatch(const gr::Json &request, std::span<const std::uint8_t> bytes) {
    if (!request.is_object() || !request.contains("op") || !request.at("op").is_string())
        throw gr::Error("invalid_request", "A string operation is required");
    const auto op = request.at("op").get<std::string>();
    for (auto it = request.begin(); it != request.end(); ++it)
        if (it.key() != "op" && !(op == "render" && it.key() == "options"))
            throw gr::Error("invalid_request", "Unknown visualization request field");
    if (op == "manifest" && bytes.empty())
        return gr_visualization::manifest();
    if (op != "render")
        throw gr::Error("unsupported_operation", "Unknown visualization operation or extra payload");
    const auto snapshot = gr_relational_analysis::NetworkSnapshot::decode(bytes);
    auto options =
        gr_visualization::ProjectionOptions::from_json(request.value("options", gr::Json::object()));
    const auto scene = gr_visualization::project(snapshot, std::move(options));
    return {{"scene", scene.to_json()},
            {"svg", gr_visualization::svg(scene)},
            {"validation_basis", snapshot.validation_basis()},
            {"snapshot_id", snapshot.id()}};
}
} // namespace
extern "C" uint32_t gr_visualization_abi_version(void) {
    return 1;
}
extern "C" void gr_visualization_free(void *allocation) {
    std::free(allocation);
}
extern "C" int gr_visualization_call_v1(const char *request, size_t request_size, const uint8_t *snapshot,
                                        size_t snapshot_size, char **response, size_t *response_size) {
    if (!response || !response_size)
        return 2;
    *response = nullptr;
    *response_size = 0;
    try {
        if ((!request && request_size) || (!snapshot && snapshot_size) || request_size > 1024 * 1024 ||
            snapshot_size > 64 * 1024 * 1024)
            throw gr::Error("invalid_request", "Invalid transport pointer or byte limit");
        return respond(
            {{"ok", true},
             {"result", dispatch(gr::parse_metadata(std::string_view(request ? request : "", request_size)),
                                 {snapshot, snapshot_size})}},
            0, response, response_size);
    } catch (const gr::Error &error) {
        try {
            return respond(
                {{"ok", false},
                 {"error", {{"code", error.code}, {"message", error.what()}, {"details", error.details}}}},
                1, response, response_size);
        } catch (...) {
            return 2;
        }
    } catch (const std::exception &error) {
        try {
            return respond(
                {{"ok", false}, {"error", {{"code", "invalid_request"}, {"message", error.what()}}}}, 1,
                response, response_size);
        } catch (...) {
            return 2;
        }
    } catch (...) {
        return 2;
    }
}
