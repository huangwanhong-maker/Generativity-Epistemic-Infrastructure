#include "gr/memory/buffer.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <thread>

using namespace gr;
using namespace gr::memory;
namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void fails(const char *code, F function) {
    try {
        function();
    } catch (const Error &error) {
        if (error.code != code)
            throw std::runtime_error(std::string("Expected ") + code + ", got " + error.code);
        ++checks;
        return;
    }
    throw std::runtime_error(std::string("Missing error: ") + code);
}
Buffer sequence(std::size_t n, std::shared_ptr<AllocationBudget> budget = {}) {
    auto builder = MutableBuffer::allocate(n, 64, std::move(budget));
    {
        auto view = builder.borrow();
        for (std::size_t i = 0; i < n; ++i)
            view.bytes()[i] = static_cast<std::uint8_t>(i);
    }
    return std::move(builder).freeze();
}
void ownership() {
    auto budget = std::make_shared<AllocationBudget>(128);
    auto builder = MutableBuffer::allocate(128, 64, budget);
    check(budget->used() == 128 && budget->peak() == 128, "Allocation charged once");
    auto lease = builder.borrow();
    check(lease.bytes()[0] == 0 && lease.bytes()[127] == 0, "No uninitialized exported padding");
    lease.bytes()[0] = 42;
    fails("buffer_borrow", [&] { builder.borrow(); });
    fails("buffer_borrow", [&] { std::move(builder).freeze(); });
    auto moved = std::move(lease);
    check(lease.bytes().empty() && moved.bytes()[0] == 42, "Mutable lease moves without duplication");
    moved = {};
    auto buffer = std::move(builder).freeze();
    check(buffer.bytes()[0] == 42 && buffer.alignment() >= 64, "Frozen owner retains initialized bytes");
    check(reinterpret_cast<std::uintptr_t>(buffer.bytes().data()) % 64 == 0, "Actual allocation alignment");
    fails("buffer_builder", [&] { builder.borrow(); });
    fails("buffer_budget", [&] { MutableBuffer::allocate(1, 64, budget); });
    auto copy = buffer;
    buffer = {};
    check(budget->used() == 128 && copy.bytes()[0] == 42, "Copied immutable owner retains budget");
    copy = {};
    check(budget->used() == 0, "Final owner releases allocation charge");
    auto orphan_builder = MutableBuffer::allocate(32, 16, budget);
    auto orphan = orphan_builder.borrow();
    orphan_builder = {};
    check(budget->used() == 32, "Mutable lease survives builder destruction");
    orphan.bytes()[31] = 9;
    orphan = {};
    check(budget->used() == 0, "Orphan mutable lease releases allocation");
    fails("buffer_alignment", [] { MutableBuffer::allocate(0, 3); });
    fails("buffer_alignment", [] { MutableBuffer::allocate(1, 8192); });
    fails("buffer_size", [] { MutableBuffer::allocate(std::numeric_limits<std::size_t>::max()); });
    check(MutableBuffer::allocate(0, 64, std::make_shared<AllocationBudget>(0)).size() == 0,
          "Zero allocation fits zero budget");
}
void layouts() {
    auto owner = sequence(16);
    BufferView matrix(owner, {}, {4, 4});
    check(matrix.c_contiguous() && matrix.elements() == 16, "Row-major layout inferred");
    const std::array<std::size_t, 2> index{2, 3};
    check(matrix.element_bytes(index)[0] == 11, "Element coordinates map to checked bytes");
    auto reverse = matrix.slice(0, 3, 4, -1);
    check(reverse.offset() == 12 && reverse.strides()[0] == -4 && !reverse.c_contiguous(),
          "Negative stride view retains layout");
    const auto copied = reverse.copy_contiguous();
    check(copied.contiguous_bytes()[0] == 12 && copied.contiguous_bytes()[15] == 3,
          "Noncontiguous copy follows logical element order");
    check(copied.buffer().owner_id() != owner.owner_id(), "Explicit copy owns another allocation");
    auto skipped = matrix.slice(1, 0, 2, 2).copy_contiguous();
    check(skipped.logical_bytes() == 8 && skipped.contiguous_bytes()[7] == 14, "Stepped slice copy");
    BufferView transpose(owner, {}, {4, 4}, {1, 4});
    check(transpose.copy_contiguous().contiguous_bytes()[1] == 4, "Transposed layout copy");
    BufferView broadcast(owner, {}, {1024}, {0});
    check(broadcast.logical_bytes() == 1024 && broadcast.retained_bytes() == 16,
          "Broadcast logical volume differs from ownership");
    BufferSet set({{"left", matrix}, {"right", matrix}});
    check(set.logical_bytes() == 32 && set.retained_bytes() == 16, "Aliased owners counted once per set");
    check(set.accounted_bytes() > set.logical_bytes(), "Descriptor memory charged conservatively");
    BufferSet tiny({{"one", matrix.slice(0, 0, 1).slice(1, 0, 1)}});
    check(tiny.logical_bytes() == 1 && tiny.retained_bytes() == 16, "Tiny slice retains whole owner");
    BufferSet expanded({{"repeat", broadcast}});
    check(expanded.accounted_bytes() > 1024, "Broadcast cannot bypass logical payload cap");
    BufferView empty(owner, {}, {0}, {}, 16);
    check(empty.c_contiguous() && empty.contiguous_bytes().empty(), "Empty view may point one past owner");
    BufferView scalar(owner, {}, {}, {}, 7);
    check(scalar.elements() == 1 && scalar.element_bytes({})[0] == 7, "Rank-zero scalar has one element");
    ElementDescriptor word{"gr.int64@1", 8, 8, ByteOrder::Big};
    BufferView words(owner, word, {2});
    auto words_copy = words.copy_contiguous();
    check(words_copy.element().byte_order == ByteOrder::Big && words_copy.contiguous_bytes()[1] == 1,
          "Layout copy performs no implicit byte swap");
    check(set.content_digest() == BufferSet({{"left", BufferView(sequence(16), {}, {4, 4})},
                                             {"right", BufferView(sequence(16), {}, {4, 4})}})
                                      .content_digest(),
          "Content identity excludes process address and owner sharing topology");
    check(set.descriptor().dump().find("owner_id") == std::string::npos, "No addresses in portable metadata");
    check(owner.content_digest() == sequence(16).content_digest(), "Stable immutable allocation digest");
    fails("buffer_layout", [&] { transpose.contiguous_bytes(); });
    fails("buffer_bounds", [&] { BufferView(owner, {}, {17}); });
    fails("buffer_bounds", [&] { BufferView(owner, {}, {2}, {-1}, 0); });
    fails("buffer_alignment", [&] { BufferView(owner, word, {1}, {}, 1); });
    fails("buffer_alignment", [&] { BufferView(owner, word, {2}, {3}); });
    fails("buffer_rank", [&] { BufferView(owner, {}, {2}, {1, 1}); });
    fails("buffer_stride", [&] { BufferView(owner, {}, {0}, {std::numeric_limits<std::ptrdiff_t>::min()}); });
    fails("buffer_overflow", [&] {
        BufferView(owner, {}, {static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()), 3});
    });
    fails("buffer_slice", [&] { matrix.slice(0, 0, 2, -1); });
    fails("buffer_slice", [&] { matrix.slice(0, 0, 1, 0); });
    fails("buffer_slice", [&] { matrix.slice(0, 4, 1); });
    fails("buffer_set", [&] { BufferSet({{"same", matrix}, {"same", matrix}}); });
    fails("buffer_name", [&] { set.at("missing"); });
    std::stop_source stop;
    stop.request_stop();
    auto budget = std::make_shared<AllocationBudget>(100);
    fails("execution_cancelled", [&] { transpose.copy_contiguous(budget, stop.get_token()); });
    check(budget->used() == 0, "Cancelled copy acquires no new owner");
}
void concurrent_budget() {
    auto budget = std::make_shared<AllocationBudget>(64);
    std::vector<std::jthread> threads;
    std::atomic<int> errors{0};
    for (int i = 0; i < 8; ++i)
        threads.emplace_back([&] {
            try {
                for (int j = 0; j < 100; ++j) {
                    auto owner = sequence(8, budget);
                    auto shared = owner;
                    if (shared.bytes()[7] != 7)
                        ++errors;
                }
            } catch (...) {
                ++errors;
            }
        });
    threads.clear();
    check(errors == 0 && budget->used() == 0 && budget->peak() <= 64,
          "Concurrent independent owners share one enforced allocation budget");
}
} // namespace
int main() {
    try {
        ownership();
        layouts();
        concurrent_budget();
        std::cout << "buffers: " << checks << " checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << "buffers failure: " << error.what() << '\n';
        return 1;
    }
}
