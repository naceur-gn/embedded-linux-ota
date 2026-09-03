cmake_minimum_required(VERSION 3.16)
project(embedded-linux-ota VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

option(BUILD_TESTS "Build unit tests" ON)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(ota_core STATIC
    src/device/device_config.cpp
    src/device/device_state.cpp
    src/logging/logger.cpp
    src/network/http_client.cpp
    src/client/response_parser.cpp
    src/download/download_manager.cpp
    src/client/update_manager.cpp
    src/validation/integrity_validator.cpp
    src/security/signature_verifier.cpp
    src/installation/installer.cpp
    src/transaction/transaction_state_machine.cpp
    src/transaction/transaction_manager.cpp
    src/slot/slot_manager.cpp
    src/boot/simulated_boot_control.cpp
)

target_include_directories(ota_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ota_core PUBLIC
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

add_executable(ota-client
    src/main.cpp
)

target_link_libraries(ota-client PRIVATE ota_core)

add_executable(ota-device-info
    src/device_info_main.cpp
)

target_link_libraries(ota-device-info PRIVATE ota_core)

add_executable(ota-cli
    src/cli_main.cpp
)

target_link_libraries(ota-cli PRIVATE ota_core)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS ota-client ota-device-info ota-cli
    RUNTIME DESTINATION /opt/ota/bin
)

install(FILES configs/device.conf
    DESTINATION /etc/ota
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ
)

install(DIRECTORY
    DESTINATION /var/lib/ota
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE
)

install(DIRECTORY
    DESTINATION /var/log/ota
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE
)
