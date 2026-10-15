#include "gr/runtime/transaction.hpp"
#include "gr/storage/sqlite_store.hpp"
#include "sqlite3.h"
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

// This executable is killed by its parent. The trigger/function are installed
// only into its isolated test database; production code has no crash hook.
namespace {
using gr::Json;
std::string marker;
bool armed = false;
[[noreturn]] void barrier() {
    {
        std::ofstream stream(marker, std::ios::binary);
        stream << "publication barrier reached\n";
        stream.flush();
        if (!stream)
            std::terminate();
    }
    for (;;)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
void callback(sqlite3_context *context, int, sqlite3_value **) {
    if (armed)
        barrier();
    sqlite3_result_int(context, 1);
}
int extension(sqlite3 *db, char **, const sqlite3_api_routines *) {
    return sqlite3_create_function_v2(db, "gr_test_barrier", 0, SQLITE_UTF8, nullptr, callback, nullptr,
                                      nullptr, nullptr);
}
struct Database {
    sqlite3 *db = nullptr;
    explicit Database(const std::string &path) {
        if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
            throw std::runtime_error("Could not open isolated SQLite database");
    }
    ~Database() {
        if (db)
            sqlite3_close(db);
    }
    void execute(const char *sql) {
        char *error = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
            const std::string message = error ? error : "SQL error";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }
    std::string scalar(const char *sql) {
        sqlite3_stmt *query = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &query, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
        if (sqlite3_step(query) != SQLITE_ROW) {
            sqlite3_finalize(query);
            throw std::runtime_error("Missing scalar result");
        }
        const auto *value = sqlite3_column_text(query, 0);
        const std::string result = value ? reinterpret_cast<const char *>(value) : "";
        sqlite3_finalize(query);
        return result;
    }
};
gr::runtime::TransactionOptions options() {
    gr::runtime::TransactionOptions result;
    result.workspace = "crash";
    result.expected_revision = "0";
    result.transaction_id = "crash-transaction";
    result.actor = "recorder:test";
    result.reason = "Process interruption conformance";
    result.track_history = true;
    return result;
}
gr::runtime::Stage stage() {
    gr::runtime::Stage result;
    std::vector<std::uint8_t> bytes(1024 * 1024);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(i % 251);
    const auto digest = "sha256:" + gr::sha256(std::span<const std::uint8_t>(bytes));
    result.records.push_back(
        gr::core::construct({{"format", "gr.record/1"},
                             {"id", "material"},
                             {"type", "gr.assertion@1"},
                             {"payload", {{"value", "crash test artifact"}}},
                             {"attribution", {{"agent", "test"}, {"method", "synthetic"}}},
                             {"artifacts", Json::array({{{"sha256", digest},
                                                         {"byte_length", bytes.size()},
                                                         {"media_type", "application/octet-stream"}}})}}));
    result.blobs.push_back({digest, std::move(bytes)});
    return result;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc < 3)
            throw std::runtime_error("Usage: runtime_crash_worker MODE DATABASE [MARKER PHASE]");
        if (sqlite3_auto_extension(std::bit_cast<void (*)()>(&extension)) != SQLITE_OK)
            throw std::runtime_error("Cannot register test-only SQLite barrier function");
        const std::string mode = argv[1], path = argv[2];
        auto store = std::make_shared<gr::storage::SQLiteStore>(path);
        gr::runtime::TransactionCoordinator coordinator(store);
        if (mode == "prepare") {
            store->create_workspace("crash");
            Database db(path);
            db.execute("CREATE TRIGGER test_crash_barrier BEFORE UPDATE OF head ON workspaces "
                       "BEGIN SELECT gr_test_barrier(); END");
            std::cout << "{\"prepared\":true}\n";
        } else if (mode == "run") {
            if (argc != 5)
                throw std::runtime_error("run requires marker and phase");
            marker = argv[3];
            const std::string phase = argv[4];
            if (phase != "before" && phase != "during" && phase != "after")
                throw std::runtime_error("Unknown interruption phase");
            auto transaction = coordinator.begin(options());
            transaction->stage(stage());
            if (phase == "before")
                barrier();
            armed = phase == "during";
            const auto result = transaction->commit();
            if (phase == "after")
                barrier();
            std::cout << result.to_json().dump() << '\n';
        } else if (mode == "inspect") {
            const auto snapshot = coordinator.snapshot("crash");
            const auto receipt = coordinator.receipt("crash", "crash-transaction");
            // Opening the history artifact in snapshot checks digest, checkpoint
            // identity and equality with the physical user-record inventory.
            if (receipt) {
                const auto &artifact = snapshot.at("records").at(0).at("document").at("artifacts").at(0);
                auto reader = store->open_blob("crash", artifact.at("sha256"));
                if (reader->metadata().byte_length != artifact.at("byte_length").get<std::size_t>())
                    throw std::runtime_error("Published material length differs");
            }
            Database db(path);
            Json counts = Json::object();
            for (const char *table :
                 {"revisions", "record_versions", "snapshot_records", "blobs", "revision_blobs"}) {
                const auto sql = std::string("SELECT COUNT(*) FROM ") + table;
                counts[table] = std::stoll(db.scalar(sql.c_str()));
            }
            std::cout << Json{{"head", store->head("crash")},
                              {"receipt", bool(receipt)},
                              {"records", snapshot.at("records").size()},
                              {"history", snapshot.contains("runtime_history")},
                              {"quick_check", db.scalar("PRAGMA quick_check")},
                              {"counts", counts}}
                             .dump()
                      << '\n';
        } else if (mode == "retry") {
            std::cout << coordinator.commit(options(), stage()).to_json().dump() << '\n';
        } else
            throw std::runtime_error("Unknown mode");
        return 0;
    } catch (const gr::Error &error) {
        std::cerr << error.code << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
