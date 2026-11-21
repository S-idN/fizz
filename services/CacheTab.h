#pragma once

#include <string>
#include <cstdint>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#include "Config.h"
#include "DaemonClient.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace fizz {

// Diagnostics information retrieved from the on-disk binary cache file
struct CacheDiagnostics {
    bool fileExists = false;
    uint64_t fileSizeBytes = 0;
    size_t entryCount = 0;
    time_t lastModified = 0;
    std::string formattedSize;       // Formatted as KB or MB (e.g. "512.0 KB", "2.35 MB")
    std::string formattedTimestamp;  // Formatted as "YYYY-MM-DD HH:MM:SS" or "Never"
};

// Formats file size in bytes to a human-readable string in KB or MB
std::string formatFileSize(uint64_t bytes);

// Formats timestamp (seconds since epoch) into "YYYY-MM-DD HH:MM:SS" (or "Never" if <= 0)
std::string formatTimestamp(time_t timeSec);

// Reads live cache diagnostics from the specified cache file.
// If cachePath is empty, queries the active binary cache path via getBinaryCachePath().
CacheDiagnostics getCacheDiagnostics(const std::string& cachePath = "");

// Validates a candidate custom binary cache path:
// 1. Checks that the path is non-empty.
// 2. Checks that the path is not a symlink (via lstat).
// 3. Checks that the path is not a directory.
// 4. Checks that the parent directory exists and is a directory.
// 5. Checks that the parent directory is writable.
// 6. Rejects paths in world-writable directories not owned by current user (currentUid, defaults to geteuid()).
// Returns true if valid, false otherwise (with explanation written to errorMessage if non-null).
bool validateCachePath(const std::string& path, std::string* errorMessage = nullptr, uid_t currentUid = geteuid());

// Validates and applies a custom binary cache path:
// - Updates config.binary_cache_path.
// - Updates runtime binary cache path via setBinaryCachePath().
// - Atomically persists the configuration to fizz.conf via saveConfig().
// Returns true on success, false on validation or save failure.
bool applyCustomCachePath(const std::string& newPath, FizzConfig& config,
                          std::string* errorMessage = nullptr,
                          const std::string& configFilePath = "");

// Resets binary cache path in config and runtime state to default (empty string),
// and atomically persists the configuration to fizz.conf via saveConfig().
bool resetToDefaultCachePath(FizzConfig& config,
                            std::string* errorMessage = nullptr,
                            const std::string& configFilePath = "");

// Cache Tab UI component encapsulating state and ImGui rendering
class CacheTab {
public:
    CacheTab();
    explicit CacheTab(const FizzConfig& config);
    ~CacheTab();

    CacheTab(const CacheTab&) = delete;
    CacheTab& operator=(const CacheTab&) = delete;
    CacheTab(CacheTab&&) noexcept;
    CacheTab& operator=(CacheTab&&) noexcept;

    // Synchronizes internal input buffer with config if path has changed externally
    void syncWithConfig(const FizzConfig& config);

    // Renders the Cache tab UI inside the active ImGui container
    void render(FizzConfig& config);

    // Triggers an asynchronous rebuild request via DaemonClient
    void triggerRebuild(const FizzConfig& config);

    // Programmatically query whether rebuild / spinner is in flight
    bool isRebuilding() const;
    bool isSpinnerActive() const;

    // Programmatically set rebuilding state (for testing UI rendering)
    void setRebuilding(bool rebuilding);

    // Waits for background rebuild thread to complete (up to timeoutMs)
    void waitForRebuildCompletion(int timeoutMs = 5000);

    // Accessors for DaemonClient
    DaemonClient& getDaemonClient() { return m_daemonClient; }
    const DaemonClient& getDaemonClient() const { return m_daemonClient; }

    // Programmatically set input text buffer (for testing and automation)
    void setInputPath(const std::string& path);
    const std::string& getInputPath() const { return m_inputPath; }

    std::string getStatusMessage() const;
    bool isStatusError() const;

private:
    char m_inputBuffer[1024];
    std::string m_inputPath;
    std::string m_lastSyncedConfigPath;
    mutable std::mutex m_statusMutex;
    std::string m_statusMessage;
    bool m_statusIsError = false;
    bool m_initialized = false;

    DaemonClient m_daemonClient;
    std::atomic<bool> m_rebuilding{false};
    std::atomic<bool> m_stopRebuildWatcher{false};
    std::thread m_rebuildThread;
};

} // namespace fizz
