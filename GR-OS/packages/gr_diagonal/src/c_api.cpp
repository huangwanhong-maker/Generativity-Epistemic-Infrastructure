#include <cstdlib>
#include <cstring>
#include <gr_diagonal/c_api.h>
#include <gr_diagonal/diagonal.hpp>
#include <limits>

namespace {
int response(const gr::Json &value, int status, char **report, size_t *size) {
    const auto text = value.dump();
    if (text.size() > 4 * 1024 * 1024)
        return 2;
    auto *allocation = static_cast<char *>(std::malloc(text.size() + 1));
    if (!allocation)
        return 2;
    std::memcpy(allocation, text.c_str(), text.size() + 1);
    *report = allocation;
    *size = text.size();
    return status;
}
} // namespace
extern "C" uint32_t gr_diagonal_abi_version(void) {
    return 1;
}
extern "C" void gr_diagonal_free(void *value) {
    std::free(value);
}
extern "C" int gr_diagonal_apply_i64_v1(const int64_t *diagonal, size_t rows, const int64_t *vector,
                                        size_t length, int64_t *output, size_t capacity, uint64_t timeout_ms,
                                        char **report, size_t *report_size) {
    if (!report || !report_size)
        return 2;
    *report = nullptr;
    *report_size = 0;
    try {
        if (rows > 1024 * 1024 || length > 1024 * 1024 || capacity < rows ||
            (rows && (!diagonal || !output)) || (length && !vector) || timeout_ms > 60000)
            throw gr::Error("invalid_argument", "Invalid pointers, capacity, element limit or deadline");
        auto library = std::make_shared<gr::data::Library>();
        gr::data::install_standard(*library);
        gr_diagonal::install(*library);
        library->seal();
        auto left = gr::data::literal(library, gr_diagonal::make({diagonal, rows}));
        auto right = gr::data::literal(library, gr::data::column_i64({vector, length}));
        auto expression = gr::data::apply(library, "example.diagonal.apply@1", {left, right});
        auto program = gr::data::compile({expression});
        gr::runtime::Runtime runtime({.workers = 1});
        gr::runtime::JobOptions options;
        if (timeout_ms)
            options.timeout = std::chrono::milliseconds(timeout_ms);
        const auto completed = runtime.run(program.plan(), options);
        if (completed.status != gr::runtime::JobStatus::succeeded)
            return response({{"ok", false},
                             {"error", completed.error},
                             {"status", gr::runtime::to_string(completed.status)},
                             {"diagnostics", completed.diagnostics}},
                            1, report, report_size);
        const auto values = program.materialize(completed);
        const auto items = gr::data::to_i64(values.at(0));
        if (items.size() != rows)
            throw gr::Error("integrity_error", "Native result length mismatch");
        for (const auto &item : items)
            if (!item)
                throw gr::Error("integrity_error", "Unexpected missing result");
        const auto status = response({{"ok", true},
                                      {"count", rows},
                                      {"package", gr_diagonal::manifest()},
                                      {"provenance", values.at(0).provenance},
                                      {"diagnostics", completed.diagnostics}},
                                     0, report, report_size);
        if (status == 0)
            for (std::size_t i = 0; i < rows; ++i)
                output[i] = *items[i];
        return status;
    } catch (const gr::Error &error) {
        try {
            return response(
                {{"ok", false},
                 {"error", {{"code", error.code}, {"message", error.what()}, {"details", error.details}}}},
                1, report, report_size);
        } catch (...) {
            return 2;
        }
    } catch (const std::exception &error) {
        try {
            return response(
                {{"ok", false}, {"error", {{"code", "extension_failure"}, {"message", error.what()}}}}, 1,
                report, report_size);
        } catch (...) {
            return 2;
        }
    } catch (...) {
        return 2;
    }
}
