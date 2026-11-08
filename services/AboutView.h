#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <sys/types.h>
#include "Config.h"
#include "ViewMode.h"

namespace fizz::ui {

// Application version and metadata constants
constexpr std::string_view kAppVersion = "v1.1.0";
constexpr std::string_view kAppLicense = "MIT License";
constexpr std::string_view kRepoUrl    = "https://github.com/sahil-shefeek/fizz";

constexpr std::string_view getAppVersion() { return kAppVersion; }
constexpr std::string_view getAppLicense() { return kAppLicense; }
constexpr std::string_view getRepoUrl() { return kRepoUrl; }

// Keyboard navigation shortcut documentation entry
struct ShortcutEntry {
    std::string key;
    std::string action;
    std::string category;
};

// Returns standard shortcut cheat sheet documenting essential shortcuts for:
// - launching files
// - terminal execution
// - untrusted desktop execution
// - tab navigation
const std::vector<ShortcutEntry>& getDefaultShortcutCheatSheet();

// Formats shortcut cheat sheet entries into an aligned text table (for testing, CLI, or export)
std::string formatShortcutTable(const std::vector<ShortcutEntry>& shortcuts);

// Binary Cache live diagnostics
struct CacheDiagnostics {
    std::string path;
    bool exists = false;
    size_t totalEntries = 0;
    uint64_t fileSizeBytes = 0;
    std::string formattedFileSize = "0 B";
    std::string lastRebuildTime = "Not found";
    time_t lastRebuildTimestamp = 0;
};

// Index Daemon process health diagnostics
struct DaemonDiagnostics {
    bool isRunning = false;
    pid_t pid = 0;
    bool watchLimitReached = false;
    std::string statusIndicator = "● fizzd: stopped";
};

// Formats the Index Daemon status indicator text
// e.g.:
// - "● fizzd: running (1234)"
// - "● fizzd: stopped"
// - "⚠️ fizzd: running (watch limit reached)"
std::string formatDaemonStatus(bool isRunning, pid_t pid, bool watchLimitReached);

// Formats file size in human-readable bytes (e.g. "1.2 MB (1258291 bytes)" or "0 B")
std::string formatFileSize(uint64_t bytes);

// Formats timestamp into YYYY-MM-DD HH:MM:SS
std::string formatTimestamp(time_t t);

// Collects live Binary Cache diagnostics from on-disk cache file
CacheDiagnostics collectCacheDiagnostics(const std::string& customCachePath = "");

// Collects live Index Daemon diagnostics from lockfile and process state
DaemonDiagnostics collectDaemonDiagnostics(const std::string& customRuntimeDir = "");

// Manages rate-limited diagnostics polling (throttled to at most once per second)
class AboutViewDiagnostics {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr auto kPollInterval = std::chrono::seconds(1);

    AboutViewDiagnostics() = default;

    // Checks if at least kPollInterval has elapsed since last update. If so (or if forced),
    // collects new diagnostics. Returns true if refreshed, false if throttled.
    bool refresh(TimePoint now = Clock::now(), bool force = false);

    const CacheDiagnostics& getCacheDiagnostics() const { return m_cacheDiag; }
    const DaemonDiagnostics& getDaemonDiagnostics() const { return m_daemonDiag; }

    TimePoint getLastPollTime() const { return m_lastPollTime; }
    size_t getPollCount() const { return m_pollCount; }
    bool hasPolled() const { return m_hasPolled; }

    void reset();

    void setCustomCachePath(const std::string& path) { m_customCachePath = path; }
    void setCustomRuntimeDir(const std::string& dir) { m_customRuntimeDir = dir; }

private:
    TimePoint m_lastPollTime{};
    bool m_hasPolled = false;
    size_t m_pollCount = 0;
    std::string m_customCachePath;
    std::string m_customRuntimeDir;

    CacheDiagnostics m_cacheDiag{};
    DaemonDiagnostics m_daemonDiag{};
};

// Renders the dedicated About View Mode within the Search Frontend.
// Displays the application version (v1.1.0), license, configuration file path, and repository URL.
// Embeds live diagnostic indicators showing Binary Cache file status and background Index Daemon health.
// Provides a comprehensive keyboard navigation cheat sheet.
void renderAboutView(const FizzConfig& config, ViewModeStateMachine& stateMachine, AboutViewDiagnostics* diagnostics = nullptr);

} // namespace fizz::ui
