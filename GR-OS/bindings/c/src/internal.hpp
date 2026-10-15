#pragma once
#include "gr/common.hpp"
#include "gr/kernel/types.hpp"
#include "gr/memory/buffer.hpp"
#include "gr/runtime/scheduler.hpp"
#include "gr/runtime/transaction.hpp"
#include <functional>
#include <optional>
#include <span>
#include <vector>
namespace gr::binding {
std::string text(const Json &, const char *);
std::size_t count(const Json &, const char *, std::size_t);
const Json &array(const Json &, const char *);
std::string encode(const std::vector<std::uint8_t> &);
std::vector<std::uint8_t> decode(const std::string &);
std::optional<Json> storage_call(const Json &, const std::string &);
std::optional<Json> execution_call(const Json &, const std::string &);
std::optional<Json> runtime_call(const Json &, const std::string &);
std::optional<Json> transaction_call(const Json &, const std::string &);
std::optional<Json> buffer_call(const Json &, const std::string &);
std::optional<Json> experimental_call(const Json &, const std::string &);
std::optional<Json> data_call(const Json &, const std::string &);
Json publish_plan(std::shared_ptr<const runtime::PreparedPlan>);
void discard_plan(const std::string &) noexcept;
runtime::Job job_for(const std::string &);
std::size_t data_export_checkpoint();
void data_export_finish(std::size_t, bool) noexcept;
memory::BufferView buffer_for(const std::string &);
Json publish_buffer(memory::BufferView);
std::size_t buffer_export_checkpoint();
void buffer_export_finish(std::size_t, bool) noexcept;
Json parse_request(const char *, std::size_t);
std::shared_ptr<storage::IStore> store_for(const Json &);
std::shared_ptr<runtime::Transaction> transaction_for(const std::string &);
runtime::TransactionOptions transaction_options(const Json &);
runtime::Stage transaction_stage(const Json &);
kernel::Value value_from_json(const Json &, std::size_t &, std::size_t);
Json value_json(const kernel::Value &);
int respond(const std::function<Json()> &, char **, std::size_t *) noexcept;
} // namespace gr::binding
