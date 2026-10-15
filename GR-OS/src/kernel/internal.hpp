#pragma once
#include "gr/kernel/projections.hpp"
#include "gr/kernel/records.hpp"
namespace gr::core::detail {
void require(bool condition, const char *code, const std::string &message);
std::string text(const Json &value, const char *key);
bool digest(const std::string &value);
void array_field(const Json &value, const char *key);
const Json &records(const Json &snapshot);
struct Index {
    std::map<std::string, const Json *> current;
    std::map<std::pair<std::string, std::string>, const Json *> versions;
};
Index index(const Json &snapshot, ValidationSession *session = nullptr);
void require_valid(const Json &snapshot, const TypeRegistry &registry);
} // namespace gr::core::detail
