#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gr {
using Json = nlohmann::json;
class Error : public std::runtime_error {
  public:
    std::string code;
    Json details;
    Error(std::string code, std::string message, Json details = Json::object());
};
// gr.canonical-json/1: UTF-8, sorted object keys, safe integers only, no normalization.
// Bounded metadata: depth <= 64, <= 1 MiB serialized, <= 100000 values.
std::string canonical(const Json &value);
// Strict metadata decoding: duplicate keys, unsafe numbers and malformed UTF-8
// are rejected before a value is returned. Input and canonical output <= 1 MiB.
// Parsing changes neither the gr.canonical-json/1 encoding nor its digest domain.
Json parse_metadata(std::string_view utf8);
std::string sha256(std::string_view bytes);
std::string sha256(std::span<const std::uint8_t> bytes);
} // namespace gr
