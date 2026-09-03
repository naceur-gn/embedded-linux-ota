# Task 02 Testing Report

## Environment

| Property | Value |
|----------|-------|
| OS | Ubuntu (Linux) |
| Compiler | GCC 15.2.0 |
| CMake | 3.16+ |
| C++ Standard | C++17 |
| Testing Framework | Google Test 1.17.0 |

## Test Structure

```
tests/
├── device/
│   └── test_initialization.cpp    (6 tests)
├── config/
│   ├── test_version_validation.cpp (11 tests)
│   └── test_slot_validation.cpp   (7 tests)
├── state/
│   └── (covered in test_device_state.cpp)
├── logging/
│   └── (covered in test_logger.cpp)
├── test_device_config.cpp         (10 tests)
├── test_device_state.cpp          (5 tests)
├── test_logger.cpp                (6 tests)
├── test_integration.cpp           (3 tests)
└── CMakeLists.txt
```

## Test Results

| Test | Expected | Result | Status |
|------|----------|--------|--------|
| Valid configuration loads | Accepted | Correct device_id, hw_version, sw_version, slot loaded | PASS |
| Missing device_id | Rejected | validate_config returns false | PASS |
| Empty device_id | Rejected | validate_config returns false | PASS |
| Missing hardware_version | Rejected | validate_config returns false | PASS |
| Invalid software version (1.0) | Rejected | validate_config returns false | PASS |
| Invalid software version (abc) | Rejected | validate_config returns false | PASS |
| Invalid software version (1.x.0) | Rejected | validate_config returns false | PASS |
| Invalid software version (1.0.0.1) | Rejected | validate_config returns false | PASS |
| Leading zero version (01.0.0) | Rejected | validate_config returns false | PASS |
| Valid version 1.0.0 | Accepted | validate_config returns true | PASS |
| Valid version 1.1.0 | Accepted | validate_config returns true | PASS |
| Valid version 2.0.0 | Accepted | validate_config returns true | PASS |
| Valid version 10.25.3 | Accepted | validate_config returns true | PASS |
| Slot A accepted | Accepted | validate_config returns true | PASS |
| Slot B accepted | Accepted | validate_config returns true | PASS |
| Slot C rejected | Rejected | validate_config returns false | PASS |
| Empty slot rejected | Rejected | validate_config returns false | PASS |
| Lowercase slot rejected | Rejected | validate_config returns false | PASS |
| Numeric slot rejected | Rejected | validate_config returns false | PASS |
| Persistent state initialization | Creates state file | State file created with correct values | PASS |
| State save and load | Preserves all fields | All fields survive round-trip | PASS |
| State directory creation | Creates nested directories | Directory created automatically | PASS |
| State loads from nonexistent dir | Returns nullopt | load_state returns nullopt | PASS |
| UpdateState to string | Correct strings | All 13 states map correctly | PASS |
| Logger initialization | Creates log file | Log file created and writable | PASS |
| Logger writes messages | Correct format | Timestamp, level, component, message present | PASS |
| Logger filters by level | Only WARN+ logged | 2 lines written (WARN, ERROR) | PASS |
| Logger set level | Changes filtering | Only ERROR logged after set_level(ERROR) | PASS |
| Log level conversions | Correct strings | Both directions work correctly | PASS |
| Valid config initialization | Succeeds | State initialized with correct values | PASS |
| Invalid config initialization | Fails safely | Error message returned, no crash | PASS |
| Missing config file | Returns nullopt | load_config returns nullopt | PASS |
| Logger failure handling | Returns false | initialize returns false on bad path | PASS |
| Device info reflects config | Correct values | State version matches config version | PASS |
| Complete lifecycle integration | All steps pass | Config → State → Logger → Verify → Shutdown | PASS |
| Config change affects identity | Values update | New config values reflected in loaded config | PASS |
| State survives restart | Persists correctly | State survives Logger shutdown and reload | PASS |

**Total: 48 tests PASSED, 0 tests FAILED**

## Automated Test Command

```bash
# Build tests
mkdir -p build && cd build
cmake ..
cmake --build .

# Run all tests
./tests/ota_tests

# Or using ctest
ctest --test-dir build --output-on-failure
```

## systemd Test Procedure

### Prerequisites

```bash
# Build and install
cmake --build build
sudo scripts/setup-user.sh
sudo scripts/install.sh
```

### Test 1: Service Starts Successfully

```bash
# Start the service
sudo systemctl start ota-client

# Check status
sudo systemctl status ota-client

# Expected output:
# Active: active (running)
# CGroup: /system.slice/ota-client.service
```

### Test 2: Service Stays Running

```bash
# Wait 5 seconds
sleep 5

# Verify still running
sudo systemctl is-active ota-client

# Expected: active
```

### Test 3: Logs Generated

```bash
# Check application logs
sudo cat /var/log/ota/ota.log

# Expected: Lines with timestamps and [INFO] level
```

### Test 4: Service Restarts After Crash

```bash
# Get the main PID
PID=$(sudo systemctl show -p MainPID ota-client | cut -d= -f2)

# Kill the process to simulate crash
sudo kill -9 $PID

# Wait for systemd to restart
sleep 6

# Check new PID is different
NEW_PID=$(sudo systemctl show -p MainPID ota-client | cut -d= -f2)

# Verify service is running with new PID
sudo systemctl is-active ota-client

# Expected: active, PID changed
```

### Test 5: Clean Shutdown

```bash
# Stop the service
sudo systemctl stop ota-client

# Verify stopped
sudo systemctl is-active ota-client

# Check shutdown logged
sudo tail -5 /var/log/ota/ota.log

# Expected: "OTA client shutting down" message
```

### systemd Test Commands Summary

```bash
sudo systemctl start ota-client      # Start
sudo systemctl status ota-client     # Status
sudo systemctl stop ota-client       # Stop
sudo systemctl restart ota-client    # Restart
sudo journalctl -u ota-client        # View journal
sudo cat /var/log/ota/ota.log        # View app logs
```
