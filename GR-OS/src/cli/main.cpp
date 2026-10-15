#include "gr/c_api.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

namespace {
constexpr std::size_t limit = 16 * 1024 * 1024;
struct Session {
    std::set<std::string> runtimes;
    ~Session() {
        // Drain while main and native function statics are still alive. A
        // process-global destructor must not be the CLI's worker join boundary.
        for (const auto &handle : runtimes) {
            try {
                const auto request =
                    nlohmann::json{{"abi", 1}, {"op", "runtime.close"}, {"handle", handle}}.dump();
                char *response = nullptr;
                std::size_t size = 0;
                gr_call_json(request.data(), request.size(), &response, &size);
                gr_free(response);
            } catch (...) {
            }
        }
    }
    void observe(const std::string &request, const char *response, std::size_t size) {
        const auto parsed = nlohmann::json::parse(request);
        if (parsed.at("op") == "runtime.open") {
            const auto result = nlohmann::json::parse(response, response + size);
            runtimes.insert(result.at("result").at("handle").get<std::string>());
        } else if (parsed.at("op") == "runtime.close")
            runtimes.erase(parsed.at("handle").get<std::string>());
    }
};
int error(const std::string &code, const std::string &message) {
    std::cout
        << nlohmann::json{{"abi", 1},
                          {"ok", false},
                          {"error",
                           {{"code", code}, {"message", message}, {"details", nlohmann::json::object()}}}}
               .dump()
        << '\n';
    return 2;
}
bool read(std::istream &input, std::string &value, bool line) {
    value.clear();
    char ch;
    bool oversized = false;
    while (input.get(ch)) {
        if (line && ch == '\n')
            break;
        if (value.size() < limit)
            value.push_back(ch);
        else
            oversized = true;
    }
    if (oversized)
        throw std::length_error("Request exceeds 16 MiB");
    if (input.bad())
        throw std::runtime_error("Input read failed");
    return !value.empty() || static_cast<bool>(input);
}
int invoke(const std::string &request, bool runtime_result = false, Session *session = nullptr) {
    char *raw = nullptr;
    std::size_t size = 0;
    const auto status = gr_call_json(request.data(), request.size(), &raw, &size);
    std::unique_ptr<char, decltype(&gr_free)> response(raw, &gr_free);
    if (!raw)
        return error("transport_failure", "Native call did not produce a response");
    if (status == 0 && session)
        session->observe(request, raw, size);
    std::cout.write(raw, static_cast<std::streamsize>(size));
    std::cout << '\n' << std::flush;
    if (status)
        return 1;
    if (runtime_result) {
        const auto envelope = nlohmann::json::parse(raw, raw + size);
        if (envelope.at("result").value("status", std::string{}) != "succeeded")
            return 3;
    }
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    try {
        Session session;
        const std::string command = argc > 1 ? argv[1] : "help";
        if (command == "help" || command == "--help") {
            std::cout
                << R"({"commands":{"capabilities":"SDK capabilities JSON","call":"One ABI 1 JSON request from stdin or --file PATH","run":"One object with graph/config/options from stdin or --file PATH; creates and drains a runtime","session":"Bounded JSONL ABI requests and responses; handles live until close or process exit"},"exit_codes":{"0":"success","1":"native request error","2":"usage/input/transport error","3":"job terminal failure"},"request_bytes":16777216})"
                << '\n';
            return 0;
        }
        if (command == "capabilities") {
            if (argc != 2)
                return error("usage", "capabilities takes no arguments");
            return invoke(R"({"abi":1,"op":"sdk.capabilities"})");
        }
        if (command != "call" && command != "run" && command != "session")
            return error("usage", "Unknown command; use gr help");
        std::ifstream file;
        std::istream *input = &std::cin;
        if (argc == 4 && std::string(argv[2]) == "--file" && command != "session") {
            file.open(argv[3], std::ios::binary);
            if (!file)
                return error("input_error", "Cannot open request file");
            input = &file;
        } else if (argc != 2)
            return error("usage", "Expected command or command --file PATH");
        std::string request;
        if (command == "session") {
            int result = 0;
            while (read(*input, request, true)) {
                const int code = invoke(request, false, &session);
                if (code)
                    result = code;
            }
            return result;
        }
        if (!read(*input, request, false))
            return error("input_error", "Empty request");
        if (command == "run") {
            // Wrap the unmodified JSON text, so duplicate keys and depth remain
            // subject to the same strict native parser as every other client.
            const auto first = request.find_first_not_of(" \r\n\t");
            const auto last = request.find_last_not_of(" \r\n\t");
            if (first == std::string::npos || request[first] != '{' || request[last] != '}')
                return error("invalid_request",
                             "run expects an object containing graph, optional config/options");
            auto body = request.substr(first + 1, last - first - 1);
            const bool empty = body.find_first_not_of(" \r\n\t") == std::string::npos;
            request = std::string(R"({"abi":1,"op":"runtime.evaluate")") + (empty ? "" : ",") + body + "}";
        }
        return invoke(request, command == "run", &session);
    } catch (const std::length_error &e) {
        return error("request_limit", e.what());
    } catch (const std::exception &e) {
        return error("input_error", e.what());
    } catch (...) {
        return error("transport_failure", "Unknown native failure");
    }
}
