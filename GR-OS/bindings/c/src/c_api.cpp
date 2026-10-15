#include "gr/c_api.h"
#include "gr/core.hpp"
#include "gr/kernel/history.hpp"
#include "internal.hpp"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
namespace gr::binding {
constexpr std::size_t request_limit = 16 * 1024 * 1024;
constexpr std::size_t response_limit = 64 * 1024 * 1024;
std::string text(const Json &object, const char *key) {
    if (!object.contains(key) || !object.at(key).is_string())
        throw gr::Error("invalid_request", std::string(key) + " must be a string");
    const auto value = object.at(key).get<std::string>();
    if (value.find('\0') != std::string::npos)
        throw gr::Error("invalid_request", std::string(key) + " contains NUL");
    return value;
}
std::size_t count(const Json &object, const char *key, std::size_t fallback) {
    if (!object.contains(key))
        return fallback;
    const auto &value = object.at(key);
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
        throw gr::Error("invalid_request", std::string(key) + " must be a nonnegative integer");
    auto result = value.get<std::uint64_t>();
    if (result > std::numeric_limits<std::size_t>::max())
        throw gr::Error("invalid_request", std::string(key) + " is out of range");
    return static_cast<std::size_t>(result);
}
const Json &array(const Json &object, const char *key) {
    const auto &value = object.at(key);
    if (!value.is_array())
        throw gr::Error("invalid_request", std::string(key) + " must be an array");
    return value;
}

std::string encode(const std::vector<std::uint8_t> &bytes) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        unsigned n = static_cast<unsigned>(bytes[i]) << 16;
        if (i + 1 < bytes.size())
            n |= static_cast<unsigned>(bytes[i + 1]) << 8;
        if (i + 2 < bytes.size())
            n |= bytes[i + 2];
        result.push_back(alphabet[(n >> 18) & 63]);
        result.push_back(alphabet[(n >> 12) & 63]);
        result.push_back(i + 1 < bytes.size() ? alphabet[(n >> 6) & 63] : '=');
        result.push_back(i + 2 < bytes.size() ? alphabet[n & 63] : '=');
    }
    return result;
}
std::vector<std::uint8_t> decode(const std::string &encoded) {
    auto digit = [](char c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1;
    };
    auto bad = [] { throw gr::Error("invalid_base64", "Expected canonical padded base64"); };
    if (encoded.size() % 4 != 0)
        bad();
    std::vector<std::uint8_t> result;
    result.reserve(encoded.size() / 4 * 3);
    for (std::size_t i = 0; i < encoded.size(); i += 4) {
        const bool final = i + 4 == encoded.size();
        const int a = digit(encoded[i]), b = digit(encoded[i + 1]);
        const int c = encoded[i + 2] == '=' ? 0 : digit(encoded[i + 2]);
        const int d = encoded[i + 3] == '=' ? 0 : digit(encoded[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0)
            bad();
        if (encoded[i + 2] == '=' && (!final || encoded[i + 3] != '=' || (b & 15)))
            bad();
        if (encoded[i + 3] == '=' && (!final || (c & 3)))
            bad();
        const unsigned n = static_cast<unsigned>((a << 18) | (b << 12) | (c << 6) | d);
        result.push_back(static_cast<std::uint8_t>(n >> 16));
        if (encoded[i + 2] != '=')
            result.push_back(static_cast<std::uint8_t>(n >> 8));
        if (encoded[i + 3] != '=')
            result.push_back(static_cast<std::uint8_t>(n));
    }
    return result;
}

Json dispatch(const Json &request) {
    if (!request.is_object() || !request.contains("abi") || !request.at("abi").is_number_integer() ||
        request.at("abi") != 1)
        throw gr::Error("abi_version", "Request must declare integer abi: 1");
    const auto op = text(request, "op");
    if (op == "sdk.capabilities")
        return Json{{"version", GR_OS_VERSION},
                    {"abi", 1},
                    {"features",
                     {"typed_execution", "recording_history", "verified_binary_readers", "strict_metadata",
                      "runtime_jobs", "runtime_transactions", "owned_buffers", "compact_history"
#ifdef GR_DATA_LANGUAGE
                      ,
                      "data_language"
#endif
                     }},
                    {"request_bytes", request_limit},
                    {"response_bytes", response_limit}};
    if (op == "core.types")
        return gr::core::TypeRegistry{}.descriptors();
    if (op == "history.validate")
        return gr::history::validate(request.at("history"));
    if (op == "history.append")
        return gr::history::append(request.at("history"), request.at("snapshot"), array(request, "parents"),
                                   request.at("attribution"));
    if (op == "history.reconstruct")
        return gr::history::reconstruct(request.at("history"), text(request, "revision"));
    if (op == "history.diff")
        return gr::history::diff(request.at("history"), text(request, "left"), text(request, "right"));
    if (op == "history.merge")
        return gr::history::merge(request.at("history"), text(request, "base"), text(request, "left"),
                                  text(request, "right"), request.at("resolutions"),
                                  request.at("attribution"));
    if (op == "core.construct")
        return gr::core::construct(request.at("document"));
    if (op == "core.verify")
        return gr::core::verify(request.at("record"));
    if (op == "core.validate")
        return gr::core::validate_snapshot(request.at("snapshot"), request.value("for_mutation", false));
    if (op == "core.apply")
        return gr::core::apply(request.at("snapshot"), array(request, "updates"));
    if (op == "core.graph")
        return gr::core::graph(request.at("snapshot"));
    if (op == "core.time")
        return gr::core::temporal(request.at("snapshot"), request.value("query", Json::object()));
    if (op == "core.match")
        return gr::core::match(request.at("snapshot"), request.value("filter", Json::object()));
    if (auto result = storage_call(request, op))
        return *result;
    if (auto result = buffer_call(request, op))
        return *result;
#ifdef GR_DATA_LANGUAGE
    if (auto result = data_call(request, op))
        return *result;
#endif
#ifdef GR_EXPERIMENTAL_DATA
    if (auto result = experimental_call(request, op))
        return *result;
#endif
    if (auto result = execution_call(request, op))
        return *result;
    if (auto result = transaction_call(request, op))
        return *result;
    if (auto result = runtime_call(request, op))
        return *result;
    if (op == "metadata.parse")
        return gr::parse_metadata(text(request, "utf8"));
    throw gr::Error("unsupported_operation", "Unknown C ABI operation", {{"op", op}});
}
Json parse_request(const char *begin, std::size_t size) {
    if (size > request_limit)
        throw gr::Error("request_limit", "C ABI request exceeds 16 MiB");
    if (std::memchr(begin, '\0', size))
        throw gr::Error("invalid_request", "JSON request contains NUL");
    std::vector<std::set<std::string>> keys;
    return Json::parse(begin, begin + size, [&keys](int depth, Json::parse_event_t event, Json &parsed) {
        if (depth > 64)
            throw gr::Error("request_limit", "JSON request nesting exceeds 64");
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        else if (event == Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
            throw gr::Error("invalid_request", "Duplicate JSON object key");
        else if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    });
}
} // namespace gr::binding

extern "C" uint32_t gr_abi_version(void) {
    return 1;
}
extern "C" void gr_free(void *allocation) {
    std::free(allocation);
}
namespace gr::binding {
int respond(const std::function<Json()> &action, char **response, size_t *response_size) noexcept {
    if (!response || !response_size)
        return 2;
    *response = nullptr;
    *response_size = 0;
    struct ExportGuard {
        std::size_t checkpoint = buffer_export_checkpoint();
#ifdef GR_DATA_LANGUAGE
        std::size_t data_checkpoint = data_export_checkpoint();
#endif
        bool success = false;
        ~ExportGuard() {
            buffer_export_finish(checkpoint, success);
#ifdef GR_DATA_LANGUAGE
            data_export_finish(data_checkpoint, success);
#endif
        }
    } exports;
    try {
        Json envelope;
        int status = 0;
        try {
            envelope = {{"abi", 1}, {"ok", true}, {"result", action()}};
        } catch (const gr::Error &e) {
            status = 1;
            envelope = {{"abi", 1},
                        {"ok", false},
                        {"error", {{"code", e.code}, {"message", e.what()}, {"details", e.details}}}};
        } catch (const Json::exception &e) {
            status = 1;
            envelope = {
                {"abi", 1},
                {"ok", false},
                {"error", {{"code", "invalid_request"}, {"message", e.what()}, {"details", Json::object()}}}};
        } catch (const std::exception &e) {
            status = 1;
            envelope = {
                {"abi", 1},
                {"ok", false},
                {"error", {{"code", "native_error"}, {"message", e.what()}, {"details", Json::object()}}}};
        }
        auto result = envelope.dump();
        if (result.size() > response_limit) {
            status = 1;
            result =
                R"({"abi":1,"ok":false,"error":{"code":"response_limit","message":"C ABI response exceeds 64 MiB","details":{}}})";
        }
        auto *allocation = static_cast<char *>(std::malloc(result.size() + 1));
        if (!allocation)
            return 2;
        std::memcpy(allocation, result.c_str(), result.size() + 1);
        *response = allocation;
        *response_size = result.size();
        exports.success = status == 0;
        return status;
    } catch (...) {
        return 2;
    }
}

} // namespace gr::binding
extern "C" int gr_call_json(const char *request, size_t request_size, char **response,
                            size_t *response_size) {
    try {
        if (!request) {
            if (response)
                *response = nullptr;
            if (response_size)
                *response_size = 0;
            return 2;
        }
        return gr::binding::respond(
            [&] { return gr::binding::dispatch(gr::binding::parse_request(request, request_size)); },
            response, response_size);

    } catch (...) {
        if (response)
            *response = nullptr;
        if (response_size)
            *response_size = 0;
        return 2;
    }
}
