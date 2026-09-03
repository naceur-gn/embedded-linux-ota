#include "device/device_config.h"
#include "device/device_state.h"
#include "logging/logger.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

static const std::string DEFAULT_CONFIG_PATH = "/etc/ota/device.conf";
static const std::string DEFAULT_STATE_DIR = "/var/lib/ota";
static const std::string DEFAULT_LOG_DIR = "/var/log/ota";

static volatile bool running = true;

static void signal_handler(int signum) {
    (void)signum;
    running = false;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  -c, --config PATH    Path to device config (default: " << DEFAULT_CONFIG_PATH << ")\n"
              << "  -s, --state-dir PATH Path to state directory (default: " << DEFAULT_STATE_DIR << ")\n"
              << "  -l, --log-dir PATH   Path to log directory (default: " << DEFAULT_LOG_DIR << ")\n"
              << "  -h, --help           Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string config_path = DEFAULT_CONFIG_PATH;
    std::string state_dir = DEFAULT_STATE_DIR;
    std::string log_dir = DEFAULT_LOG_DIR;

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
        } else if (arg == "-l" || arg == "--log-dir") {
            if (i + 1 < argc) {
                log_dir = argv[++i];
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
    if (!logger.initialize(log_dir, ota::LogLevel::INFO)) {
        std::cerr << "Error: Failed to initialize logger\n";
        return 1;
    }

    logger.info("main", "OTA client starting");

    auto config = ota::load_config(config_path);
    if (!config) {
        logger.error("main", "Failed to load configuration from " + config_path);
        return 1;
    }

    if (!ota::validate_config(*config)) {
        logger.error("main", "Invalid configuration: " + ota::config_error_message(*config));
        return 1;
    }

    logger.info("main", "Configuration loaded: device=" + config->device_id +
               " hw=" + config->hardware_version +
               " sw=" + config->software_version +
               " slot=" + config->active_slot);

    auto state = ota::load_state(state_dir);
    if (!state) {
        logger.info("main", "No persistent state found, initializing");
        if (!ota::initialize_state(state_dir, config->software_version, config->active_slot)) {
            logger.error("main", "Failed to initialize persistent state");
            return 1;
        }
        state = ota::load_state(state_dir);
        if (!state) {
            logger.error("main", "Failed to load state after initialization");
            return 1;
        }
    }

    logger.info("main", "State loaded: version=" + state->current_version +
               " slot=" + state->active_slot +
               " state=" + ota::update_state_to_string(state->update_state));

    logger.info("main", "OTA client running (PID: " + std::to_string(getpid()) + ")");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (running) {
        sleep(1);
    }

    logger.info("main", "OTA client shutting down");
    logger.shutdown();

    return 0;
}
