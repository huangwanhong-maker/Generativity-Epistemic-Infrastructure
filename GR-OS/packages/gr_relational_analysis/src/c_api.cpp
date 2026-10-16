#include <atomic>
#include <cstdlib>
#include <cstring>
#include <gr_relational_analysis/analysis.hpp>
#include <gr_relational_analysis/c_api.h>
#include <gr_relational_analysis/package.hpp>
#include <map>
#include <mutex>

namespace {
using gr::Json;
constexpr std::size_t source_limit = 64 * 1024 * 1024;
constexpr std::size_t retained_limit = 256 * 1024 * 1024;
struct Context {
    std::shared_ptr<gr::data::Library> library = std::make_shared<gr::data::Library>();
    std::shared_ptr<gr::data::relational::Session> session;
    Context() {
        session = gr::data::relational::install(*library);
        library->seal();
    }
};
struct Entry {
    std::shared_ptr<Context> context;
    gr_relational_analysis::NetworkView view;
    std::size_t charge = 0;
    std::shared_ptr<const void> reservation;
};
std::mutex mutex;
std::map<std::string, std::shared_ptr<Entry>> entries;
std::size_t reserved_bytes = 0, reserved_entries = 0;
std::atomic<std::uint64_t> sequence{1};
struct Reservation {
    std::size_t charge = 0;
    bool active = false;
    ~Reservation() {
        if (active) {
            std::lock_guard lock(mutex);
            --reserved_entries;
            reserved_bytes -= charge;
        }
    }
};
std::shared_ptr<Context> context() {
    static const auto value = std::make_shared<Context>();
    return value;
}
void keys(const Json &value, std::initializer_list<std::string_view> allowed) {
    if (!value.is_object())
        throw gr::Error("invalid_request", "Request must be an object");
    for (auto it = value.begin(); it != value.end(); ++it)
        if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end())
            throw gr::Error("invalid_request", "Unknown request field: " + it.key());
}
std::shared_ptr<Entry> get(const std::string &handle) {
    std::lock_guard lock(mutex);
    const auto found = entries.find(handle);
    if (found == entries.end())
        throw gr::Error("invalid_handle", "Unknown or closed analysis network");
    return found->second;
}
bool close_handle(const std::string &handle) {
    std::shared_ptr<Entry> removed;
    {
        std::lock_guard lock(mutex);
        const auto found = entries.find(handle);
        if (found == entries.end())
            return false;
        removed = std::move(found->second);
        entries.erase(found);
    }
    return true;
}
int respond(const Json &value, int status, char **response, size_t *size) {
    const auto encoded = value.dump();
    if (encoded.size() > source_limit + 1024 * 1024)
        throw gr::Error("response_limit", "Analysis response exceeds transport limit");
    auto allocation = static_cast<char *>(std::malloc(encoded.size() + 1));
    if (!allocation)
        return 2;
    std::memcpy(allocation, encoded.c_str(), encoded.size() + 1);
    *response = allocation;
    *size = encoded.size();
    return status;
}
Json dispatch(const Json &request, std::span<const std::uint8_t> payload) {
    if (!request.is_object() || !request.contains("op") || !request.at("op").is_string())
        throw gr::Error("invalid_request", "A string operation is required");
    const auto operation = request.at("op").get<std::string>();
    if (!payload.empty() && operation != "import" && operation != "snapshot.validate")
        throw gr::Error("invalid_request", "This operation does not accept raw payload");
    if (operation == "manifest") {
        keys(request, {"op"});
        return gr_relational_analysis::manifest();
    }
    if (operation == "import") {
        keys(request, {"op", "spec", "provenance"});
        const auto charge = payload.size() + gr::canonical(request).size();
        auto reservation = std::make_shared<Reservation>();
        {
            std::lock_guard lock(mutex);
            if (reserved_entries >= 64 || charge > retained_limit - reserved_bytes)
                throw gr::Error("resource_limit", "Analysis handles/source-byte reservation exhausted");
            ++reserved_entries;
            reserved_bytes += charge;
            reservation->charge = charge;
            reservation->active = true;
        }
        try {
            auto value = std::make_shared<Entry>();
            value->reservation = reservation;
            value->context = context();
            gr::data::DataValue source{
                gr::data::ValueSpec::from_json(request.at("spec")),
                gr::kernel::Value::bytes("gr.data.payload@1", {payload.begin(), payload.end()}),
                request.at("provenance")};
            if (!source.provenance.is_object())
                throw gr::Error("invalid_request", "Computational provenance must be an object");
            value->view = gr_relational_analysis::analyze(source, value->context->session);
            value->charge = charge;
            const auto handle = "network:" + std::to_string(sequence.fetch_add(1));
            Json result{{"handle", handle},
                        {"identity", value->view.identity()},
                        {"validation_basis", "source_derived"},
                        {"source_bytes", payload.size()}};
            std::lock_guard lock(mutex);
            entries.emplace(handle, std::move(value));
            return result;
        } catch (...) {
            throw;
        }
    }
    if (operation == "snapshot.validate") {
        keys(request, {"op"});
        const auto snapshot = gr_relational_analysis::NetworkSnapshot::decode(payload);
        return {{"snapshot_id", snapshot.id()}, {"validation_basis", snapshot.validation_basis()}};
    }
    if (operation == "close") {
        keys(request, {"op", "handle"});
        if (!close_handle(request.at("handle").get_ref<const std::string &>()))
            throw gr::Error("invalid_handle", "Unknown or closed analysis network");
        return Json::object();
    }
    if (operation == "summary" || operation == "snapshot" || operation == "describe") {
        keys(request, {"op", "handle"});
        const auto value = get(request.at("handle").get<std::string>());
        if (operation == "summary")
            return gr_relational_analysis::summarize(value->view);
        if (operation == "snapshot")
            return value->view.snapshot().to_json();
        return {{"identity", value->view.identity()},
                {"source_bytes_reserved", value->charge},
                {"validation_basis", "source_derived"}};
    }
    if (operation == "compare") {
        keys(request, {"op", "left", "right"});
        const auto left = get(request.at("left").get<std::string>());
        const auto right = get(request.at("right").get<std::string>());
        return gr_relational_analysis::compare(left->view, right->view);
    }
    throw gr::Error("unsupported_operation", "Unknown analysis operation");
}
} // namespace
extern "C" uint32_t gr_analysis_abi_version(void) {
    return 1;
}
extern "C" void gr_analysis_free(void *allocation) {
    std::free(allocation);
}
extern "C" int gr_analysis_call_v1(const char *request, size_t request_size, const uint8_t *payload,
                                   size_t payload_size, char **response, size_t *response_size) {
    if (!response || !response_size)
        return 2;
    *response = nullptr;
    *response_size = 0;
    try {
        if ((!request && request_size) || (!payload && payload_size) || request_size > 1024 * 1024 ||
            payload_size > source_limit)
            throw gr::Error("invalid_request", "Invalid transport pointer or byte limit");
        const auto parsed = gr::parse_metadata(std::string_view(request ? request : "", request_size));
        const auto result = dispatch(parsed, {payload, payload_size});
        // An allocation failure must not strand an unreported new network.
        const auto rollback = [&] {
            if (parsed.at("op").get_ref<const std::string &>() == "import")
                close_handle(result.at("handle").get_ref<const std::string &>());
        };
        try {
            const auto status = respond({{"ok", true}, {"result", result}}, 0, response, response_size);
            if (status != 0)
                rollback();
            return status;
        } catch (...) {
            rollback();
            throw;
        }
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
