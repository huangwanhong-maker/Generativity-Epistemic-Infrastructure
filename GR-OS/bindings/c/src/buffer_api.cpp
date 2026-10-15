#include "gr/c_api.h"
#include "internal.hpp"
#include <atomic>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <set>

namespace gr::binding {
namespace {
std::mutex buffer_mutex;
std::map<std::string, memory::BufferView> buffers;
std::atomic<std::uint64_t> sequence{1};
thread_local std::vector<std::string> exported;
auto import_budget = std::make_shared<memory::AllocationBudget>(256 * 1024 * 1024);
constexpr std::size_t maximum_buffer = 64 * 1024 * 1024;
void keys(const Json &j, std::initializer_list<const char *> allowed) {
    if (!j.is_object())
        throw Error("invalid_request", "Expected buffer layout object");
    std::set<std::string> names(allowed.begin(), allowed.end());
    for (auto it = j.begin(); it != j.end(); ++it)
        if (!names.contains(it.key()))
            throw Error("invalid_request", "Unknown buffer field: " + it.key());
}
std::ptrdiff_t signed_count(const Json &j, const char *key, std::ptrdiff_t fallback) {
    if (!j.contains(key))
        return fallback;
    const auto &v = j.at(key);
    if (!v.is_number_integer() ||
        (v.is_number_unsigned() && v.get<std::uint64_t>() > static_cast<std::uint64_t>(PTRDIFF_MAX)))
        throw Error("invalid_request", std::string(key) + " must be a signed pointer-sized integer");
    const auto n = v.get<std::int64_t>();
    if (n < PTRDIFF_MIN || n > PTRDIFF_MAX)
        throw Error("invalid_request", "Stride exceeds pointer range");
    return static_cast<std::ptrdiff_t>(n);
}
memory::BufferView view(memory::Buffer data, const Json &layout) {
    keys(layout, {"element", "shape", "strides", "byte_offset"});
    memory::ElementDescriptor element;
    if (layout.contains("element")) {
        const auto &e = layout.at("element");
        keys(e, {"format_id", "item_size", "alignment", "byte_order", "parameters"});
        if (e.contains("format_id"))
            element.format_id = text(e, "format_id");
        element.item_size = count(e, "item_size", 1);
        element.alignment = count(e, "alignment", 1);
        element.parameters = e.value("parameters", Json::object());
        const auto order = e.value("byte_order", std::string("not_applicable"));
        if (order == "little")
            element.byte_order = memory::ByteOrder::Little;
        else if (order == "big")
            element.byte_order = memory::ByteOrder::Big;
        else if (order != "not_applicable")
            throw Error("invalid_request", "Unknown byte order");
    }
    std::vector<std::size_t> shape;
    if (layout.contains("shape")) {
        for (const auto &v : array(layout, "shape"))
            shape.push_back(count(Json{{"v", v}}, "v", 0));
    } else
        shape.push_back(element.item_size ? data.size() / element.item_size : 0);
    std::vector<std::ptrdiff_t> strides;
    if (layout.contains("strides"))
        for (const auto &v : array(layout, "strides"))
            strides.push_back(signed_count(Json{{"v", v}}, "v", 0));
    return memory::BufferView(std::move(data), std::move(element), std::move(shape), std::move(strides),
                              count(layout, "byte_offset", 0));
}
std::string token(const char *value, std::size_t size) {
    if (!value || size > 128 || std::memchr(value, 0, size))
        throw Error("invalid_handle", "Invalid buffer handle");
    return {value, size};
}
} // namespace
memory::BufferView buffer_for(const std::string &handle) {
    std::lock_guard lock(buffer_mutex);
    const auto it = buffers.find(handle);
    if (it == buffers.end())
        throw Error("invalid_handle", "Buffer is closed or unknown");
    return it->second;
}
Json publish_buffer(memory::BufferView value) {
    const auto handle = "buffer:" + std::to_string(sequence.fetch_add(1));
    Json result{{"handle", handle},
                {"layout", value.descriptor()},
                {"logical_bytes", value.logical_bytes()},
                {"retained_bytes", value.retained_bytes()},
                {"contiguous", value.c_contiguous()}};
    std::lock_guard lock(buffer_mutex);
    if (buffers.size() >= 4096)
        throw Error("resource_limit", "At most 4096 exported buffer handles");
    exported.push_back(handle);
    buffers.emplace(handle, std::move(value));
    return result;
}
std::size_t buffer_export_checkpoint() {
    return exported.size();
}
void buffer_export_finish(std::size_t checkpoint, bool success) noexcept {
    if (!success) {
        std::lock_guard lock(buffer_mutex);
        for (std::size_t i = checkpoint; i < exported.size(); ++i)
            buffers.erase(exported[i]);
    }
    exported.resize(checkpoint);
}
std::optional<Json> buffer_call(const Json &request, const std::string &op) {
    if (op.rfind("buffer.", 0) != 0)
        return {};
    if (op == "buffer.capabilities")
        return Json{{"immutable", true},
                    {"import", "copy"},
                    {"memory_domain", "host"},
                    {"max_import_bytes", maximum_buffer},
                    {"import_budget_bytes", import_budget->limit()},
                    {"import_bytes_retained", import_budget->used()},
                    {"max_handles", 4096}};
    if (op == "buffer.import_base64") {
        const auto encoded = text(request, "data_base64");
        if (encoded.size() > 16 * 1024 * 1024)
            throw Error("request_limit", "Encoded buffer exceeds request budget");
        auto bytes = decode(encoded);
        auto owned = memory::Buffer::copy(bytes, 64, import_budget);
        return publish_buffer(view(std::move(owned), request.value("layout", Json::object())));
    }
    const auto handle = text(request, "handle");
    if (op == "buffer.close") {
        std::lock_guard lock(buffer_mutex);
        if (!buffers.erase(handle))
            throw Error("invalid_handle", "Buffer is closed or unknown");
        return Json{{"closed", true}};
    }
    const auto value = buffer_for(handle);
    if (op == "buffer.describe")
        return Json{{"layout", value.descriptor()},
                    {"logical_bytes", value.logical_bytes()},
                    {"retained_bytes", value.retained_bytes()},
                    {"contiguous", value.c_contiguous()}};
    if (op == "buffer.retain")
        return publish_buffer(value);
    if (op == "buffer.slice") {
        keys(request, {"abi", "op", "handle", "axis", "start", "count", "step"});
        return publish_buffer(value.slice(count(request, "axis", 0), count(request, "start", 0),
                                          count(request, "count", 0), signed_count(request, "step", 1)));
    }
    if (op == "buffer.copy_contiguous")
        return publish_buffer(value.copy_contiguous(import_budget));
    if (op == "buffer.digest")
        return Json{{"digest", value.content_digest()}};
    if (op == "buffer.export_base64") {
        if (value.logical_bytes() > 12 * 1024 * 1024)
            throw Error("response_limit", "JSON buffer export exceeds 12 MiB; use the binary API");
        auto copy = value.copy_contiguous(import_budget);
        const auto bytes = copy.contiguous_bytes();
        return Json{{"data_base64", encode(std::vector<std::uint8_t>(bytes.begin(), bytes.end()))},
                    {"layout", copy.descriptor()}};
    }
    throw Error("unsupported_operation", "Unknown buffer operation");
}
} // namespace gr::binding

struct gr_buffer_lease {
    gr::memory::BufferView view;
    std::vector<std::uint64_t> shape;
    std::vector<std::int64_t> strides;
    explicit gr_buffer_lease(gr::memory::BufferView v)
        : view(std::move(v)), shape(view.shape().begin(), view.shape().end()),
          strides(view.strides().begin(), view.strides().end()) {}
};
namespace {
int transport_failure(char **response, std::size_t *size) noexcept {
    if (response)
        *response = nullptr;
    if (size)
        *size = 0;
    return 2;
}
} // namespace
extern "C" int gr_buffer_import(const std::uint8_t *data, std::size_t size, const char *layout,
                                std::size_t layout_size, char **response, std::size_t *response_size) {
    try {
        return gr::binding::respond(
            [&]() {
                if ((!data && size) || !layout || layout_size > 1024 * 1024 ||
                    size > gr::binding::maximum_buffer)
                    throw gr::Error("request_limit", "Invalid or oversized buffer import");
                auto descriptor = gr::parse_metadata(std::string_view(layout, layout_size));
                auto owned = gr::memory::Buffer::copy({data, size}, 64, gr::binding::import_budget);
                return gr::binding::publish_buffer(gr::binding::view(std::move(owned), descriptor));
            },
            response, response_size);
    } catch (...) {
        return transport_failure(response, response_size);
    }
}
extern "C" int gr_buffer_read(const char *handle, std::size_t handle_size, std::uint64_t offset,
                              std::uint8_t *destination, std::size_t capacity, char **response,
                              std::size_t *response_size) {
    try {
        return gr::binding::respond(
            [&]() {
                if ((!destination && capacity) || capacity > gr::binding::maximum_buffer)
                    throw gr::Error("invalid_request", "Invalid buffer destination");
                const auto v = gr::binding::buffer_for(gr::binding::token(handle, handle_size));
                const auto bytes = v.contiguous_bytes();
                if (offset > bytes.size())
                    throw gr::Error("buffer_bounds", "Offset exceeds buffer length");
                const auto n = std::min(capacity, bytes.size() - static_cast<std::size_t>(offset));
                if (n)
                    std::memcpy(destination, bytes.data() + offset, n);
                return gr::Json{{"bytes_read", n}};
            },
            response, response_size);
    } catch (...) {
        return transport_failure(response, response_size);
    }
}
extern "C" int gr_buffer_pin(const char *handle, std::size_t handle_size, gr_buffer_lease **lease,
                             gr_buffer_info *info, char **response, std::size_t *response_size) {
    if (lease)
        *lease = nullptr;
    if (info)
        *info = {};
    try {
        if (!lease || !info)
            return transport_failure(response, response_size);
        std::unique_ptr<gr_buffer_lease> owned;
        const auto status = gr::binding::respond(
            [&]() {
                owned = std::make_unique<gr_buffer_lease>(
                    gr::binding::buffer_for(gr::binding::token(handle, handle_size)));
                return gr::Json{{"pinned", true}};
            },
            response, response_size);
        if (status != 0)
            return status;
        const auto &v = owned->view;
        const auto bytes = v.buffer().bytes();
        info->data = bytes.data() ? bytes.data() + v.offset() : nullptr;
        info->logical_bytes = v.logical_bytes();
        info->item_size = v.element().item_size;
        info->ndim = v.shape().size();
        info->shape = owned->shape.data();
        info->strides = owned->strides.data();
        info->format_id = v.element().format_id.c_str();
        info->byte_order = v.element().byte_order == gr::memory::ByteOrder::Little ? 1
                           : v.element().byte_order == gr::memory::ByteOrder::Big  ? 2
                                                                                   : 0;
        info->known_format = v.element().format_id == "gr.int64@1" && v.element().item_size == 8 &&
                                     v.element().parameters.empty() && info->byte_order != 0
                                 ? 1
                                 : 0;
        *lease = owned.release();
        return 0;
    } catch (...) {
        return transport_failure(response, response_size);
    }
}
extern "C" void gr_buffer_unpin(gr_buffer_lease *lease) {
    delete lease;
}
