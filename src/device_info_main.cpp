#include "device/device_config.h"
#include "device/device_state.h"
#include "logging/logger.h"
#include <iostream>
#include <iomanip>

static const std::string DEFAULT_CONFIG_PATH = "/etc/ota/device.conf";
static const std::string DEFAULT_STATE_DIR = "/var/lib/ota";
static const std::string DEFAULT_LOG_DIR = "/var/log/ota";

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  -c, --config PATH    Path to device config (default: " << DEFAULT_CONFIG_PATH << ")\n"
              << "  -s, --state-dir PATH Path to state directory (default: " << DEFAULT_STATE_DIR << ")\n"
              << "  -h, --help           Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string config_path = DEFAULT_CONFIG_PATH;
    std::string state_dir = DEFAULT_STATE_DIR;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a path argument\n";
                return 1;
            }
        } else if (arg == "-s" || arg == "--state-dir") {
            if (i + 1 < argc) {
                state_dir = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a path argument\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    auto& logger = ota::Logger::instance();
    logger.initialize(DEFAULT_LOG_DIR, ota::LogLevel::INFO);

    auto config = ota::load_config(config_path);
    if (!config) {
        std::cerr << "Error: Failed to load config from " << config_path << "\n";
        return 1;
    }

    if (!ota::validate_config(*config)) {
        std::cerr << "Error: Invalid configuration: " << ota::config_error_message(*config) << "\n";
        return 1;
    }

    std::string state_version = config->software_version;
    std::string state_slot = config->active_slot;

    auto state = ota::load_state(state_dir);
    if (state) {
        state_version = state->current_version.empty() ? config->software_version : state->current_version;
        state_slot = state->active_slot.empty() ? config->active_slot : state->active_slot;
    }

    std::cout << "Device ID:        " << config->device_id << "\n"
              << "Hardware Version: " << config->hardware_version << "\n"
              << "Software Version: " << state_version << "\n"
              << "Active Slot:      " << state_slot << "\n"
              << "OTA State:        " << ota::update_state_to_string(
                     state ? state->update_state : ota::UpdateState::IDLE) << "\n";

    logger.info("device-info", "Device information displayed");
    logger.shutdown();

    return 0;
}
