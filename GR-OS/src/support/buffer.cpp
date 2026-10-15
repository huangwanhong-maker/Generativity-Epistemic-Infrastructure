#include "gr/memory/buffer.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <set>

namespace gr::memory {
namespace {
constexpr auto address_limit = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
void need(bool ok, const char *code, const char *message) {
    if (!ok)
        throw Error(code, message);
}
std::size_t add(std::size_t a, std::size_t b) {
    need(b <= std::numeric_limits<std::size_t>::max() - a, "buffer_overflow",
         "Buffer size addition overflow.");
    return a + b;
}
std::size_t multiply(std::size_t a, std::size_t b) {
    need(!a || b <= std::numeric_limits<std::size_t>::max() / a, "buffer_overflow",
         "Buffer size multiplication overflow.");
    return a * b;
}
std::size_t magnitude(std::ptrdiff_t value) {
    need(value != std::numeric_limits<std::ptrdiff_t>::min(), "buffer_stride",
         "Minimum signed stride is outside the supported address range.");
    return static_cast<std::size_t>(value < 0 ? -value : value);
}
bool power_of_two(std::size_t value) {
    return value && !(value & (value - 1));
}
void alignment_valid(std::size_t alignment) {
    need(power_of_two(alignment) && alignment <= 4096, "buffer_alignment",
         "Alignment must be a power of two between 1 and 4096.");
}
void active(std::stop_token stop) {
    if (stop.stop_requested())
        throw Error("execution_cancelled", "Buffer copy was cancelled.");
}
std::size_t shifted(std::size_t offset, std::ptrdiff_t stride, std::size_t index) {
    const auto distance = multiply(magnitude(stride), index);
    if (stride < 0) {
        need(distance <= offset, "buffer_bounds", "Negative stride precedes its buffer.");
        return offset - distance;
    }
    return add(offset, distance);
}
} // namespace

AllocationBudget::AllocationBudget(std::size_t limit_bytes) : limit_(limit_bytes) {}
std::size_t AllocationBudget::limit() const noexcept {
    return limit_;
}
std::size_t AllocationBudget::used() const noexcept {
    return used_.load();
}
std::size_t AllocationBudget::peak() const noexcept {
    return peak_.load();
}
void AllocationBudget::acquire(std::size_t amount) {
    auto old = used_.load();
    do {
        need(amount <= limit_ - old, "buffer_budget", "Owned buffer allocation budget exhausted.");
    } while (!used_.compare_exchange_weak(old, old + amount));
    auto peak = peak_.load();
    while (peak < old + amount && !peak_.compare_exchange_weak(peak, old + amount)) {
    }
}
void AllocationBudget::release(std::size_t amount) noexcept {
    used_.fetch_sub(amount);
}

struct BufferStorage {
    std::uint8_t *data = nullptr;
    std::size_t size, alignment;
    std::shared_ptr<AllocationBudget> budget;
    std::atomic<bool> borrowed{false};
    mutable std::once_flag hash_once;
    mutable std::string digest;
    BufferStorage(std::size_t bytes, std::size_t requested_alignment, std::shared_ptr<AllocationBudget> b)
        : size(bytes), alignment(std::max(requested_alignment, alignof(std::max_align_t))),
          budget(std::move(b)) {
        alignment_valid(requested_alignment);
        need(bytes <= address_limit, "buffer_size", "Allocation exceeds the supported host address range.");
        if (budget)
            budget->acquire(size);
        try {
            if (size) {
                data = static_cast<std::uint8_t *>(::operator new(size, std::align_val_t(alignment)));
                std::memset(data, 0, size);
            }
        } catch (...) {
            if (budget)
                budget->release(size);
            throw;
        }
    }
    ~BufferStorage() {
        if (data)
            ::operator delete(data, std::align_val_t(alignment));
        if (budget)
            budget->release(size);
    }
};
Buffer::Buffer(std::shared_ptr<const BufferStorage> storage) : storage_(std::move(storage)) {}
Buffer Buffer::copy(std::span<const std::uint8_t> data, std::size_t alignment,
                    std::shared_ptr<AllocationBudget> budget) {
    auto builder = MutableBuffer::allocate(data.size(), alignment, std::move(budget));
    {
        auto view = builder.borrow();
        std::copy(data.begin(), data.end(), view.bytes().begin());
    }
    return std::move(builder).freeze();
}
std::span<const std::uint8_t> Buffer::bytes() const noexcept {
    return storage_ ? std::span<const std::uint8_t>(storage_->data, storage_->size)
                    : std::span<const std::uint8_t>();
}
std::size_t Buffer::size() const noexcept {
    return storage_ ? storage_->size : 0;
}
std::size_t Buffer::alignment() const noexcept {
    return storage_ ? storage_->alignment : 1;
}
std::size_t Buffer::retained_bytes() const noexcept {
    return size();
}
const void *Buffer::owner_id() const noexcept {
    return storage_.get();
}
std::string Buffer::content_digest() const {
    if (!storage_)
        return "sha256:" + sha256(std::span<const std::uint8_t>());
    std::call_once(storage_->hash_once, [&] { storage_->digest = "sha256:" + sha256(bytes()); });
    return storage_->digest;
}
bool Buffer::operator==(const Buffer &other) const {
    return storage_ == other.storage_ ||
           (size() == other.size() && std::equal(bytes().begin(), bytes().end(), other.bytes().begin()));
}
MutableView::MutableView(std::shared_ptr<BufferStorage> storage, std::size_t offset, std::size_t size)
    : storage_(std::move(storage)), offset_(offset), size_(size) {}
MutableView::~MutableView() {
    release();
}
MutableView::MutableView(MutableView &&other) noexcept
    : storage_(std::move(other.storage_)), offset_(other.offset_), size_(other.size_) {}
MutableView &MutableView::operator=(MutableView &&other) noexcept {
    if (this != &other) {
        release();
        storage_ = std::move(other.storage_);
        offset_ = other.offset_;
        size_ = other.size_;
    }
    return *this;
}
void MutableView::release() noexcept {
    if (storage_) {
        storage_->borrowed.store(false);
        storage_.reset();
    }
}
std::span<std::uint8_t> MutableView::bytes() const noexcept {
    if (!storage_ || !storage_->size)
        return {};
    return {storage_->data + offset_, size_};
}
MutableBuffer::MutableBuffer(std::shared_ptr<BufferStorage> storage) : storage_(std::move(storage)) {}
MutableBuffer MutableBuffer::allocate(std::size_t bytes, std::size_t alignment,
                                      std::shared_ptr<AllocationBudget> budget) {
    return MutableBuffer(std::make_shared<BufferStorage>(bytes, alignment, std::move(budget)));
}
std::size_t MutableBuffer::size() const noexcept {
    return storage_ ? storage_->size : 0;
}
MutableView MutableBuffer::borrow() {
    return borrow(0, size());
}
MutableView MutableBuffer::borrow(std::size_t offset, std::size_t length) {
    need(bool(storage_), "buffer_builder", "Buffer builder is empty or already frozen.");
    need(offset <= size() && length <= size() - offset, "buffer_bounds", "Mutable view is out of bounds.");
    need(!storage_->borrowed.exchange(true), "buffer_borrow", "A mutable borrow is already active.");
    return MutableView(storage_, offset, length);
}
Buffer MutableBuffer::freeze() && {
    need(bool(storage_), "buffer_builder", "Buffer builder is empty or already frozen.");
    need(!storage_->borrowed.load(), "buffer_borrow", "Release mutable views before freezing.");
    return Buffer(std::move(storage_));
}
std::string to_string(ByteOrder order) {
    switch (order) {
    case ByteOrder::NotApplicable:
        return "not_applicable";
    case ByteOrder::Little:
        return "little";
    case ByteOrder::Big:
        return "big";
    }
    throw Error("buffer_endian", "Unknown byte order.");
}
ByteOrder native_byte_order() {
    if constexpr (std::endian::native == std::endian::little) {
        return ByteOrder::Little;
    } else if constexpr (std::endian::native == std::endian::big) {
        return ByteOrder::Big;
    } else {
        throw Error("buffer_endian", "Mixed-endian host is unsupported.");
    }
}
void ElementDescriptor::validate() const {
    const auto at = format_id.find('@');
    need(!format_id.empty() && format_id.size() <= 256 && at != std::string::npos && at != 0 &&
             at + 1 < format_id.size() && format_id.find('@', at + 1) == std::string::npos,
         "buffer_format", "Physical encoding needs a bounded qualified versioned identity.");
    const auto name = std::string_view(format_id).substr(0, at);
    const auto version = std::string_view(format_id).substr(at + 1);
    need(name.find('.') != std::string_view::npos && name.front() != '.' && name.back() != '.' &&
             name.find("..") == std::string_view::npos && version.size() <= 64 && version.front() != '.' &&
             version.back() != '.' && version.find("..") == std::string_view::npos,
         "buffer_format", "Physical encoding identity has malformed namespace or version components.");
    for (const auto c : format_id)
        need((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                 c == '_' || c == '-' || c == '@',
             "buffer_format", "Physical encoding identity contains an unsupported character.");
    need(item_size > 0 && item_size <= 1024 * 1024, "buffer_item", "Element size must be 1..1048576 bytes.");
    alignment_valid(alignment);
    need(item_size % alignment == 0, "buffer_alignment", "Element size must preserve declared alignment.");
    need(parameters.is_object(), "buffer_format", "Physical encoding parameters must be an object.");
    need(canonical(to_json()).size() <= 65536, "buffer_format", "Physical encoding descriptor is too large.");
}
Json ElementDescriptor::to_json() const {
    return {{"format_id", format_id},
            {"item_size", item_size},
            {"alignment", alignment},
            {"byte_order", to_string(byte_order)},
            {"parameters", parameters}};
}
BufferView::BufferView(Buffer buffer, ElementDescriptor element, std::vector<std::size_t> shape,
                       std::vector<std::ptrdiff_t> strides, std::size_t offset)
    : buffer_(std::move(buffer)), element_(std::move(element)), shape_(std::move(shape)),
      strides_(std::move(strides)), offset_(offset) {
    element_.validate();
    need(shape_.size() <= 64, "buffer_rank", "At most 64 physical dimensions are supported.");
    need(offset_ <= buffer_.size(), "buffer_bounds", "View offset is outside its owner.");
    for (auto dimension : shape_)
        need(dimension <= address_limit, "buffer_shape", "Dimension exceeds the host index range.");
    const bool empty = std::find(shape_.begin(), shape_.end(), 0) != shape_.end();
    elements_ = empty ? 0 : 1;
    if (!empty)
        for (auto dimension : shape_)
            elements_ = multiply(elements_, dimension);
    logical_bytes_ = multiply(elements_, element_.item_size);
    if (strides_.empty() && !shape_.empty()) {
        strides_.resize(shape_.size());
        std::size_t stride = element_.item_size;
        for (std::size_t axis = shape_.size(); axis-- > 0;) {
            need(stride <= address_limit, "buffer_stride", "Contiguous stride exceeds host address range.");
            strides_[axis] = static_cast<std::ptrdiff_t>(stride);
            stride = multiply(stride, shape_[axis]);
        }
    }
    need(strides_.size() == shape_.size(), "buffer_rank", "Shape and byte strides need the same rank.");
    std::size_t before = 0, after = 0;
    for (std::size_t axis = 0; axis < shape_.size(); ++axis) {
        const auto distance = magnitude(strides_[axis]);
        need(shape_[axis] <= 1 || distance % element_.alignment == 0, "buffer_alignment",
             "Stride does not preserve declared alignment.");
        if (!empty) {
            const auto extent = multiply(distance, shape_[axis] - 1);
            if (strides_[axis] < 0)
                before = add(before, extent);
            else
                after = add(after, extent);
        }
    }
    if (!empty) {
        need(before <= offset_, "buffer_bounds", "Negative strides precede allocation.");
        const auto available = buffer_.size() - offset_;
        need(after <= available && element_.item_size <= available - after, "buffer_bounds",
             "View's reachable elements exceed allocation.");
        need(reinterpret_cast<std::uintptr_t>(buffer_.bytes().data() + offset_) % element_.alignment == 0,
             "buffer_alignment", "View origin violates declared alignment.");
    }
}
const Buffer &BufferView::buffer() const noexcept {
    return buffer_;
}
const ElementDescriptor &BufferView::element() const noexcept {
    return element_;
}
const std::vector<std::size_t> &BufferView::shape() const noexcept {
    return shape_;
}
const std::vector<std::ptrdiff_t> &BufferView::strides() const noexcept {
    return strides_;
}
std::size_t BufferView::offset() const noexcept {
    return offset_;
}
std::size_t BufferView::elements() const noexcept {
    return elements_;
}
std::size_t BufferView::logical_bytes() const noexcept {
    return logical_bytes_;
}
std::size_t BufferView::retained_bytes() const noexcept {
    return buffer_.retained_bytes();
}
bool BufferView::c_contiguous() const noexcept {
    if (!elements_)
        return true;
    std::size_t expected = element_.item_size;
    for (std::size_t axis = shape_.size(); axis-- > 0;) {
        if (shape_[axis] > 1 && (strides_[axis] < 0 || static_cast<std::size_t>(strides_[axis]) != expected))
            return false;
        expected *= shape_[axis]; // Constructor checked the complete logical product.
    }
    return true;
}
std::span<const std::uint8_t> BufferView::contiguous_bytes() const {
    need(c_contiguous(), "buffer_layout", "This view is not C contiguous.");
    if (!logical_bytes_)
        return {};
    return buffer_.bytes().subspan(offset_, logical_bytes_);
}
std::span<const std::uint8_t> BufferView::element_bytes(std::span<const std::size_t> indices) const {
    need(indices.size() == shape_.size(), "buffer_rank", "Element index rank differs from shape.");
    // Accumulate negative/positive offsets separately: valid mixed strides must
    // not be rejected because an arbitrary intermediate axis temporarily exceeds an edge.
    std::size_t before = 0, after = 0;
    for (std::size_t axis = 0; axis < indices.size(); ++axis) {
        need(indices[axis] < shape_[axis], "buffer_bounds", "Element index is outside shape.");
        const auto distance = multiply(indices[axis], magnitude(strides_[axis]));
        if (strides_[axis] < 0)
            before = add(before, distance);
        else
            after = add(after, distance);
    }
    return buffer_.bytes().subspan(offset_ - before + after, element_.item_size);
}
BufferView BufferView::slice(std::size_t axis, std::size_t start, std::size_t count,
                             std::ptrdiff_t step) const {
    need(axis < shape_.size() && step != 0, "buffer_slice", "Slice needs an existing axis and nonzero step.");
    const auto step_size = magnitude(step);
    need(count ? start < shape_[axis] : start <= shape_[axis], "buffer_slice", "Slice starts outside axis.");
    if (count) {
        const auto distance = multiply(count - 1, step_size);
        need(step < 0 ? distance <= start : distance < shape_[axis] - start, "buffer_slice",
             "Slice endpoint is outside axis.");
    }
    auto shape = shape_;
    auto strides = strides_;
    shape[axis] = count;
    const auto stride_size = multiply(magnitude(strides[axis]), step_size);
    need(stride_size <= address_limit, "buffer_stride", "Slice stride exceeds signed host range.");
    strides[axis] = static_cast<std::ptrdiff_t>(stride_size);
    if ((strides_[axis] < 0) != (step < 0))
        strides[axis] = -strides[axis];
    const auto offset = count && elements_ ? shifted(offset_, strides_[axis], start) : offset_;
    return BufferView(buffer_, element_, std::move(shape), std::move(strides), offset);
}
BufferView BufferView::copy_contiguous(std::shared_ptr<AllocationBudget> budget, std::stop_token stop) const {
    active(stop);
    auto builder = MutableBuffer::allocate(logical_bytes_, std::max(std::size_t(64), element_.alignment),
                                           std::move(budget));
    {
        auto writable = builder.borrow();
        auto output = writable.bytes();
        if (c_contiguous()) {
            auto input = contiguous_bytes();
            for (std::size_t offset = 0; offset < input.size();) {
                active(stop);
                const auto length = std::min(std::size_t(65536), input.size() - offset);
                std::memcpy(output.data() + offset, input.data() + offset, length);
                offset += length;
            }
        } else {
            std::vector<std::size_t> indices(shape_.size());
            const auto poll_elements = std::max(std::size_t(1), std::size_t(65536) / element_.item_size);
            for (std::size_t item = 0; item < elements_; ++item) {
                if (item % poll_elements == 0)
                    active(stop);
                const auto input = element_bytes(indices);
                std::memcpy(output.data() + item * element_.item_size, input.data(), element_.item_size);
                for (std::size_t axis = shape_.size(); axis-- > 0;) {
                    if (++indices[axis] < shape_[axis])
                        break;
                    indices[axis] = 0;
                }
            }
        }
    }
    active(stop);
    return BufferView(std::move(builder).freeze(), element_, shape_);
}
Json BufferView::descriptor() const {
    return {{"element", element_.to_json()},
            {"shape", shape_},
            {"strides", strides_},
            {"offset", offset_},
            {"logical_bytes", logical_bytes_},
            {"owner_bytes", buffer_.size()},
            {"memory_domain", "host"},
            {"readonly", true},
            {"c_contiguous", c_contiguous()}};
}
std::string BufferView::content_digest() const {
    return "sha256:" + sha256(canonical(Json{{"format", "gr.buffer-view/1"},
                                             {"view", descriptor()},
                                             {"owner_sha256", buffer_.content_digest()}}));
}
bool BufferView::operator==(const BufferView &other) const {
    return element_ == other.element_ && shape_ == other.shape_ && strides_ == other.strides_ &&
           offset_ == other.offset_ && buffer_ == other.buffer_;
}
BufferSet::BufferSet(std::vector<NamedBuffer> buffers, Json metadata)
    : buffers_(std::move(buffers)), metadata_(std::move(metadata)) {
    need(buffers_.size() <= 256, "buffer_set", "At most 256 named buffers are supported per value.");
    need(metadata_.is_object() && canonical(metadata_).size() <= 65536, "buffer_set",
         "Buffer-set metadata must be a bounded object.");
    std::set<std::string> names;
    std::set<const void *> owners;
    for (const auto &buffer : buffers_) {
        need(!buffer.name.empty() && buffer.name.size() <= 256 &&
                 buffer.name.find('\0') == std::string::npos && names.insert(buffer.name).second,
             "buffer_set", "Buffer names must be bounded and distinct.");
        logical_ = add(logical_, buffer.view.logical_bytes());
        if (owners.insert(buffer.view.buffer().owner_id()).second)
            retained_ = add(retained_, buffer.view.retained_bytes());
    }
    accounted_ = add(std::max(logical_, retained_), canonical(descriptor()).size());
}
const std::vector<NamedBuffer> &BufferSet::buffers() const noexcept {
    return buffers_;
}
const BufferView &BufferSet::at(std::string_view name) const {
    for (const auto &buffer : buffers_)
        if (buffer.name == name)
            return buffer.view;
    throw Error("buffer_name", "Named buffer is unavailable.");
}
const Json &BufferSet::metadata() const noexcept {
    return metadata_;
}
std::size_t BufferSet::logical_bytes() const noexcept {
    return logical_;
}
std::size_t BufferSet::retained_bytes() const noexcept {
    return retained_;
}
std::size_t BufferSet::accounted_bytes() const noexcept {
    return accounted_;
}
Json BufferSet::descriptor() const {
    Json buffers = Json::array();
    for (const auto &buffer : buffers_)
        buffers.push_back({{"name", buffer.name}, {"view", buffer.view.descriptor()}});
    return {{"format", "gr.buffer-set/1"}, {"buffers", buffers}, {"metadata", metadata_}};
}
std::string BufferSet::content_digest() const {
    Json buffers = Json::array();
    for (const auto &buffer : buffers_)
        buffers.push_back({{"name", buffer.name}, {"digest", buffer.view.content_digest()}});
    return "sha256:" +
           sha256(canonical(
               Json{{"format", "gr.buffer-set-content/1"}, {"buffers", buffers}, {"metadata", metadata_}}));
}
bool BufferSet::operator==(const BufferSet &other) const {
    return metadata_ == other.metadata_ && buffers_ == other.buffers_;
}
} // namespace gr::memory
