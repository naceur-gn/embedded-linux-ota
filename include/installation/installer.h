#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include "validation/integrity_validator.h"
#include "security/signature_verifier.h"

namespace ota {

enum class InstallStatus {
    NOT_READY,
    READY,
    INSTALLING,
    INSTALLED,
    VERIFICATION_FAILED,
    INSTALLATION_FAILED,
    INSUFFICIENT_SPACE,
    PERMISSION_DENIED,
    PATH_TRAVERSAL_DETECTED,
    INVALID_ARTIFACT,
    STAGING_FAILED
};

std::string install_status_to_string(InstallStatus status);

struct InstallConfig {
    std::string staging_dir;
    std::string install_target;
    std::string state_dir;
    int64_t min_free_space_mb;
};

struct InstallInfo {
    std::string version;
    std::string image_path;
    std::string expected_sha256;
    int64_t expected_size;
    std::string metadata_json;
    std::string signature_base64;
    std::string public_key_path;
};

struct InstallResult {
    InstallStatus status;
    std::string installed_path;
    std::string calculated_sha256;
    std::string error_message;

    bool is_success() const { return status == InstallStatus::INSTALLED; }
};

struct InstallationState {
    std::string version;
    std::string status;
    std::string sha256;
    std::string installed_at;
    std::string target;
};

using InstallProgressCallback = std::function<bool(int percent, const std::string& message)>;

class InstallManager {
public:
    InstallManager();

    void set_config(const InstallConfig& config);

    InstallResult install(const InstallInfo& info,
                         InstallProgressCallback progress = nullptr);

    bool check_disk_space(int64_t required_bytes);

    bool validate_path(const std::string& path);

    bool is_safe_path(const std::string& base_dir, const std::string& path);

    std::string calculate_sha256(const std::string& file_path);

    bool sync_to_disk(const std::string& path);

    InstallationState load_installation_state(const std::string& version);

    bool save_installation_state(const InstallationState& state);

    bool cleanup_staging(const std::string& version);

    bool cleanup_install_target(const std::string& version);

    InstallConfig get_default_config();

private:
    bool create_staging_dir(const std::string& version);

    bool copy_to_staging(const std::string& source, const std::string& dest);

    bool verify_staged_artifact(const std::string& staged_path,
                               const std::string& expected_hash,
                               int64_t expected_size);

    bool finalize_installation(const std::string& staging_path,
                              const std::string& target_path);

    bool verify_installed_artifact(const std::string& installed_path,
                                  const std::string& expected_hash);

    bool is_symlink(const std::string& path);

    bool is_traversal_attempt(const std::string& path);

    int64_t get_available_space(const std::string& path);

    bool ensure_directory_exists(const std::string& path);

    bool set_restrictive_permissions(const std::string& path);

    InstallConfig config_;
    IntegrityValidator integrity_validator_;
};

}
