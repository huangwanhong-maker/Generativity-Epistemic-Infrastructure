#pragma once
#include "gr/common.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <string>

namespace gr::storage {
struct BlobMetadata {
    std::string digest;
    std::uint64_t byte_length = 0;
    std::string integrity = "unverified";
};
struct BlobReadOptions {
    std::size_t chunk_bytes = 64 * 1024; // 1..1 MiB, including verification chunks.
    std::stop_token stop;
};
// An independent object-reader contract; nontransactional artifact adapters need
// not implement workspace publication. Readers are caller-owned, not concurrently
// callable, and remain usable after their originating store closes. open_blob
// verifies the complete object before exposing bytes; destruction unpins it.
class BlobReader {
  public:
    virtual ~BlobReader() = default;
    virtual const BlobMetadata &metadata() const noexcept = 0;
    // Returns <= destination.size() and <= chunk_bytes. EOF returns 0; an offset
    // beyond EOF fails. Cancellation already requested exposes no output.
    virtual std::size_t read(std::uint64_t offset, std::span<std::uint8_t> destination,
                             std::stop_token stop = {}) = 0;
};
} // namespace gr::storage
