#pragma once
#include "gr/memory/buffer.hpp"
#include <bit>
#include <cstring>
#include <type_traits>

namespace gr::memory {
// An owning, rank-one byte range. BufferView has already checked every reachable
// byte; only a single index check remains for random access. No typed object is
// assumed to exist in raw allocation storage. Copies retain the immutable owner.
class ElementRange {
  public:
    explicit ElementRange(const BufferView &view)
        : owner_(view.buffer()), count_(view.elements()), width_(view.element().item_size) {
        if (view.shape().size() != 1)
            throw Error("buffer_rank", "ElementRange requires a rank-one physical view.");
        stride_ = view.strides()[0];
        magnitude_ =
            stride_ < 0 ? static_cast<std::size_t>(-(stride_ + 1)) + 1 : static_cast<std::size_t>(stride_);
        if (count_)
            origin_ = owner_.bytes().data() + view.offset();
    }
    std::size_t size() const noexcept { return count_; }
    std::size_t item_size() const noexcept { return width_; }
    std::ptrdiff_t stride() const noexcept { return stride_; }
    bool contiguous() const noexcept { return count_ <= 1 || (stride_ >= 0 && magnitude_ == width_); }
    std::span<const std::uint8_t> at(std::size_t index) const {
        if (index >= count_)
            throw Error("buffer_bounds", "ElementRange index is outside its validated range.");
        const auto distance = index * magnitude_; // Checked complete extent in BufferView.
        return {stride_ < 0 ? origin_ - distance : origin_ + distance, width_};
    }
    std::span<const std::uint8_t> contiguous_bytes() const {
        if (!contiguous())
            throw Error("buffer_layout", "ElementRange is not contiguous.");
        return count_ ? std::span<const std::uint8_t>(origin_, count_ * width_)
                      : std::span<const std::uint8_t>();
    }
    template <class F> void for_each(F &&function) const {
        auto cursor = origin_;
        for (std::size_t i = 0; i < count_; ++i) {
            function(i, std::span<const std::uint8_t>(cursor, width_));
            // Do not form a before-owner/beyond-one-past pointer after the last
            // item, especially for reversed or widely strided views.
            if (i + 1 < count_)
                cursor = stride_ < 0 ? cursor - magnitude_ : cursor + magnitude_;
        }
    }

  private:
    Buffer owner_;
    const std::uint8_t *origin_ = nullptr;
    std::size_t count_ = 0, width_ = 0, magnitude_ = 0;
    std::ptrdiff_t stride_ = 0;
};
// Inline fixed-width physical loads allow an unaligned machine load where the
// compiler supports it, without aliasing or object-lifetime reinterpretation.
template <class UInt> UInt load_little(std::span<const std::uint8_t> bytes) {
    static_assert(std::is_unsigned_v<UInt> && std::is_integral_v<UInt> && !std::is_same_v<UInt, bool>);
    if (bytes.size() != sizeof(UInt))
        throw Error("buffer_element", "Physical integer load width differs from the byte span.");
    UInt value = 0;
    if constexpr (std::endian::native == std::endian::little)
        std::memcpy(&value, bytes.data(), sizeof(UInt));
    else
        for (std::size_t i = 0; i < sizeof(UInt); ++i)
            value |= static_cast<UInt>(bytes[i]) << (i * 8);
    return value;
}
template <class UInt> void store_little(std::span<std::uint8_t> bytes, UInt value) {
    static_assert(std::is_unsigned_v<UInt> && std::is_integral_v<UInt> && !std::is_same_v<UInt, bool>);
    if (bytes.size() != sizeof(UInt))
        throw Error("buffer_element", "Physical integer store width differs from the byte span.");
    if constexpr (std::endian::native == std::endian::little)
        std::memcpy(bytes.data(), &value, sizeof(UInt));
    else
        for (std::size_t i = 0; i < sizeof(UInt); ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
}
} // namespace gr::memory
