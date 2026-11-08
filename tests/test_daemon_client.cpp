#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>

#include "services/DaemonClient.h"
#include "services/FileCacheBuilder.h"

namespace fs = std::filesystem;

#define DAEMON_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

static volatile sig_atomic_t g_testSignalReceived = 0;

static void testSigusr1Handler(int) {
    g_testSignalReceived = 1;
}

static void test_lockfile_reading_valid() {
    std::cout << "[TEST] Running test_lockfile_reading_valid...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_valid";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path lockFile = sandboxDir / "fizzd.lock";
    {
        std::ofstream out(lockFile);
        out << getpid() << "\n";
    }
    chmod(lockFile.c_str(), 0600);

    fizz::DaemonClient client(lockFile.string());
    fizz::DaemonStatus status = client.readDaemonPid();
    DAEMON_TEST_ASSERT(status.pid == getpid());
    DAEMON_TEST_ASSERT(status.error.empty());
    DAEMON_TEST_ASSERT(client.isDaemonRunning());

    fizz::DaemonStatus fullStatus = client.getStatus();
    DAEMON_TEST_ASSERT(fullStatus.pid == getpid());
    DAEMON_TEST_ASSERT(fullStatus.isRunning);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_lockfile_reading_valid passed.\n";
}

static void test_lockfile_security_permissions() {
    std::cout << "[TEST] Running test_lockfile_security_permissions...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_perms";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path lockFile = sandboxDir / "fizzd.lock";
    {
        std::ofstream out(lockFile);
        out << getpid() << "\n";
    }

    fizz::DaemonClient client(lockFile.string());

    // 0644 (world readable) must be rejected
    chmod(lockFile.c_str(), 0644);
    fizz::DaemonStatus st644 = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st644.pid == -1);
    DAEMON_TEST_ASSERT(st644.error.find("0600") != std::string::npos);

    // 0666 must be rejected
    chmod(lockFile.c_str(), 0666);
    fizz::DaemonStatus st666 = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st666.pid == -1);

    // 0777 must be rejected
    chmod(lockFile.c_str(), 0777);
    fizz::DaemonStatus st777 = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st777.pid == -1);

    // 0700 (executable) must be rejected
    chmod(lockFile.c_str(), 0700);
    fizz::DaemonStatus st700 = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st700.pid == -1);

    // Exact 0600 must succeed
    chmod(lockFile.c_str(), 0600);
    fizz::DaemonStatus st600 = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st600.pid == getpid());

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_lockfile_security_permissions passed.\n";
}

static void test_lockfile_symlink_rejection() {
    std::cout << "[TEST] Running test_lockfile_symlink_rejection...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_symlink";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path realFile = sandboxDir / "real_file.pid";
    {
        std::ofstream out(realFile);
        out << getpid() << "\n";
    }
    chmod(realFile.c_str(), 0600);

    fs::path symlinkLock = sandboxDir / "fizzd.lock";
    int res = symlink(realFile.c_str(), symlinkLock.c_str());
    DAEMON_TEST_ASSERT(res == 0);

    fizz::DaemonClient client(symlinkLock.string());
    fizz::DaemonStatus st = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st.pid == -1);
    DAEMON_TEST_ASSERT(st.error.find("symbolic link") != std::string::npos ||
                       st.error.find("symlink") != std::string::npos);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_lockfile_symlink_rejection passed.\n";
}

static void test_lockfile_corrupt_content() {
    std::cout << "[TEST] Running test_lockfile_corrupt_content...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_corrupt";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path lockFile = sandboxDir / "fizzd.lock";
    fizz::DaemonClient client(lockFile.string());

    // 1. Empty file
    {
        std::ofstream out(lockFile);
    }
    chmod(lockFile.c_str(), 0600);
    DAEMON_TEST_ASSERT(client.readDaemonPid().pid == -1);

    // 2. Negative PID
    {
        std::ofstream out(lockFile, std::ios::trunc);
        out << "-42\n";
    }
    chmod(lockFile.c_str(), 0600);
    DAEMON_TEST_ASSERT(client.readDaemonPid().pid == -1);

    // 3. String / non-numeric
    {
        std::ofstream out(lockFile, std::ios::trunc);
        out << "not_a_pid\n";
    }
    chmod(lockFile.c_str(), 0600);
    DAEMON_TEST_ASSERT(client.readDaemonPid().pid == -1);

    // 4. Zero / PID 1 (init / kernel)
    {
        std::ofstream out(lockFile, std::ios::trunc);
        out << "0\n";
    }
    chmod(lockFile.c_str(), 0600);
    DAEMON_TEST_ASSERT(client.readDaemonPid().pid == -1);

    // 5. Huge PID exceeding linux limit (4194304)
    {
        std::ofstream out(lockFile, std::ios::trunc);
        out << "99999999\n";
    }
    chmod(lockFile.c_str(), 0600);
    DAEMON_TEST_ASSERT(client.readDaemonPid().pid == -1);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_lockfile_corrupt_content passed.\n";
}

static void test_dead_pid_detection() {
    std::cout << "[TEST] Running test_dead_pid_detection...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_dead";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path lockFile = sandboxDir / "fizzd.lock";
    // Write PID 4194000 which is virtually guaranteed to not exist
    {
        std::ofstream out(lockFile);
        out << "4194000\n";
    }
    chmod(lockFile.c_str(), 0600);

    fizz::DaemonClient client(lockFile.string());
    fizz::DaemonStatus st = client.readDaemonPid();
    DAEMON_TEST_ASSERT(st.pid == 4194000);
    DAEMON_TEST_ASSERT(!client.isDaemonRunning());

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_dead_pid_detection passed.\n";
}

static void test_pid_signaling_sigusr1() {
    std::cout << "[TEST] Running test_pid_signaling_sigusr1...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_signal";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path lockFile = sandboxDir / "fizzd.lock";
    {
        std::ofstream out(lockFile);
        out << getpid() << "\n";
    }
    chmod(lockFile.c_str(), 0600);

    // Register test signal handler
    g_testSignalReceived = 0;
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = testSigusr1Handler;
    sigaction(SIGUSR1, &sa, nullptr);

    fizz::DaemonClient client(lockFile.string());
    fizz::DaemonTriggerStatus triggerResult = client.triggerRebuild();
    DAEMON_TEST_ASSERT(triggerResult == fizz::DaemonTriggerStatus::SuccessSignal);
    DAEMON_TEST_ASSERT(g_testSignalReceived == 1);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_pid_signaling_sigusr1 passed.\n";
}

static void test_cooldown_rate_limiting() {
    std::cout << "[TEST] Running test_cooldown_rate_limiting...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_cooldown";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path lockFile = sandboxDir / "fizzd.lock";
    {
        std::ofstream out(lockFile);
        out << getpid() << "\n";
    }
    chmod(lockFile.c_str(), 0600);

    fizz::DaemonClient client(lockFile.string());
    DAEMON_TEST_ASSERT(!client.isCooldownActive());
    DAEMON_TEST_ASSERT(client.getCooldownRemainingSeconds() == 0.0);

    // First trigger must succeed
    fizz::DaemonTriggerStatus res1 = client.triggerRebuild();
    DAEMON_TEST_ASSERT(res1 == fizz::DaemonTriggerStatus::SuccessSignal);

    // Immediate second trigger must be rate-limited by 5s cooldown
    DAEMON_TEST_ASSERT(client.isCooldownActive());
    DAEMON_TEST_ASSERT(client.getCooldownRemainingSeconds() > 0.0);
    DAEMON_TEST_ASSERT(client.getCooldownRemainingSeconds() <= 5.0);

    fizz::DaemonTriggerStatus res2 = client.triggerRebuild();
    DAEMON_TEST_ASSERT(res2 == fizz::DaemonTriggerStatus::CooldownIgnored);

    // Reset cooldown programmatically
    client.resetCooldown();
    DAEMON_TEST_ASSERT(!client.isCooldownActive());
    DAEMON_TEST_ASSERT(client.getCooldownRemainingSeconds() == 0.0);

    // Trigger should now succeed again
    fizz::DaemonTriggerStatus res3 = client.triggerRebuild();
    DAEMON_TEST_ASSERT(res3 == fizz::DaemonTriggerStatus::SuccessSignal);

    // Fast-expiration test with custom short cooldown
    client.setCooldownDuration(std::chrono::milliseconds(50));
    client.triggerRebuild(); // activates 50ms cooldown
    DAEMON_TEST_ASSERT(client.isCooldownActive());
    usleep(60000); // sleep 60ms
    DAEMON_TEST_ASSERT(!client.isCooldownActive());

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_cooldown_rate_limiting passed.\n";
}

static void test_systemctl_fallback_execution_path() {
    std::cout << "[TEST] Running test_systemctl_fallback_execution_path...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_daemon_fallback";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    // Lockfile does not exist, so daemon PID cannot be found
    fs::path nonExistentLock = sandboxDir / "fizzd.lock";
    fizz::DaemonClient client(nonExistentLock.string());

    std::vector<std::string> invokedArgs;
    int mockExitCode = 0;
    client.setSystemctlRunner([&](const std::vector<std::string>& args) -> int {
        invokedArgs = args;
        return mockExitCode;
    });

    // 1. Fallback succeeds when systemctl returns 0
    mockExitCode = 0;
    fizz::DaemonTriggerStatus status1 = client.triggerRebuild();
    DAEMON_TEST_ASSERT(status1 == fizz::DaemonTriggerStatus::SuccessSystemd);
    DAEMON_TEST_ASSERT(invokedArgs.size() == 3);
    DAEMON_TEST_ASSERT(invokedArgs[0] == "--user");
    DAEMON_TEST_ASSERT(invokedArgs[1] == "restart");
    DAEMON_TEST_ASSERT(invokedArgs[2] == "fizzd");

    // 2. Fallback fails when systemctl returns non-zero
    client.resetCooldown();
    mockExitCode = 1;
    invokedArgs.clear();
    fizz::DaemonTriggerStatus status2 = client.triggerRebuild();
    DAEMON_TEST_ASSERT(status2 == fizz::DaemonTriggerStatus::DaemonNotRunning);
    DAEMON_TEST_ASSERT(invokedArgs.size() == 3);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_systemctl_fallback_execution_path passed.\n";
}

static void test_index_daemon_sigusr1_handler() {
    std::cout << "[TEST] Running test_index_daemon_sigusr1_handler...\n";

    // Test the FileCacheBuilder SIGUSR1 handler setting g_RebuildRequested
    setRebuildRequested(false);
    DAEMON_TEST_ASSERT(!isRebuildRequested());

    setRebuildRequested(true);
    DAEMON_TEST_ASSERT(isRebuildRequested());

    setRebuildRequested(false);
    installDaemonSignalHandlers();
    raise(SIGUSR1);
    DAEMON_TEST_ASSERT(isRebuildRequested());

    // Reset flag
    setRebuildRequested(false);
    DAEMON_TEST_ASSERT(!isRebuildRequested());

    std::cout << "[PASS] test_index_daemon_sigusr1_handler passed.\n";
}

void run_daemon_client_tests() {
    std::cout << "--- Running DaemonClient Unit Tests ---\n";
    test_lockfile_reading_valid();
    test_lockfile_security_permissions();
    test_lockfile_symlink_rejection();
    test_lockfile_corrupt_content();
    test_dead_pid_detection();
    test_pid_signaling_sigusr1();
    test_cooldown_rate_limiting();
    test_systemctl_fallback_execution_path();
    test_index_daemon_sigusr1_handler();
    std::cout << "--- DaemonClient Unit Tests Passed ---\n";
}
