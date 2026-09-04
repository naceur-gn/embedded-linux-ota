cmake_minimum_required(VERSION 3.16)

find_package(GTest REQUIRED)

add_executable(ota_tests
    test_device_config.cpp
    test_device_state.cpp
    test_logger.cpp
    test_integration.cpp
    config/test_version_validation.cpp
    config/test_slot_validation.cpp
    device/test_initialization.cpp
    test_response_parser.cpp
    test_update_manager.cpp
    test_download_manager.cpp
    test_communication_integration.cpp
    test_failure_injection.cpp
    test_integrity_validator.cpp
    test_attack_simulation.cpp
    test_signature_verifier.cpp
    test_signature_attack.cpp
    test_installer.cpp
    test_install_attack.cpp
    test_transaction_state_machine.cpp
    test_transaction_manager.cpp
    test_transaction_persistence.cpp
    test_transaction_crash_recovery.cpp
    test_transaction_concurrency.cpp
    test_transaction_integration.cpp
    test_slot_manager.cpp
    test_slot_integration.cpp
    test_slot_failure_injection.cpp
    test_boot_control.cpp
    test_boot_integration.cpp
    test_boot_failure_injection.cpp
    test_bootloader.cpp
    test_bootloader_persistence.cpp
    test_bootloader_failure_injection.cpp
    test_bootloader_integration.cpp
)

target_link_libraries(ota_tests PRIVATE ota_core GTest::GTest GTest::Main)

add_test(NAME ota_tests COMMAND ota_tests)
