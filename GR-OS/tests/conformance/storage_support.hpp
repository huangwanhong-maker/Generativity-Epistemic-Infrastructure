#pragma once
#include "gr/storage.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
using gr::Json;
using namespace gr::storage;
namespace {
void check(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void error(const std::string &expected, F &&operation) {
    try {
        operation();
    } catch (const gr::Error &e) {
        check(e.code == expected, "Expected " + expected + ", got " + e.code + ": " + e.what());
        return;
    }
    throw std::runtime_error("Expected failure: " + expected);
}
std::string content_version(const Json &data) {
    return "sha256:" + gr::sha256(gr::canonical(data));
}
Blob material(std::vector<std::uint8_t> data) {
    auto digest = "sha256:" + gr::sha256(std::span<const std::uint8_t>(data.data(), data.size()));
    return {digest, std::move(data)};
}
Commit request(const std::string &transaction, const std::string &head = "0",
               const std::string &workspace = "test") {
    Commit c;
    c.workspace = workspace;
    c.expected_revision = head;
    c.transaction_id = transaction;
    c.actor = "recorder:test";
    c.reason = "Storage contract test";
    return c;
}

std::filesystem::path temporary_folder(const std::string &name) {
    auto folder = std::filesystem::temp_directory_path() /
                  ("gr-os-storage-" + name + "-" +
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(folder), "create isolated test directory");
    return folder;
}
void cleanup(const std::filesystem::path &folder) {
    check(std::filesystem::weakly_canonical(folder).parent_path() ==
              std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()),
          "safe test cleanup parent");
    check(folder.filename().string().starts_with("gr-os-storage-"), "safe test cleanup name");
    std::filesystem::remove_all(folder);
}
} // namespace
