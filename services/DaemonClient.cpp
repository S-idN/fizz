#include "DaemonClient.h"
#include "Config.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace fizz {

DaemonClient::DaemonClient() = default;

DaemonClient::DaemonClient(const std::string& customLockPath)
    : m_customLockPath(customLockPath) {}

void DaemonClient::setCustomLockPath(const std::string& path) {
    m_customLockPath = path;
}

std::string DaemonClient::resolveLockPath() const {
    if (!m_customLockPath.empty()) {
        std::error_code ec;
        if (fs::is_directory(m_customLockPath, ec)) {
            return m_customLockPath + "/fizzd.lock";
        }
        return m_customLockPath;
    }

    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0') {
        std::string xdgLock = std::string(xdg) + "/fizzd.lock";
        if (access(xdgLock.c_str(), F_OK) == 0) {
            return xdgLock;
        }

        // Also check if getUserRuntimeDir() has fizzd.lock
        std::string userRt = getUserRuntimeDir();
        if (!userRt.empty()) {
            std::string userLock = userRt + "/fizzd.lock";
            if (access(userLock.c_str(), F_OK) == 0) {
                return userLock;
            }
        }
        return xdgLock;
    }

    std::string userRt = getUserRuntimeDir();
    if (!userRt.empty()) {
        return userRt + "/fizzd.lock";
    }

    return "/tmp/fizz-" + std::to_string(getuid()) + "/fizzd.lock";
}

DaemonStatus DaemonClient::readDaemonPid() const {
    DaemonStatus status;
    std::string lockPath = resolveLockPath();
    if (lockPath.empty()) {
        status.error = "Lockfile path could not be resolved";
        return status;
    }

    // 1. Initial lstat check: reject symlinks immediately
    struct stat lst;
    if (lstat(lockPath.c_str(), &lst) != 0) {
        status.error = "Lockfile does not exist or cannot be accessed";
        return status;
    }
    if (S_ISLNK(lst.st_mode)) {
        status.error = "Lockfile is a symbolic link (rejected)";
        return status;
    }
    if (!S_ISREG(lst.st_mode)) {
        status.error = "Lockfile is not a regular file";
        return status;
    }

    // 2. Open with O_NOFOLLOW to atomically prevent symlink race conditions
    int fd = open(lockPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        status.error = "Could not open lockfile";
        return status;
    }

    // 3. fstat on the opened file descriptor to verify properties securely
    struct stat fst;
    if (fstat(fd, &fst) != 0) {
        close(fd);
        status.error = "fstat failed on lockfile descriptor";
        return status;
    }

    if (!S_ISREG(fst.st_mode)) {
        close(fd);
        status.error = "Lockfile descriptor is not a regular file";
        return status;
    }

    // 4. Mode 0600 check
    if ((fst.st_mode & 0777) != 0600) {
        close(fd);
        status.error = "Lockfile permissions are not 0600";
        return status;
    }

    // 5. UID ownership check against current effective UID
    if (fst.st_uid != geteuid()) {
        close(fd);
        status.error = "Lockfile is not owned by the current user";
        return status;
    }

    // 6. Read and parse PID
    char buf[64] = {0};
    ssize_t bytesRead = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytesRead <= 0) {
        status.error = "Lockfile is empty";
        return status;
    }

    char* endPtr = nullptr;
    long parsed = std::strtol(buf, &endPtr, 10);
    if (parsed <= 1 || parsed > 4194304) {
        status.error = "Invalid PID format or value in lockfile";
        return status;
    }

    status.pid = static_cast<pid_t>(parsed);
    return status;
}

bool DaemonClient::isDaemonRunning() const {
    DaemonStatus st = readDaemonPid();
    if (st.pid <= 0) return false;
    return (kill(st.pid, 0) == 0);
}

DaemonStatus DaemonClient::getStatus() const {
    DaemonStatus st = readDaemonPid();
    if (st.pid > 0) {
        st.isRunning = (kill(st.pid, 0) == 0);
    } else {
        st.isRunning = false;
    }
    return st;
}

bool DaemonClient::isCooldownActive() const {
    if (!m_hasTriggered) return false;
    auto now = std::chrono::steady_clock::now();
    return (now - m_lastTriggerTime) < m_cooldownDuration;
}

double DaemonClient::getCooldownRemainingSeconds() const {
    if (!m_hasTriggered) return 0.0;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - m_lastTriggerTime;
    if (elapsed >= m_cooldownDuration) return 0.0;
    auto rem = m_cooldownDuration - elapsed;
    return std::chrono::duration<double>(rem).count();
}

void DaemonClient::resetCooldown() {
    m_hasTriggered = false;
    m_lastTriggerTime = {};
}

void DaemonClient::setCooldownDuration(std::chrono::milliseconds duration) {
    m_cooldownDuration = duration;
}

void DaemonClient::setSystemctlRunner(SystemctlRunner runner) {
    m_systemctlRunner = std::move(runner);
}

bool DaemonClient::restartViaSystemd() {
    if (m_systemctlRunner) {
        return m_systemctlRunner({"--user", "restart", "fizzd"}) == 0;
    }

    const char* systemctlPath = "/usr/bin/systemctl";
    if (access(systemctlPath, X_OK) != 0) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        // Child process: silence output in background
        int devNull = open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            dup2(devNull, STDOUT_FILENO);
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }
        char* const argv[] = {
            const_cast<char*>("/usr/bin/systemctl"),
            const_cast<char*>("--user"),
            const_cast<char*>("restart"),
            const_cast<char*>("fizzd"),
            nullptr
        };
        execv(systemctlPath, argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return false;
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

DaemonTriggerStatus DaemonClient::triggerRebuild() {
    // 1. Check rate-limiting cooldown (< 5 seconds since last trigger)
    if (isCooldownActive()) {
        return DaemonTriggerStatus::CooldownIgnored;
    }

    // 2. Read PID from lockfile and verify process alive
    DaemonStatus st = readDaemonPid();
    if (st.pid > 0 && kill(st.pid, 0) == 0) {
        if (kill(st.pid, SIGUSR1) == 0) {
            m_lastTriggerTime = std::chrono::steady_clock::now();
            m_hasTriggered = true;
            return DaemonTriggerStatus::SuccessSignal;
        }
    }

    // 3. Daemon not running or signal failed: fallback to systemctl restart
    if (restartViaSystemd()) {
        m_lastTriggerTime = std::chrono::steady_clock::now();
        m_hasTriggered = true;
        return DaemonTriggerStatus::SuccessSystemd;
    }

    return DaemonTriggerStatus::DaemonNotRunning;
}

} // namespace fizz
