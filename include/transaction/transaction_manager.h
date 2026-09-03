#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
#include "transaction/transaction_state_machine.h"

namespace ota {

struct TransactionRecord {
    std::string transaction_id;
    TransactionState state;
    std::string target_version;
    std::string source_version;
    std::string hardware_version;
    std::string download_path;
    std::string installation_target;
    std::string sha256;
    std::string error_code;
    std::string error_message;
    std::string started_at;
    std::string updated_at;
    pid_t owner_pid;
    std::string active_slot;
    std::string target_slot;
};

struct TransactionHistoryEntry {
    std::string transaction_id;
    std::string version;
    std::string result;
    std::string started_at;
    std::string completed_at;
    std::string sha256;
};

struct TransactionManagerConfig {
    std::string state_dir;
    std::string history_dir;
    std::string lock_file;
    size_t max_history_entries;
};

class TransactionManager {
public:
    TransactionManager();
    ~TransactionManager();

    void set_config(const TransactionManagerConfig& config);

    bool acquire_lock();
    void release_lock();

    bool create_transaction(const std::string& target_version,
                           const std::string& source_version,
                           const std::string& hardware_version,
                           const std::string& sha256 = "");

    bool update_state(TransactionState new_state, const std::string& error_code = "", const std::string& error_message = "");

    bool update_download_path(const std::string& path);

    bool update_installation_target(const std::string& target);

    bool update_active_slot(const std::string& slot);

    bool update_target_slot(const std::string& slot);

    bool record_failure(const std::string& error_code, const std::string& error_message);

    bool complete_transaction();

    bool abort_transaction();

    TransactionRecord get_current_transaction() const;

    bool has_active_transaction() const;

    TransactionState get_current_state() const;

    bool persist_transaction();

    bool load_transaction();

    bool has_incomplete_transaction() const;

    std::string detect_incomplete_state() const;

    void record_history(const std::string& result, const std::string& sha256 = "");

    std::vector<TransactionHistoryEntry> get_history() const;

    bool load_history();

    bool save_history();

    bool add_history_entry(const TransactionHistoryEntry& entry);

    void prune_history();

    std::string generate_transaction_id() const;

    std::string get_current_timestamp() const;

    TransactionManagerConfig get_default_config();

    static TransactionManagerConfig get_test_config(const std::string& base_dir);

private:
    bool write_json_file(const std::string& path, const std::string& json);

    std::string read_json_file(const std::string& path) const;

    bool ensure_directory_exists(const std::string& path);

    std::string escape_json_string(const std::string& str) const;

    std::string unescape_json_string(const std::string& str) const;

    std::string extract_json_value(const std::string& json, const std::string& key) const;

    TransactionManagerConfig config_;
    TransactionRecord current_transaction_;
    std::vector<TransactionHistoryEntry> history_;
    int lock_fd_;
    bool owns_lock_;
};

}
