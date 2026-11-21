#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <vector>
#include <sys/types.h>

namespace fizz {

// Result status for daemon rebuild trigger attempts
enum class DaemonTriggerStatus {
    SuccessSignal,      // Dispatched SIGUSR1 to active daemon process
    SuccessSystemd,     // Successfully restarted daemon via /usr/bin/systemctl fallback
    CooldownIgnored,    // Rate-limited: trigger requested within 5s cooldown window
    DaemonNotRunning,   // Daemon process not alive and systemctl fallback failed or unavailable
    Failed              // General failure
};

// Process status reported by DaemonClient
struct DaemonStatus {
    pid_t pid = -1;
    bool isRunning = false;
    std::string error;
};

// Client for interacting with the background fizzd Index Daemon:
// - Reads daemon PID securely from $XDG_RUNTIME_DIR/fizzd.lock (0600, regular file, UID owner).
// - Checks process status via kill(pid, 0).
// - Dispatches SIGUSR1 for immediate background index rebuild.
// - Rate-limits rebuild triggers to a 5-second cooldown window.
// - Falls back to /usr/bin/systemctl --user restart fizzd if daemon is not running.
class DaemonClient {
public:
    using SystemctlRunner = std::function<int(const std::vector<std::string>&)>;

    DaemonClient();
    explicit DaemonClient(const std::string& customLockPath);
    ~DaemonClient() = default;

    // Resolves lockfile path: custom path if set, otherwise $XDG_RUNTIME_DIR/fizzd.lock,
    // with fallback to getUserRuntimeDir() + "/fizzd.lock".
    std::string resolveLockPath() const;

    // Sets a custom lockfile path (used for testing or explicit overrides)
    void setCustomLockPath(const std::string& path);

    // Reads daemon PID from the lockfile:
    // Enforces:
    // 1. Regular file check (not a symlink, via O_NOFOLLOW / lstat / S_ISREG)
    // 2. Mode 0600 (st.st_mode & 0777 == 0600)
    // 3. UID ownership matches current process (st.st_uid == geteuid())
    // 4. Parses valid PID (> 1, <= 4194304)
    DaemonStatus readDaemonPid() const;

    // Checks whether daemon process is currently running (valid PID and kill(pid, 0) == 0)
    bool isDaemonRunning() const;

    // Returns full daemon status (PID and running state)
    DaemonStatus getStatus() const;

    // Rate-limiting checks:
    // Returns true if cooldown is currently active (< 5 seconds since last trigger).
    bool isCooldownActive() const;

    // Returns seconds remaining in cooldown, or 0.0 if cooldown has expired.
    double getCooldownRemainingSeconds() const;

    // Resets the cooldown timer (useful for testing)
    void resetCooldown();

    // Sets custom cooldown duration (for testing)
    void setCooldownDuration(std::chrono::milliseconds duration);

    // Injects custom systemctl command runner for testing fallback execution
    void setSystemctlRunner(SystemctlRunner runner);

    // Dispatches rebuild trigger:
    // 1. If cooldown is active (< 5 seconds), returns CooldownIgnored.
    // 2. If daemon PID is active, sends SIGUSR1 and returns SuccessSignal.
    // 3. If daemon is not running, falls back to /usr/bin/systemctl --user restart fizzd.
    //    Returns SuccessSystemd if systemctl succeeds, or DaemonNotRunning / Failed.
    DaemonTriggerStatus triggerRebuild();

    // Directly invokes fallback: /usr/bin/systemctl --user restart fizzd
    bool restartViaSystemd();

private:
    std::string m_customLockPath;
    std::chrono::steady_clock::time_point m_lastTriggerTime{};
    bool m_hasTriggered = false;
    std::chrono::milliseconds m_cooldownDuration{5000};
    SystemctlRunner m_systemctlRunner;
};

} // namespace fizz
