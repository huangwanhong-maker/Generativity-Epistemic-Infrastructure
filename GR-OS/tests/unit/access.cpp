#include "gr/memory/access.hpp"
#include <array>
#include <iostream>
#include <optional>

using namespace gr;
using namespace gr::memory;
namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F fn) {
    try {
        fn();
    } catch (const Error &e) {
        check(e.code == code, "Unexpected error");
        return;
    }
    throw std::runtime_error("Expected failure");
}
} // namespace
int main() {
    try {
        auto budget = std::make_shared<AllocationBudget>(48);
        std::optional<ElementRange> retained;
        {
            auto builder = MutableBuffer::allocate(48, 64, budget);
            {
                auto bytes = builder.borrow();
                for (std::size_t i = 0; i < 6; ++i)
                    store_little(bytes.bytes().subspan(i * 8, 8), std::uint64_t(11 + i));
            }
            const ElementDescriptor e{"fixture.u64@1", 8, 8, ByteOrder::Little};
            BufferView forward(std::move(builder).freeze(), e, {6});
            for (const auto &view :
                 {forward, forward.slice(0, 5, 3, -2), BufferView(forward.buffer(), e, {9}, {0}, 16),
                  forward.slice(0, 6, 0), forward.slice(0, 1, 1, -1)}) {
                ElementRange range(view);
                std::size_t visited = 0;
                range.for_each([&](std::size_t i, auto bytes) {
                    const std::array<std::size_t, 1> position{i};
                    check(load_little<std::uint64_t>(bytes) ==
                              load_little<std::uint64_t>(view.element_bytes(position)),
                          "Batch/scalar byte access differs");
                    check(range.at(i).data() == bytes.data(), "Random and sequential access differ");
                    ++visited;
                });
                check(visited == range.size(), "Batch visit count");
                fails("buffer_bounds", [&] { (void)range.at(range.size()); });
                if (range.contiguous())
                    check(range.contiguous_bytes().size() == range.size() * 8, "Contiguous byte extent");
                else
                    fails("buffer_layout", [&] { (void)range.contiguous_bytes(); });
            }
            retained.emplace(forward.slice(0, 5, 6, -1));
            fails("buffer_rank", [&] { ElementRange rank0(BufferView(forward.buffer(), e, {})); });
        }
        check(budget->used() == 48 && load_little<std::uint64_t>(retained->at(0)) == 16,
              "Range retains owner after source view closes");
        retained.reset();
        check(budget->used() == 0, "Range releases final allocation budget");
        std::array<std::uint8_t, 9> unaligned{};
        store_little(std::span(unaligned).subspan(1), std::uint64_t(0xfedcba9876543210ULL));
        check(load_little<std::uint64_t>(std::span(unaligned).subspan(1)) == 0xfedcba9876543210ULL,
              "Physical load uses memcpy, not an aligned typed reinterpretation");
        fails("buffer_element", [&] { (void)load_little<std::uint64_t>(std::span(unaligned)); });
        std::cout << "access: " << checks << " checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
