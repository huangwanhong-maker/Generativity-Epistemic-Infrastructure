#include "gr/common.hpp"
#include <picosha2.h>
#include <set>
#include <vector>

namespace gr {
Error::Error(std::string c, std::string message, Json d)
    : std::runtime_error(std::move(message)), code(std::move(c)), details(std::move(d)) {}
namespace {
void check(const Json &j, unsigned depth, std::size_t &count, std::size_t &text_bytes) {
    if (depth > 64 || ++count > 100000)
        throw Error("value_limit", "Metadata nesting or value count exceeds gr.canonical-json/1 bounds");
    if (j.is_number_float() || j.is_binary() || j.is_discarded())
        throw Error("unsupported_value",
                    "Use safe integers, typed strings or artifact bytes; floating metadata is not supported");
    if (j.is_number_unsigned() && j.get<std::uint64_t>() > 9007199254740991ULL)
        throw Error("integer_range", "Integer exceeds exact interoperable range");
    if (j.is_number_integer() && !j.is_number_unsigned()) {
        auto v = j.get<std::int64_t>();
        if (v < -9007199254740991LL || v > 9007199254740991LL)
            throw Error("integer_range", "Integer exceeds exact interoperable range");
    }
    if (j.is_string())
        text_bytes += j.get_ref<const std::string &>().size();
    if (text_bytes > 1024 * 1024)
        throw Error("value_limit", "Metadata text exceeds 1 MiB");
    if (j.is_structured())
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (j.is_object())
                text_bytes += it.key().size();
            check(it.value(), depth + 1, count, text_bytes);
        }
}
} // namespace
std::string canonical(const Json &value) {
    std::size_t count = 0, bytes = 0;
    check(value, 0, count, bytes);
    try {
        auto result = value.dump(-1, ' ', false, Json::error_handler_t::strict);
        if (result.size() > 1024 * 1024)
            throw Error("value_limit", "Canonical metadata exceeds 1 MiB");
        return result;
    } catch (const Json::exception &e) {
        throw Error("invalid_json", e.what());
    }
}
std::string sha256(std::string_view bytes) {
    return picosha2::hash256_hex_string(bytes.begin(), bytes.end());
}
Json parse_metadata(std::string_view utf8) {
    if (utf8.size() > 1024 * 1024)
        throw Error("value_limit", "Metadata input exceeds 1 MiB");
    std::vector<std::set<std::string>> keys;
    std::size_t values = 0;
    try {
        auto result = Json::parse(
            utf8.begin(), utf8.end(), [&keys, &values](int depth, Json::parse_event_t event, Json &parsed) {
                if (depth > 64)
                    throw Error("value_limit", "Metadata nesting exceeds 64");
                if (event == Json::parse_event_t::object_start)
                    keys.emplace_back();
                else if (event == Json::parse_event_t::key &&
                         !keys.back().insert(parsed.get<std::string>()).second)
                    throw Error("duplicate_key", "Duplicate metadata object key");
                else if (event == Json::parse_event_t::object_end)
                    keys.pop_back();
                if ((event == Json::parse_event_t::value || event == Json::parse_event_t::object_end ||
                     event == Json::parse_event_t::array_end) &&
                    ++values > 100000)
                    throw Error("value_limit", "Metadata value count exceeds 100000");
                return true;
            });
        (void)canonical(result);
        return result;
    } catch (const Json::exception &e) {
        throw Error("invalid_json", e.what());
    }
}
std::string sha256(std::span<const std::uint8_t> bytes) {
    return picosha2::hash256_hex_string(bytes.begin(), bytes.end());
}
} // namespace gr
