#include "installation/installer.h"
#include "logging/logger.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <libgen.h>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>

namespace ota {

std::string install_status_to_string(InstallStatus status) {
    switch (status) {
        case InstallStatus::NOT_READY:
            return "NOT_READY";
        case InstallStatus::READY:
            return "READY";
        case InstallStatus::INSTALLING:
            return "INSTALLING";
        case InstallStatus::INSTALLED:
            return "INSTALLED";
        case InstallStatus::VERIFICATION_FAILED:
            return "VERIFICATION_FAILED";
        case InstallStatus::INSTALLATION_FAILED:
            return "INSTALLATION_FAILED";
        case InstallStatus::INSUFFICIENT_SPACE:
            return "INSUFFICIENT_SPACE";
        case InstallStatus::PERMISSION_DENIED:
            return "PERMISSION_DENIED";
        case InstallStatus::PATH_TRAVERSAL_DETECTED:
            return "PATH_TRAVERSAL_DETECTED";
        case InstallStatus::INVALID_ARTIFACT:
            return "INVALID_ARTIFACT";
        case InstallStatus::STAGING_FAILED:
            return "STAGING_FAILED";
        default:
            return "UNKNOWN";
    }
}

InstallManager::InstallManager() {
    config_ = get_default_config();
}

void InstallManager::set_config(const InstallConfig& config) {
    config_ = config;
}

InstallConfig InstallManager::get_default_config() {
    InstallConfig config;
    config.staging_dir = "/var/lib/ota/staging";
    config.install_target = "/var/lib/ota/install-target";
    config.state_dir = "/var/lib/ota/state";
    config.min_free_space_mb = 100;
    return config;
}

InstallResult InstallManager::install(const InstallInfo& info,
                                     InstallProgressCallback progress) {
    auto& logger = Logger::instance();
    InstallResult result;

    logger.info("installer", "Starting installation for version: " + info.version);

    if (progress) {
        if (!progress(0, "Validating installation prerequisites...")) {
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    if (info.version.empty() || info.image_path.empty()) {
        result.status = InstallStatus::INVALID_ARTIFACT;
        result.error_message = "Invalid installation info: missing version or image path";
        logger.error("installer", result.error_message);
        return result;
    }

    if (!validate_path(info.version)) {
        result.status = InstallStatus::PATH_TRAVERSAL_DETECTED;
        result.error_message = "Invalid version string: potential path traversal";
        logger.error("installer", result.error_message);
        return result;
    }

    struct stat image_stat;
    if (stat(info.image_path.c_str(), &image_stat) != 0) {
        result.status = InstallStatus::INVALID_ARTIFACT;
        result.error_message = "Image file not found: " + info.image_path;
        logger.error("installer", result.error_message);
        return result;
    }

    if (!S_ISREG(image_stat.st_mode)) {
        result.status = InstallStatus::INVALID_ARTIFACT;
        result.error_message = "Image is not a regular file: " + info.image_path;
        logger.error("installer", result.error_message);
        return result;
    }

    int64_t required_space = info.expected_size > 0 ? info.expected_size : image_stat.st_size;
    required_space *= 2;

    if (!check_disk_space(required_space)) {
        result.status = InstallStatus::INSUFFICIENT_SPACE;
        result.error_message = "Insufficient disk space for installation";
        logger.error("installer", result.error_message);
        return result;
    }

    if (progress) {
        if (!progress(10, "Creating staging directory...")) {
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    if (!create_staging_dir(info.version)) {
        result.status = InstallStatus::STAGING_FAILED;
        result.error_message = "Failed to create staging directory";
        logger.error("installer", result.error_message);
        return result;
    }

    std::string staging_path = config_.staging_dir + "/" + info.version + "/image.bin";

    if (progress) {
        if (!progress(20, "Copying update to staging...")) {
            cleanup_staging(info.version);
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    if (!copy_to_staging(info.image_path, staging_path)) {
        cleanup_staging(info.version);
        result.status = InstallStatus::STAGING_FAILED;
        result.error_message = "Failed to copy update to staging";
        logger.error("installer", result.error_message);
        return result;
    }

    if (progress) {
        if (!progress(50, "Verifying staged artifact...")) {
            cleanup_staging(info.version);
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    if (!verify_staged_artifact(staging_path, info.expected_sha256, info.expected_size)) {
        cleanup_staging(info.version);
        result.status = InstallStatus::VERIFICATION_FAILED;
        result.error_message = "Staged artifact verification failed";
        logger.error("installer", result.error_message);
        return result;
    }

    if (progress) {
        if (!progress(70, "Installing to target...")) {
            cleanup_staging(info.version);
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    std::string target_path = config_.install_target + "/" + info.version;

    if (!finalize_installation(staging_path, target_path)) {
        cleanup_staging(info.version);
        result.status = InstallStatus::INSTALLATION_FAILED;
        result.error_message = "Failed to finalize installation";
        logger.error("installer", result.error_message);
        return result;
    }

    if (progress) {
        if (!progress(85, "Synchronizing filesystem...")) {
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    if (!sync_to_disk(target_path)) {
        result.status = InstallStatus::INSTALLATION_FAILED;
        result.error_message = "Failed to synchronize filesystem";
        logger.error("installer", result.error_message);
        return result;
    }

    if (progress) {
        if (!progress(90, "Verifying installed artifact...")) {
            result.status = InstallStatus::INSTALLATION_FAILED;
            result.error_message = "Installation cancelled by user";
            return result;
        }
    }

    if (!verify_installed_artifact(target_path + "/image.bin", info.expected_sha256)) {
        result.status = InstallStatus::VERIFICATION_FAILED;
        result.error_message = "Post-installation verification failed";
        logger.error("installer", result.error_message);
        return result;
    }

    InstallationState state;
    state.version = info.version;
    state.status = "installed";
    state.sha256 = info.expected_sha256;
    state.installed_at = std::to_string(std::time(nullptr));
    state.target = target_path;

    if (!save_installation_state(state)) {
        result.status = InstallStatus::INSTALLATION_FAILED;
        result.error_message = "Failed to save installation state";
        logger.error("installer", result.error_message);
        return result;
    }

    cleanup_staging(info.version);

    result.status = InstallStatus::INSTALLED;
    result.installed_path = target_path + "/image.bin";
    result.calculated_sha256 = info.expected_sha256;

    if (progress) {
        progress(100, "Installation completed successfully");
    }

    logger.info("installer", "Installation completed for version: " + info.version);

    return result;
}

bool InstallManager::check_disk_space(int64_t required_bytes) {
    struct statvfs stat;
    if (statvfs(config_.install_target.c_str(), &stat) != 0) {
        if (statvfs(config_.staging_dir.c_str(), &stat) != 0) {
            return false;
        }
    }

    int64_t available_bytes = static_cast<int64_t>(stat.f_bavail) * stat.f_frsize;
    int64_t required_with_margin = required_bytes + (config_.min_free_space_mb * 1024 * 1024);

    return available_bytes >= required_with_margin;
}

bool InstallManager::validate_path(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    if (is_traversal_attempt(path)) {
        return false;
    }

    if (path.find('\0') != std::string::npos) {
        return false;
    }

    return true;
}

bool InstallManager::is_safe_path(const std::string& base_dir, const std::string& path) {
    std::string resolved_base = base_dir;
    if (resolved_base.back() != '/') {
        resolved_base += '/';
    }

    std::string full_path = resolved_base + path;

    if (!validate_path(full_path)) {
        return false;
    }

    char resolved_path[PATH_MAX];
    char* result = realpath(full_path.c_str(), resolved_path);
    if (result == nullptr) {
        char base_resolved[PATH_MAX];
        char* base_result = realpath(base_dir.c_str(), base_resolved);
        if (base_result == nullptr) {
            return true;
        }
        std::string normalized_base = base_resolved;
        if (normalized_base.back() != '/') {
            normalized_base += '/';
        }
        return full_path.find(normalized_base) == 0;
    }

    std::string normalized_base;
    char base_resolved[PATH_MAX];
    if (realpath(base_dir.c_str(), base_resolved)) {
        normalized_base = base_resolved;
    } else {
        normalized_base = base_dir;
    }

    if (normalized_base.back() != '/') {
        normalized_base += '/';
    }

    return std::string(resolved_path).find(normalized_base) == 0;
}

std::string InstallManager::calculate_sha256(const std::string& file_path) {
    return integrity_validator_.calculate_sha256(file_path);
}

bool InstallManager::sync_to_disk(const std::string& path) {
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) != 0) {
        return false;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            return false;
        }

        bool success = (fsync(fd) == 0);
        close(fd);
        return success;
    } else {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            return false;
        }

        bool success = (fsync(fd) == 0);
        close(fd);
        return success;
    }
}

InstallationState InstallManager::load_installation_state(const std::string& version) {
    InstallationState state;
    std::string state_file = config_.state_dir + "/installation_" + version + ".json";

    std::ifstream file(state_file);
    if (!file.good()) {
        return state;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto extract_value = [&content](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return "";

        pos = content.find(":", pos);
        if (pos == std::string::npos) return "";

        pos = content.find("\"", pos + 1);
        if (pos == std::string::npos) return "";

        size_t end = content.find("\"", pos + 1);
        if (end == std::string::npos) return "";

        return content.substr(pos + 1, end - pos - 1);
    };

    state.version = extract_value("version");
    state.status = extract_value("status");
    state.sha256 = extract_value("sha256");
    state.installed_at = extract_value("installed_at");
    state.target = extract_value("target");

    return state;
}

bool InstallManager::save_installation_state(const InstallationState& state) {
    ensure_directory_exists(config_.state_dir);

    std::string state_file = config_.state_dir + "/installation_" + state.version + ".json";

    std::ofstream file(state_file);
    if (!file.good()) {
        return false;
    }

    file << "{\n";
    file << "  \"version\": \"" << state.version << "\",\n";
    file << "  \"status\": \"" << state.status << "\",\n";
    file << "  \"sha256\": \"" << state.sha256 << "\",\n";
    file << "  \"installed_at\": \"" << state.installed_at << "\",\n";
    file << "  \"target\": \"" << state.target << "\"\n";
    file << "}\n";

    return file.good();
}

bool InstallManager::cleanup_staging(const std::string& version) {
    if (!validate_path(version)) {
        return false;
    }

    std::string staging_path = config_.staging_dir + "/" + version;

    DIR* dir = opendir(staging_path.c_str());
    if (dir == nullptr) {
        return true;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string file_path = staging_path + "/" + entry->d_name;
        unlink(file_path.c_str());
    }
    closedir(dir);

    rmdir(staging_path.c_str());

    return true;
}

bool InstallManager::cleanup_install_target(const std::string& version) {
    if (!validate_path(version)) {
        return false;
    }

    std::string target_path = config_.install_target + "/" + version;

    DIR* dir = opendir(target_path.c_str());
    if (dir == nullptr) {
        return true;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string file_path = target_path + "/" + entry->d_name;
        unlink(file_path.c_str());
    }
    closedir(dir);

    rmdir(target_path.c_str());

    return true;
}

bool InstallManager::create_staging_dir(const std::string& version) {
    std::string staging_path = config_.staging_dir + "/" + version;

    if (mkdir(config_.staging_dir.c_str(), 0755) != 0) {
        if (errno != EEXIST) {
            return false;
        }
    }

    if (mkdir(staging_path.c_str(), 0755) != 0) {
        if (errno != EEXIST) {
            return false;
        }
    }

    return true;
}

bool InstallManager::copy_to_staging(const std::string& source, const std::string& dest) {
    std::ifstream src_file(source, std::ios::binary);
    if (!src_file.good()) {
        return false;
    }

    std::ofstream dest_file(dest, std::ios::binary);
    if (!dest_file.good()) {
        return false;
    }

    char buffer[8192];
    while (src_file.read(buffer, sizeof(buffer))) {
        dest_file.write(buffer, src_file.gcount());
    }
    dest_file.write(buffer, src_file.gcount());

    dest_file.flush();

    return dest_file.good();
}

bool InstallManager::verify_staged_artifact(const std::string& staged_path,
                                           const std::string& expected_hash,
                                           int64_t expected_size) {
    auto result = integrity_validator_.validate_file(staged_path, expected_hash, expected_size);
    return result.is_valid();
}

bool InstallManager::finalize_installation(const std::string& staging_path,
                                         const std::string& target_path) {
    if (mkdir(config_.install_target.c_str(), 0755) != 0) {
        if (errno != EEXIST) {
            return false;
        }
    }

    if (mkdir(target_path.c_str(), 0755) != 0) {
        if (errno != EEXIST) {
            return false;
        }
    }

    std::string final_path = target_path + "/image.bin";

    if (rename(staging_path.c_str(), final_path.c_str()) != 0) {
        std::ifstream src(staging_path, std::ios::binary);
        if (!src.good()) {
            return false;
        }

        std::ofstream dst(final_path, std::ios::binary);
        if (!dst.good()) {
            return false;
        }

        char buffer[8192];
        while (src.read(buffer, sizeof(buffer))) {
            dst.write(buffer, src.gcount());
        }
        dst.write(buffer, src.gcount());

        dst.flush();

        if (!dst.good()) {
            return false;
        }

        unlink(staging_path.c_str());
    }

    return true;
}

bool InstallManager::verify_installed_artifact(const std::string& installed_path,
                                              const std::string& expected_hash) {
    return integrity_validator_.verify_sha256(installed_path, expected_hash);
}

bool InstallManager::is_symlink(const std::string& path) {
    struct stat stat_buf;
    if (lstat(path.c_str(), &stat_buf) != 0) {
        return false;
    }
    return S_ISLNK(stat_buf.st_mode);
}

bool InstallManager::is_traversal_attempt(const std::string& path) {
    if (path.find("..") != std::string::npos) {
        return true;
    }

    if (!path.empty() && path[0] == '/') {
        return true;
    }

    if (path.find("//") != std::string::npos) {
        return true;
    }

    return false;
}

int64_t InstallManager::get_available_space(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        return -1;
    }

    return static_cast<int64_t>(stat.f_bavail) * stat.f_frsize;
}

bool InstallManager::ensure_directory_exists(const std::string& path) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) == 0) {
        return S_ISDIR(stat_buf.st_mode);
    }

    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

bool InstallManager::set_restrictive_permissions(const std::string& path) {
    return chmod(path.c_str(), 0755) == 0;
}

}
