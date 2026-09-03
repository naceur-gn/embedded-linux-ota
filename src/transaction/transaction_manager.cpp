#include "transaction/transaction_manager.h"
#include "logging/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <random>
#include <iomanip>
#include <ctime>
#include <cstring>

namespace ota {

TransactionManager::TransactionManager()
    : lock_fd_(-1),
      owns_lock_(false) {
    current_transaction_.state = TransactionState::IDLE;
    current_transaction_.owner_pid = 0;
    config_ = get_default_config();
}

TransactionManager::~TransactionManager() {
    release_lock();
}

void TransactionManager::set_config(const TransactionManagerConfig& config) {
    config_ = config;
}

bool TransactionManager::acquire_lock() {
    ensure_directory_exists(config_.state_dir);

    lock_fd_ = open(config_.lock_file.c_str(), O_CREAT | O_RDWR, 0600);
    if (lock_fd_ < 0) {
        Logger::instance().error("transaction", "Cannot open lock file: " + config_.lock_file);
        return false;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(lock_fd_, F_SETLK, &lock) < 0) {
        close(lock_fd_);
        lock_fd_ = -1;
        Logger::instance().error("transaction", "Cannot acquire lock - another OTA transaction is active");
        return false;
    }

    owns_lock_ = true;

    if (lock_fd_ >= 0) {
        std::string pid_str = std::to_string(getpid());
        write(lock_fd_, pid_str.c_str(), pid_str.size());
        ftruncate(lock_fd_, pid_str.size());
        lseek(lock_fd_, 0, SEEK_SET);
    }

    Logger::instance().info("transaction", "Lock acquired (PID: " + std::to_string(getpid()) + ")");
    return true;
}

void TransactionManager::release_lock() {
    if (!owns_lock_ || lock_fd_ < 0) {
        return;
    }

    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    fcntl(lock_fd_, F_SETLK, &lock);
    close(lock_fd_);
    lock_fd_ = -1;
    owns_lock_ = false;
    Logger::instance().info("transaction", "Lock released");
}

bool TransactionManager::create_transaction(const std::string& target_version,
                                           const std::string& source_version,
                                           const std::string& hardware_version,
                                           const std::string& sha256) {
    current_transaction_.transaction_id = generate_transaction_id();
    current_transaction_.state = TransactionState::IDLE;
    current_transaction_.target_version = target_version;
    current_transaction_.source_version = source_version;
    current_transaction_.hardware_version = hardware_version;
    current_transaction_.sha256 = sha256;
    current_transaction_.error_code.clear();
    current_transaction_.error_message.clear();
    current_transaction_.download_path.clear();
    current_transaction_.installation_target.clear();
    current_transaction_.started_at = get_current_timestamp();
    current_transaction_.updated_at = current_transaction_.started_at;
    current_transaction_.owner_pid = getpid();

    if (!persist_transaction()) {
        Logger::instance().error("transaction", "Failed to persist new transaction");
        return false;
    }

    Logger::instance().info("transaction",
        "Transaction created: " + current_transaction_.transaction_id +
        " (target: " + target_version + ")");
    return true;
}

bool TransactionManager::update_state(TransactionState new_state, const std::string& error_code, const std::string& error_message) {
    TransactionState old_state = current_transaction_.state;

    TransactionStateMachine sm;
    sm.set_state(old_state);
    if (!sm.can_transition(old_state, new_state)) {
        Logger::instance().error("transaction",
            "Invalid transition: " + transaction_state_to_string(old_state) +
            " -> " + transaction_state_to_string(new_state));
        return false;
    }

    if (new_state == TransactionState::FAILED) {
        current_transaction_.error_code = error_code;
        current_transaction_.error_message = error_message;
    }

    current_transaction_.state = new_state;
    current_transaction_.updated_at = get_current_timestamp();

    Logger::instance().info("transaction",
        "State transition: " + transaction_state_to_string(old_state) +
        " -> " + transaction_state_to_string(new_state) +
        " [transaction: " + current_transaction_.transaction_id + "]");

    return persist_transaction();
}

bool TransactionManager::update_download_path(const std::string& path) {
    current_transaction_.download_path = path;
    current_transaction_.updated_at = get_current_timestamp();
    return persist_transaction();
}

bool TransactionManager::update_installation_target(const std::string& target) {
    current_transaction_.installation_target = target;
    current_transaction_.updated_at = get_current_timestamp();
    return persist_transaction();
}

bool TransactionManager::update_active_slot(const std::string& slot) {
    current_transaction_.active_slot = slot;
    current_transaction_.updated_at = get_current_timestamp();
    return persist_transaction();
}

bool TransactionManager::update_target_slot(const std::string& slot) {
    current_transaction_.target_slot = slot;
    current_transaction_.updated_at = get_current_timestamp();
    return persist_transaction();
}

bool TransactionManager::record_failure(const std::string& error_code, const std::string& error_message) {
    bool result = update_state(TransactionState::FAILED, error_code, error_message);
    if (result) {
        record_history("FAILED", current_transaction_.sha256);
    }
    return result;
}

bool TransactionManager::complete_transaction() {
    bool result = update_state(TransactionState::INSTALLED);
    if (result) {
        record_history("SUCCESS", current_transaction_.sha256);
    }
    return result;
}

bool TransactionManager::abort_transaction() {
    bool result = update_state(TransactionState::FAILED, "ABORTED", "Transaction aborted by user");
    if (result) {
        record_history("FAILED", current_transaction_.sha256);
    }
    return result;
}

TransactionRecord TransactionManager::get_current_transaction() const {
    return current_transaction_;
}

bool TransactionManager::has_active_transaction() const {
    return current_transaction_.state != TransactionState::IDLE &&
           current_transaction_.state != TransactionState::INSTALLED &&
           current_transaction_.state != TransactionState::FAILED;
}

TransactionState TransactionManager::get_current_state() const {
    return current_transaction_.state;
}

bool TransactionManager::persist_transaction() {
    std::string transaction_file = config_.state_dir + "/transaction.json";

    ensure_directory_exists(config_.state_dir);

    std::stringstream json;
    json << "{\n";
    json << "  \"transaction_id\": \"" << escape_json_string(current_transaction_.transaction_id) << "\",\n";
    json << "  \"state\": \"" << transaction_state_to_string(current_transaction_.state) << "\",\n";
    json << "  \"target_version\": \"" << escape_json_string(current_transaction_.target_version) << "\",\n";
    json << "  \"source_version\": \"" << escape_json_string(current_transaction_.source_version) << "\",\n";
    json << "  \"hardware_version\": \"" << escape_json_string(current_transaction_.hardware_version) << "\",\n";
    json << "  \"download_path\": \"" << escape_json_string(current_transaction_.download_path) << "\",\n";
    json << "  \"installation_target\": \"" << escape_json_string(current_transaction_.installation_target) << "\",\n";
    json << "  \"sha256\": \"" << escape_json_string(current_transaction_.sha256) << "\",\n";
    json << "  \"error_code\": \"" << escape_json_string(current_transaction_.error_code) << "\",\n";
    json << "  \"error_message\": \"" << escape_json_string(current_transaction_.error_message) << "\",\n";
    json << "  \"started_at\": \"" << escape_json_string(current_transaction_.started_at) << "\",\n";
    json << "  \"updated_at\": \"" << escape_json_string(current_transaction_.updated_at) << "\",\n";
    json << "  \"owner_pid\": " << current_transaction_.owner_pid << ",\n";
    json << "  \"active_slot\": \"" << escape_json_string(current_transaction_.active_slot) << "\",\n";
    json << "  \"target_slot\": \"" << escape_json_string(current_transaction_.target_slot) << "\"\n";
    json << "}\n";

    std::string tmp_file = transaction_file + ".tmp";
    bool result = write_json_file(tmp_file, json.str());

    if (result) {
        if (rename(tmp_file.c_str(), transaction_file.c_str()) != 0) {
            Logger::instance().error("transaction", "Failed to rename transaction file");
            result = false;
        }
    }

    return result;
}

bool TransactionManager::load_transaction() {
    std::string transaction_file = config_.state_dir + "/transaction.json";
    std::string json = read_json_file(transaction_file);

    if (json.empty()) {
        return false;
    }

    std::string state_val = extract_json_value(json, "state");
    if (state_val.empty()) {
        Logger::instance().error("transaction", "Invalid or missing state in transaction file");
        return false;
    }

    current_transaction_.transaction_id = extract_json_value(json, "transaction_id");
    current_transaction_.state = string_to_transaction_state(state_val);
    current_transaction_.target_version = extract_json_value(json, "target_version");
    current_transaction_.source_version = extract_json_value(json, "source_version");
    current_transaction_.hardware_version = extract_json_value(json, "hardware_version");
    current_transaction_.download_path = extract_json_value(json, "download_path");
    current_transaction_.installation_target = extract_json_value(json, "installation_target");
    current_transaction_.sha256 = extract_json_value(json, "sha256");
    current_transaction_.error_code = extract_json_value(json, "error_code");
    current_transaction_.error_message = extract_json_value(json, "error_message");
    current_transaction_.started_at = extract_json_value(json, "started_at");
    current_transaction_.updated_at = extract_json_value(json, "updated_at");

    std::string pid_str = extract_json_value(json, "owner_pid");
    try {
        current_transaction_.owner_pid = std::stoi(pid_str);
    } catch (...) {
        current_transaction_.owner_pid = 0;
    }

    current_transaction_.active_slot = extract_json_value(json, "active_slot");
    current_transaction_.target_slot = extract_json_value(json, "target_slot");

    Logger::instance().info("transaction",
        "Transaction loaded: " + current_transaction_.transaction_id +
        " (state: " + transaction_state_to_string(current_transaction_.state) + ")");
    return true;
}

bool TransactionManager::has_incomplete_transaction() const {
    return current_transaction_.state != TransactionState::IDLE &&
           current_transaction_.state != TransactionState::INSTALLED;
}

std::string TransactionManager::detect_incomplete_state() const {
    if (!has_incomplete_transaction()) {
        return "";
    }

    std::string msg = "Previous OTA transaction detected.\n";
    msg += "Transaction ID: " + current_transaction_.transaction_id + "\n";
    msg += "State: " + transaction_state_to_string(current_transaction_.state) + "\n";
    msg += "Target version: " + current_transaction_.target_version + "\n";
    msg += "Started: " + current_transaction_.started_at + "\n";
    msg += "Recovery required.\n";
    return msg;
}

void TransactionManager::record_history(const std::string& result, const std::string& sha256) {
    TransactionHistoryEntry entry;
    entry.transaction_id = current_transaction_.transaction_id;
    entry.version = current_transaction_.target_version;
    entry.result = result;
    entry.started_at = current_transaction_.started_at;
    entry.completed_at = get_current_timestamp();
    entry.sha256 = sha256;

    add_history_entry(entry);
}

std::vector<TransactionHistoryEntry> TransactionManager::get_history() const {
    return history_;
}

bool TransactionManager::load_history() {
    history_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(config_.history_dir, ec)) {
        return true;
    }

    for (const auto& entry : std::filesystem::directory_iterator(config_.history_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string json = read_json_file(entry.path().string());
            if (!json.empty()) {
                TransactionHistoryEntry hist_entry;
                hist_entry.transaction_id = extract_json_value(json, "transaction_id");
                hist_entry.version = extract_json_value(json, "version");
                hist_entry.result = extract_json_value(json, "result");
                hist_entry.started_at = extract_json_value(json, "started_at");
                hist_entry.completed_at = extract_json_value(json, "completed_at");
                hist_entry.sha256 = extract_json_value(json, "sha256");

                if (hist_entry.transaction_id.empty() || hist_entry.version.empty()) {
                    Logger::instance().warn("transaction", "Skipping malformed history entry: " + entry.path().string());
                    continue;
                }

                history_.push_back(hist_entry);
            }
        }
    }

    std::sort(history_.begin(), history_.end(),
        [](const TransactionHistoryEntry& a, const TransactionHistoryEntry& b) {
            return a.completed_at > b.completed_at;
        });

    return true;
}

bool TransactionManager::save_history() {
    std::error_code ec;
    if (!std::filesystem::create_directories(config_.history_dir, ec)) {
        if (ec) {
            Logger::instance().error("transaction", "Cannot create history directory");
            return false;
        }
    }

    std::error_code remove_ec;
    for (const auto& entry : std::filesystem::directory_iterator(config_.history_dir, remove_ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::filesystem::remove(entry.path(), ec);
        }
    }

    for (const auto& hist_entry : history_) {
        std::string filename = config_.history_dir + "/" + hist_entry.transaction_id + ".json";

        std::stringstream json;
        json << "{\n";
        json << "  \"transaction_id\": \"" << escape_json_string(hist_entry.transaction_id) << "\",\n";
        json << "  \"version\": \"" << escape_json_string(hist_entry.version) << "\",\n";
        json << "  \"result\": \"" << escape_json_string(hist_entry.result) << "\",\n";
        json << "  \"started_at\": \"" << escape_json_string(hist_entry.started_at) << "\",\n";
        json << "  \"completed_at\": \"" << escape_json_string(hist_entry.completed_at) << "\",\n";
        json << "  \"sha256\": \"" << escape_json_string(hist_entry.sha256) << "\"\n";
        json << "}\n";

        if (!write_json_file(filename, json.str())) {
            Logger::instance().error("transaction", "Failed to write history entry: " + filename);
            return false;
        }
    }

    return true;
}

bool TransactionManager::add_history_entry(const TransactionHistoryEntry& entry) {
    load_history();
    history_.push_back(entry);
    prune_history();
    return save_history();
}

void TransactionManager::prune_history() {
    while (history_.size() > config_.max_history_entries) {
        history_.pop_back();
    }
}

std::string TransactionManager::generate_transaction_id() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 4; ++i) {
        ss << std::setw(8) << dis(gen);
        if (i == 0 || i == 1 || i == 2) ss << "-";
        if (i == 2) ss << "4";
    }

    return ss.str();
}

std::string TransactionManager::get_current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    gmtime_r(&time, &tm_buf);

    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

TransactionManagerConfig TransactionManager::get_default_config() {
    TransactionManagerConfig config;
    config.state_dir = "/var/lib/ota/state";
    config.history_dir = "/var/lib/ota/state/history";
    config.lock_file = "/var/lib/ota/ota.lock";
    config.max_history_entries = 10;
    return config;
}

TransactionManagerConfig TransactionManager::get_test_config(const std::string& base_dir) {
    TransactionManagerConfig config;
    config.state_dir = base_dir + "/state";
    config.history_dir = base_dir + "/state/history";
    config.lock_file = base_dir + "/ota.lock";
    config.max_history_entries = 10;
    return config;
}

bool TransactionManager::write_json_file(const std::string& path, const std::string& json) {
    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::instance().error("transaction", "Cannot write file: " + path);
        return false;
    }

    file << json;
    file.flush();
    file.close();
    return file.good();
}

std::string TransactionManager::read_json_file(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool TransactionManager::ensure_directory_exists(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::create_directories(path, ec)) {
        return !ec;
    }
    return true;
}

std::string TransactionManager::escape_json_string(const std::string& str) const {
    std::string result;
    result.reserve(str.size() + 16);

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

std::string TransactionManager::unescape_json_string(const std::string& str) const {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"':  result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                default:   result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string TransactionManager::extract_json_value(const std::string& json, const std::string& key) const {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = json.find(':', key_pos + search_key.size());
    if (colon_pos == std::string::npos) {
        return "";
    }

    size_t value_start = json.find_first_not_of(" \t\r\n", colon_pos + 1);
    if (value_start == std::string::npos) {
        return "";
    }

    if (json[value_start] == '"') {
        size_t value_end = value_start + 1;
        while (value_end < json.size()) {
            if (json[value_end] == '\\' && value_end + 1 < json.size()) {
                value_end += 2;
            } else if (json[value_end] == '"') {
                break;
            } else {
                ++value_end;
            }
        }
        return unescape_json_string(json.substr(value_start + 1, value_end - value_start - 1));
    } else {
        size_t value_end = json.find_first_of(",}", value_start);
        if (value_end == std::string::npos) {
            return json.substr(value_start);
        }
        return json.substr(value_start, value_end - value_start);
    }
}

}
