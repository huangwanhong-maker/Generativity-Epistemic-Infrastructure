#include "gr/support/common.hpp"
#include <fstream>
#include <iostream>
#include <random>

int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Fixture path required");
        std::ifstream file(argv[1]);
        if (!file)
            throw std::runtime_error("Cannot open canonical fixture");
        auto fixture = gr::Json::parse(file);
        std::size_t checked = 0;
        for (const auto &test : fixture.at("accepted")) {
            const auto decoded = gr::parse_metadata(test.at("input").get<std::string>());
            const auto encoded = gr::canonical(decoded);
            if (encoded != test.at("canonical").get<std::string>() ||
                gr::sha256(encoded) != test.at("sha256").get<std::string>())
                throw std::runtime_error("Canonical vector failed: " + test.at("name").get<std::string>());
            ++checked;
        }
        for (const auto &test : fixture.at("rejected")) {
            bool rejected = false;
            try {
                (void)gr::parse_metadata(test.at("input").get<std::string>());
            } catch (const gr::Error &) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Accepted invalid canonical input: " +
                                         test.at("name").get<std::string>());
            ++checked;
        }
        std::mt19937 generator(20261013);
        for (int trial = 0; trial < 200; ++trial) {
            gr::Json value = gr::Json::object();
            for (int item = 0; item < 20; ++item)
                value["key:" + std::to_string(generator() % 31)] =
                    static_cast<std::int64_t>(generator()) - 2147483648LL;
            auto bytes = gr::canonical(value);
            if (gr::canonical(gr::parse_metadata(bytes)) != bytes)
                throw std::runtime_error("Seeded canonical fixed-point failed");
        }
        auto reject = [](std::string input) {
            try {
                (void)gr::parse_metadata(input);
            } catch (const gr::Error &) {
                return;
            }
            throw std::runtime_error("Metadata resource boundary was not enforced");
        };
        reject(std::string(66, '[') + "0" + std::string(66, ']'));
        reject("\"" + std::string(1024 * 1024, 'x') + "\"");
        reject(std::string("\"") + static_cast<char>(0xff) + "\"");
        std::string many = "[0";
        for (int i = 0; i < 100000; ++i)
            many += ",0";
        many += ']';
        reject(many);
        std::cout << checked << " canonical vectors and 200 seeded fixed points passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
