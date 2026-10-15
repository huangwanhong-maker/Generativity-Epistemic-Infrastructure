#pragma once
#include "gr/common.hpp"
#include <atomic>
#include <memory>
#include <span>
#include <stop_token>
#include <vector>

namespace gr::memory {
// Charges requested allocation bytes, not allocator overhead or unrelated heaps.
class AllocationBudget {
  public:
    explicit AllocationBudget(std::size_t limit_bytes);
    std::size_t limit() const noexcept;
    std::size_t used() const noexcept;
    std::size_t peak() const noexcept;

  private:
    friend struct BufferStorage;
    void acquire(std::size_t);
    void release(std::size_t) noexcept;
    std::size_t limit_;
    std::atomic<std::size_t> used_{0}, peak_{0};
};
struct BufferStorage;
class MutableBuffer;
// Copies share immutable host storage. No external writable allocation is adopted.
class Buffer {
  public:
    Buffer() = default;
    static Buffer copy(std::span<const std::uint8_t>, std::size_t alignment = 64,
                       std::shared_ptr<AllocationBudget> = {});
    std::span<const std::uint8_t> bytes() const noexcept;
    std::size_t size() const noexcept;
    std::size_t alignment() const noexcept;
    std::size_t retained_bytes() const noexcept;
    const void *owner_id() const noexcept; // Process-local accounting only; never serialized.
    std::string content_digest() const;
    bool operator==(const Buffer &) const;

  private:
    friend class MutableBuffer;
    explicit Buffer(std::shared_ptr<const BufferStorage>);
    std::shared_ptr<const BufferStorage> storage_;
};
// One scoped, move-only mutable borrow. Raw spans must not outlive this lease.
class MutableView {
  public:
    MutableView() = default;
    ~MutableView();
    MutableView(MutableView &&) noexcept;
    MutableView &operator=(MutableView &&) noexcept;
    MutableView(const MutableView &) = delete;
    MutableView &operator=(const MutableView &) = delete;
    std::span<std::uint8_t> bytes() const noexcept;

  private:
    friend class MutableBuffer;
    MutableView(std::shared_ptr<BufferStorage>, std::size_t, std::size_t);
    void release() noexcept;
    std::shared_ptr<BufferStorage> storage_;
    std::size_t offset_ = 0, size_ = 0;
};
// Zero-initialized, move-only builder. One mutable borrow at a time; freeze
// refuses active borrows. Builder methods require external synchronization.
class MutableBuffer {
  public:
    MutableBuffer() = default;
    static MutableBuffer allocate(std::size_t bytes, std::size_t alignment = 64,
                                  std::shared_ptr<AllocationBudget> = {});
    MutableBuffer(MutableBuffer &&) noexcept = default;
    MutableBuffer &operator=(MutableBuffer &&) noexcept = default;
    MutableBuffer(const MutableBuffer &) = delete;
    MutableBuffer &operator=(const MutableBuffer &) = delete;
    std::size_t size() const noexcept;
    MutableView borrow();
    MutableView borrow(std::size_t offset, std::size_t length);
    Buffer freeze() &&;

  private:
    explicit MutableBuffer(std::shared_ptr<BufferStorage>);
    std::shared_ptr<BufferStorage> storage_;
};
enum class ByteOrder { NotApplicable, Little, Big };
std::string to_string(ByteOrder);
ByteOrder native_byte_order();
struct ElementDescriptor {
    std::string format_id = "gr.opaque-octet@1";
    std::size_t item_size = 1;
    std::size_t alignment = 1;
    ByteOrder byte_order = ByteOrder::NotApplicable;
    Json parameters = Json::object();
    void validate() const;
    Json to_json() const;
    bool operator==(const ElementDescriptor &) const = default;
};
// Physical layout only, not mathematical tensor semantics. Strides are bytes.
class BufferView {
  public:
    BufferView(Buffer, ElementDescriptor, std::vector<std::size_t> shape,
               std::vector<std::ptrdiff_t> byte_strides = {}, std::size_t byte_offset = 0);
    const Buffer &buffer() const noexcept;
    const ElementDescriptor &element() const noexcept;
    const std::vector<std::size_t> &shape() const noexcept;
    const std::vector<std::ptrdiff_t> &strides() const noexcept;
    std::size_t offset() const noexcept;
    std::size_t elements() const noexcept;
    std::size_t logical_bytes() const noexcept;
    std::size_t retained_bytes() const noexcept;
    bool c_contiguous() const noexcept;
    std::span<const std::uint8_t> contiguous_bytes() const;
    std::span<const std::uint8_t> element_bytes(std::span<const std::size_t> indices) const;
    BufferView slice(std::size_t axis, std::size_t start, std::size_t count, std::ptrdiff_t step = 1) const;
    BufferView copy_contiguous(std::shared_ptr<AllocationBudget> = {}, std::stop_token = {}) const;
    Json descriptor() const;
    std::string content_digest() const;
    bool operator==(const BufferView &) const;

  private:
    Buffer buffer_;
    ElementDescriptor element_;
    std::vector<std::size_t> shape_;
    std::vector<std::ptrdiff_t> strides_;
    std::size_t offset_ = 0, elements_ = 0, logical_bytes_ = 0;
};
struct NamedBuffer {
    std::string name;
    BufferView view;
    bool operator==(const NamedBuffer &) const = default;
};
class BufferSet {
  public:
    explicit BufferSet(std::vector<NamedBuffer>, Json metadata = Json::object());
    const std::vector<NamedBuffer> &buffers() const noexcept;
    const BufferView &at(std::string_view name) const;
    const Json &metadata() const noexcept;
    std::size_t logical_bytes() const noexcept;
    std::size_t retained_bytes() const noexcept;
    // max(logical element bytes, distinct retained owner bytes) + metadata/layout bytes.
    std::size_t accounted_bytes() const noexcept;
    Json descriptor() const;
    std::string content_digest() const;
    bool operator==(const BufferSet &) const;

  private:
    std::vector<NamedBuffer> buffers_;
    Json metadata_;
    std::size_t logical_ = 0, retained_ = 0, accounted_ = 0;
};
} // namespace gr::memory
