#include "device/device_config.h"
#include "device/device_state.h"
#include "client/update_manager.h"
#include "validation/integrity_validator.h"
#include "security/signature_verifier.h"
#include "installation/installer.h"
#include "transaction/transaction_manager.h"
#include "transaction/transaction_state_machine.h"
#include "slot/slot_manager.h"
#include "boot/simulated_boot_control.h"
#include "logging/logger.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static const std::string DEFAULT_CONFIG_PATH = "/etc/ota/device.conf";
static const std::string DEFAULT_STATE_DIR = "/var/lib/ota";
static const std::string DEFAULT_LOG_DIR = "/var/log/ota";
static const std::string DEFAULT_SERVER_URL = "http://localhost:8080";
static const std::string DEFAULT_DOWNLOAD_DIR = "/var/lib/ota/downloads";
static const std::string DEFAULT_TRUSTED_KEY = "/etc/ota/trusted/ota-signing.pub";

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [OPTIONS]\n"
              << "\nCommands:\n"
              << "  check             Check for available updates\n"
              << "  download          Download available update\n"
              << "  verify            Verify image integrity (SHA-256)\n"
              << "  verify-signature  Verify release authenticity (digital signature)\n"
              << "  install           Install validated update\n"
              << "  status            Show OTA transaction status\n"
              << "  history           Show OTA transaction history\n"
              << "  slots             Show A/B slot status\n"
              << "  slots init        Initialize A/B slot system\n"
              << "  boot status       Show boot control status\n"
              << "  boot set <slot>   Set next boot slot (A or B)\n"
              << "  boot clear        Clear pending next boot selection\n"
              << "  boot simulate     Simulate one boot cycle\n"
              << "\nOptions:\n"
              << "  -c, --config PATH        Path to device config (default: " << DEFAULT_CONFIG_PATH << ")\n"
              << "  -s, --state-dir PATH     Path to state directory (default: " << DEFAULT_STATE_DIR << ")\n"
              << "  -l, --log-dir PATH       Path to log directory (default: " << DEFAULT_LOG_DIR << ")\n"
              << "  -u, --server-url URL     OTA server URL (default: " << DEFAULT_SERVER_URL << ")\n"
              << "  -d, --download-dir PATH  Download directory (default: " << DEFAULT_DOWNLOAD_DIR << ")\n"
              << "  -i, --image PATH         Image file to verify/install\n"
              << "  -e, --expected-hash HEX  Expected SHA-256 hash\n"
              << "  -r, --release-dir PATH   Release directory (for verify-signature command)\n"
              << "  -k, --public-key PATH    Trusted public key\n"
              << "  -v, --version VER        Version string (for install command)\n"
              << "  -h, --help               Show this help message\n";
}

void print_check_result(const ota::UpdateInfo& info) {
    std::cout << "\nCurrent version: " << info.metadata.current_version << "\n";

    switch (info.result) {
        case ota::UpdateCheckResult::UPDATE_AVAILABLE:
            std::cout << "Available version: " << info.metadata.version << "\n";
            std::cout << "Update available: YES\n";
            std::cout << "Hardware version: " << info.metadata.hardware_version << "\n";
            std::cout << "Image size: " << info.metadata.image_size << " bytes\n";
            std::cout << "SHA-256: " << info.metadata.sha256 << "\n";
            break;

        case ota::UpdateCheckResult::NO_UPDATE:
            std::cout << "Update available: NO\n";
            break;

        case ota::UpdateCheckResult::INCOMPATIBLE:
            std::cout << "Update available: NO (incompatible hardware)\n";
            std::cout << "Error: " << info.error_message << "\n";
            break;

        case ota::UpdateCheckResult::ERROR:
            std::cout << "Error checking for update: " << info.error_message << "\n";
            break;
    }
    std::cout << "\n";
}

void print_verification_result(const ota::ValidationResult& result) {
    std::cout << "\nImage: " << (result.error_message.empty() ? "verified" : "failed") << "\n";
    std::cout << "Expected SHA-256: " << result.expected_hash << "\n";
    std::cout << "Calculated SHA-256: " << result.calculated_hash << "\n";
    std::cout << "Expected size: " << result.expected_size << " bytes\n";
    std::cout << "Actual size: " << result.actual_size << " bytes\n";
    std::cout << "\nIntegrity: " << validation_status_to_string(result.status) << "\n";

    if (!result.is_valid()) {
        std::cout << "Reason: " << result.error_message << "\n";
    }
    std::cout << "\n";
}

void print_signature_result(const ota::SignatureResult& result) {
    std::cout << "\nAuthenticity: " << signature_status_to_string(result.status) << "\n";

    if (!result.signed_data_hash.empty()) {
        std::cout << "Signed data hash: " << result.signed_data_hash << "\n";
    }

    if (!result.is_valid()) {
        std::cout << "Reason: " << result.error_message << "\n";
    }
    std::cout << "\n";
}

void print_install_result(const ota::InstallResult& result) {
    std::cout << "\nInstallation: " << install_status_to_string(result.status) << "\n";

    if (!result.installed_path.empty()) {
        std::cout << "Installed to: " << result.installed_path << "\n";
    }

    if (!result.calculated_sha256.empty()) {
        std::cout << "Installed SHA-256: " << result.calculated_sha256 << "\n";
    }

    if (!result.is_success()) {
        std::cout << "Reason: " << result.error_message << "\n";
    }
    std::cout << "\n";
}

std::string load_file_content(const std::string& path) {
    std::ifstream file(path);
    if (!file.good()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string canonicalize_metadata(const std::string& metadata_json) {
    std::string temp_path = "/tmp/ota_canonical_metadata.json";
    {
        std::ofstream temp_file(temp_path);
        if (!temp_file.good()) {
            return "";
        }
        temp_file << metadata_json;
    }

    std::string cmd = "python3 -c 'import json; f=open(\"" + temp_path + "\"); metadata=json.load(f); f.close(); sign_data={k:v for k,v in metadata.items() if k!=\"signature\"}; print(json.dumps(sign_data,sort_keys=True,separators=(\",\",\":\"),ensure_ascii=True),end=\"\")' 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        unlink(temp_path.c_str());
        return "";
    }

    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);

    unlink(temp_path.c_str());
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    std::string config_path = DEFAULT_CONFIG_PATH;
    std::string state_dir = DEFAULT_STATE_DIR;
    std::string log_dir = DEFAULT_LOG_DIR;
    std::string server_url = DEFAULT_SERVER_URL;
    std::string download_dir = DEFAULT_DOWNLOAD_DIR;
    std::string image_path;
    std::string expected_hash;
    std::string release_dir;
    std::string public_key_path = DEFAULT_TRUSTED_KEY;
    std::string version;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) config_path = argv[++i];
        } else if (arg == "-s" || arg == "--state-dir") {
            if (i + 1 < argc) state_dir = argv[++i];
        } else if (arg == "-l" || arg == "--log-dir") {
            if (i + 1 < argc) log_dir = argv[++i];
        } else if (arg == "-u" || arg == "--server-url") {
            if (i + 1 < argc) server_url = argv[++i];
        } else if (arg == "-d" || arg == "--download-dir") {
            if (i + 1 < argc) download_dir = argv[++i];
        } else if (arg == "-i" || arg == "--image") {
            if (i + 1 < argc) image_path = argv[++i];
        } else if (arg == "-e" || arg == "--expected-hash") {
            if (i + 1 < argc) expected_hash = argv[++i];
        } else if (arg == "-r" || arg == "--release-dir") {
            if (i + 1 < argc) release_dir = argv[++i];
        } else if (arg == "-k" || arg == "--public-key") {
            if (i + 1 < argc) public_key_path = argv[++i];
        } else if (arg == "-v" || arg == "--version") {
            if (i + 1 < argc) version = argv[++i];
        }
    }

    auto& logger = ota::Logger::instance();
    logger.initialize(log_dir, ota::LogLevel::INFO);

    if (command == "verify") {
        if (image_path.empty() || expected_hash.empty()) {
            std::cerr << "Error: verify command requires -i <image> and -e <expected-hash>\n";
            print_usage(argv[0]);
            return 1;
        }

        ota::IntegrityValidator validator;
        auto result = validator.validate_file(image_path, expected_hash);

        print_verification_result(result);

        logger.info("cli", "Verification completed: " + validation_status_to_string(result.status));

        return result.is_valid() ? 0 : 1;

    } else if (command == "verify-signature") {
        if (release_dir.empty()) {
            std::cerr << "Error: verify-signature command requires -r <release-dir>\n";
            print_usage(argv[0]);
            return 1;
        }

        std::string metadata_path = release_dir + "/metadata.json";
        std::string signature_path = release_dir + "/metadata.sig";

        std::string metadata_content = load_file_content(metadata_path);
        if (metadata_content.empty()) {
            std::cerr << "Error: Cannot load metadata from: " << metadata_path << "\n";
            logger.error("cli", "Cannot load metadata: " + metadata_path);
            return 1;
        }

        std::string signature_b64;
        {
            std::ifstream sig_file(signature_path, std::ios::binary);
            if (!sig_file.good()) {
                std::cerr << "Error: Cannot load signature from: " << signature_path << "\n";
                logger.error("cli", "Cannot load signature: " + signature_path);
                return 1;
            }

            std::stringstream sig_buffer;
            sig_buffer << sig_file.rdbuf();
            std::string sig_der = sig_buffer.str();

            BIO* b64 = BIO_new(BIO_f_base64());
            BIO* bmem = BIO_new(BIO_s_mem());
            b64 = BIO_push(b64, bmem);
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            BIO_write(b64, sig_der.data(), sig_der.size());
            BIO_flush(b64);

            BUF_MEM* bptr;
            BIO_get_mem_ptr(b64, &bptr);
            signature_b64.assign(bptr->data, bptr->length);
            BIO_free_all(b64);
        }

        std::string canonical_data = canonicalize_metadata(metadata_content);

        ota::SignatureVerifier verifier;
        auto result = verifier.verify_signature(canonical_data, signature_b64, public_key_path);

        print_signature_result(result);

        logger.info("cli", "Signature verification completed: " +
                   signature_status_to_string(result.status));

        return result.is_valid() ? 0 : 1;

    } else if (command == "install") {
        if (image_path.empty() || version.empty() || expected_hash.empty()) {
            std::cerr << "Error: install command requires -i <image>, -v <version>, and -e <expected-hash>\n";
            print_usage(argv[0]);
            return 1;
        }

        ota::InstallConfig install_config;
        install_config.staging_dir = state_dir + "/staging";
        install_config.install_target = state_dir + "/install-target";
        install_config.state_dir = state_dir + "/state";
        install_config.min_free_space_mb = 100;

        ota::InstallManager installer;
        installer.set_config(install_config);

        ota::InstallInfo install_info;
        install_info.version = version;
        install_info.image_path = image_path;
        install_info.expected_sha256 = expected_hash;
        install_info.expected_size = 0;

        struct stat image_stat;
        if (stat(image_path.c_str(), &image_stat) == 0) {
            install_info.expected_size = image_stat.st_size;
        }

        std::cout << "Preparing installation...\n";
        std::cout << "Version: " << version << "\n";
        std::cout << "Image: " << image_path << "\n";
        std::cout << "Expected SHA-256: " << expected_hash << "\n\n";

        auto result = installer.install(install_info,
            [](int percent, const std::string& message) -> bool {
                std::cout << "\r[" << percent << "%] " << message << std::flush;
                return true;
            });

        std::cout << "\n";

        print_install_result(result);

        logger.info("cli", "Installation completed: " + install_status_to_string(result.status));

        return result.is_success() ? 0 : 1;

    } else if (command == "check") {
        auto config = ota::load_config(config_path);
        if (!config || !ota::validate_config(*config)) {
            std::cerr << "Error: Invalid configuration\n";
            return 1;
        }

        ota::UpdateManager manager;
        manager.set_server_url(server_url);
        manager.set_download_dir(download_dir);
        manager.set_device_config(*config);

        std::cout << "Checking for update...\n";

        auto info = manager.check_for_update();
        print_check_result(info);

        logger.info("cli", "Update check completed");

    } else if (command == "download") {
        auto config = ota::load_config(config_path);
        if (!config || !ota::validate_config(*config)) {
            std::cerr << "Error: Invalid configuration\n";
            return 1;
        }

        ota::UpdateManager manager;
        manager.set_server_url(server_url);
        manager.set_download_dir(download_dir);
        manager.set_device_config(*config);

        std::cout << "Checking for update...\n";

        auto info = manager.check_for_update();
        print_check_result(info);

        if (info.result != ota::UpdateCheckResult::UPDATE_AVAILABLE) {
            return 1;
        }

        std::cout << "Downloading version " << info.metadata.version << "...\n";

        auto result = manager.download_update(info.metadata.version,
            [](int64_t current, int64_t total) -> bool {
                if (total > 0) {
                    int percent = static_cast<int>((current * 100) / total);
                    std::cout << "\rDownload progress: " << percent << "%"
                              << " (" << current << "/" << total << " bytes)" << std::flush;
                }
                return true;
            });

        std::cout << "\n";

        if (result.success) {
            std::cout << "Download completed.\n";
            std::cout << "Temporary image: " << result.file_path << "\n";
            std::cout << "File size: " << result.file_size << " bytes\n";
            std::cout << "SHA-256: " << result.sha256 << "\n";

            ota::IntegrityValidator validator;
            auto validation = validator.validate_file(result.file_path, info.metadata.sha256,
                                                     info.metadata.image_size);
            print_verification_result(validation);

            if (!validation.is_valid()) {
                std::cerr << "Integrity verification failed. Image rejected.\n";
                validator.calculate_sha256(result.file_path);
                logger.error("cli", "Integrity verification failed: " + validation.error_message);
                return 1;
            }

            logger.info("cli", "Download and verification completed: " + result.file_path);
        } else {
            std::cerr << "Download failed: " << result.error_message << "\n";
            logger.error("cli", "Download failed: " + result.error_message);
            return 1;
        }

    } else if (command == "status") {
        ota::TransactionManagerConfig tm_config;
        tm_config.state_dir = state_dir + "/state";
        tm_config.history_dir = state_dir + "/state/history";
        tm_config.lock_file = state_dir + "/ota.lock";
        tm_config.max_history_entries = 10;

        ota::TransactionManager tm;
        tm.set_config(tm_config);

        if (tm.load_transaction()) {
            auto tx = tm.get_current_transaction();
            std::cout << "\nOTA Transaction Status\n\n";
            std::cout << "Transaction ID : " << tx.transaction_id << "\n";
            std::cout << "State          : " << ota::transaction_state_to_string(tx.state) << "\n";
            std::cout << "Target Version : " << tx.target_version << "\n";
            std::cout << "Source Version : " << tx.source_version << "\n";
            std::cout << "Hardware       : " << tx.hardware_version << "\n";
            std::cout << "Started        : " << tx.started_at << "\n";
            std::cout << "Updated        : " << tx.updated_at << "\n";

            if (!tx.error_code.empty()) {
                std::cout << "Error Code     : " << tx.error_code << "\n";
            }
            if (!tx.error_message.empty()) {
                std::cout << "Error Message  : " << tx.error_message << "\n";
            }
            std::cout << "\n";
        } else {
            std::cout << "\nOTA Transaction Status\n\n";
            std::cout << "State: IDLE\n";
            std::cout << "No active OTA transaction.\n\n";
        }

    } else if (command == "history") {
        ota::TransactionManagerConfig tm_config;
        tm_config.state_dir = state_dir + "/state";
        tm_config.history_dir = state_dir + "/state/history";
        tm_config.lock_file = state_dir + "/ota.lock";
        tm_config.max_history_entries = 10;

        ota::TransactionManager tm;
        tm.set_config(tm_config);
        tm.load_history();

        auto history = tm.get_history();

        std::cout << "\nOTA Transaction History\n\n";

        if (history.empty()) {
            std::cout << "No transaction history.\n\n";
        } else {
            for (size_t i = 0; i < history.size(); ++i) {
                const auto& entry = history[i];
                std::cout << (i + 1) << ". " << entry.version << "   " << entry.result
                         << " (" << entry.completed_at << ")\n";
            }
            std::cout << "\n";
        }

    } else if (command == "slots") {
        std::string subcommand = "status";
        if (argc > 2) {
            subcommand = argv[2];
        }

        ota::SlotConfig slot_config;
        slot_config.slots_dir = state_dir + "/slots";
        slot_config.state_file = state_dir + "/slots/global.json";
        slot_config.default_active_slot = ota::SlotId::SLOT_A;
        slot_config.default_version = "1.0.0";
        slot_config.default_hardware_version = "hw-v1";

        ota::SlotManager slot_manager;
        slot_manager.set_config(slot_config);

        if (subcommand == "init") {
            std::cout << "\nInitializing A/B slot system...\n\n";

            slot_manager.initialize_slots();

            auto slot_a = slot_manager.get_slot_a_info();
            auto slot_b = slot_manager.get_slot_b_info();

            std::cout << "Slot A:\n";
            std::cout << "  Version: " << (slot_a.version.empty() ? "none" : slot_a.version) << "\n";
            std::cout << "  State: " << ota::slot_state_to_string(slot_a.state) << "\n\n";

            std::cout << "Slot B:\n";
            std::cout << "  Version: " << (slot_b.version.empty() ? "none" : slot_b.version) << "\n";
            std::cout << "  State: " << ota::slot_state_to_string(slot_b.state) << "\n\n";

            std::cout << "Active slot: " << ota::slot_id_to_string(slot_manager.get_active_slot()) << "\n";
            std::cout << "Inactive slot: " << ota::slot_id_to_string(slot_manager.get_inactive_slot()) << "\n\n";

        } else {
            slot_manager.load_slot_state();

            auto slot_a = slot_manager.get_slot_a_info();
            auto slot_b = slot_manager.get_slot_b_info();

            std::cout << "\nA/B Slot Status\n\n";

            std::cout << "Slot A\n";
            std::cout << "------\n";
            std::cout << "State: " << ota::slot_state_to_string(slot_a.state) << "\n";
            std::cout << "Version: " << (slot_a.version.empty() ? "none" : slot_a.version) << "\n";
            if (!slot_a.sha256.empty()) {
                std::cout << "SHA-256: " << slot_a.sha256 << "\n";
            }
            std::cout << "Valid: " << (slot_a.is_valid() ? "YES" : "NO") << "\n\n";

            std::cout << "Slot B\n";
            std::cout << "------\n";
            std::cout << "State: " << ota::slot_state_to_string(slot_b.state) << "\n";
            std::cout << "Version: " << (slot_b.version.empty() ? "none" : slot_b.version) << "\n";
            if (!slot_b.sha256.empty()) {
                std::cout << "SHA-256: " << slot_b.sha256 << "\n";
            }
            std::cout << "Valid: " << (slot_b.is_valid() ? "YES" : "NO") << "\n\n";

            std::cout << "Active slot: " << ota::slot_id_to_string(slot_manager.get_active_slot()) << "\n";
            std::cout << "Inactive slot: " << ota::slot_id_to_string(slot_manager.get_inactive_slot()) << "\n\n";
        }

    } else if (command == "boot") {
        std::string subcommand = "status";
        std::string slot_arg;
        if (argc > 2) {
            subcommand = argv[2];
        }
        if (subcommand == "set" && argc > 3) {
            slot_arg = argv[3];
        }

        ota::SlotConfig slot_config;
        slot_config.slots_dir = state_dir + "/slots";
        slot_config.state_file = state_dir + "/slots/global.json";
        slot_config.default_active_slot = ota::SlotId::SLOT_A;
        slot_config.default_version = "1.0.0";
        slot_config.default_hardware_version = "hw-v1";

        ota::SlotManager slot_manager;
        slot_manager.set_config(slot_config);
        slot_manager.load_slot_state();

        ota::SimulatedBootConfig boot_config;
        boot_config.boot_state_dir = state_dir + "/boot";
        boot_config.boot_state_file = state_dir + "/boot/boot_state.json";

        ota::SimulatedBootControl boot_control;
        boot_control.set_config(boot_config);
        boot_control.initialize();

        if (subcommand == "status") {
            ota::BootState state = boot_control.get_boot_state();
            std::cout << "\nBoot Control Status\n\n";
            std::cout << "Current slot : " << ota::slot_id_to_string(state.current_slot) << "\n";
            std::cout << "Next slot    : " << (state.current_slot == state.next_slot ? "none" : ota::slot_id_to_string(state.next_slot)) << "\n\n";
            std::cout << "Boot attempts:\n";
            std::cout << "  A: " << state.boot_attempts.at(ota::SlotId::SLOT_A) << "\n";
            std::cout << "  B: " << state.boot_attempts.at(ota::SlotId::SLOT_B) << "\n\n";

        } else if (subcommand == "set") {
            if (slot_arg.empty()) {
                std::cerr << "Error: boot set requires a slot argument (A or B)\n";
                return 1;
            }

            ota::SlotId target_slot;
            if (slot_arg == "A" || slot_arg == "a") {
                target_slot = ota::SlotId::SLOT_A;
            } else if (slot_arg == "B" || slot_arg == "b") {
                target_slot = ota::SlotId::SLOT_B;
            } else {
                std::cerr << "Error: Invalid slot: " << slot_arg << "\n";
                std::cerr << "Valid slots: A, B\n";
                return 1;
            }

            if (!boot_control.validate_boot_target(target_slot, slot_manager)) {
                std::cerr << "Error: Cannot select slot " << slot_arg << " for boot.\n";
                ota::SlotInfo info = slot_manager.get_slot_info(target_slot);
                if (info.state == ota::SlotState::EMPTY) {
                    std::cerr << "Reason: Slot is empty\n";
                } else if (info.state == ota::SlotState::INVALID) {
                    std::cerr << "Reason: Slot is invalid\n";
                } else if (target_slot == boot_control.get_current_boot_slot()) {
                    std::cerr << "Reason: Slot is currently active\n";
                } else {
                    std::cerr << "Reason: Slot validation failed\n";
                }
                return 1;
            }

            if (!boot_control.set_next_boot_slot(target_slot)) {
                std::cerr << "Error: Failed to set next boot slot\n";
                return 1;
            }

            std::cout << "Next boot slot set to: " << slot_arg << "\n";

        } else if (subcommand == "clear") {
            if (!boot_control.clear_next_boot_slot()) {
                std::cerr << "Error: Failed to clear next boot slot\n";
                return 1;
            }
            std::cout << "Next boot selection cleared\n";

        } else if (subcommand == "simulate") {
            if (!boot_control.simulate_boot()) {
                std::cerr << "Error: Failed to simulate boot\n";
                return 1;
            }
            ota::BootState state = boot_control.get_boot_state();
            std::cout << "Boot simulated successfully\n";
            std::cout << "Current slot: " << ota::slot_id_to_string(state.current_slot) << "\n";
            std::cout << "Boot attempts for current slot: " << state.boot_attempts.at(state.current_slot) << "\n";

        } else {
            std::cerr << "Unknown boot subcommand: " << subcommand << "\n";
            std::cerr << "Valid subcommands: status, set, clear, simulate\n";
            return 1;
        }

    } else {
        std::cerr << "Unknown command: " << command << "\n";
        print_usage(argv[0]);
        return 1;
    }

    logger.shutdown();
    return 0;
}
