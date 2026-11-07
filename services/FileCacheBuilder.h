#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <sys/inotify.h>

// Forward declarations and helpers for inotify watching and event processing
int watchDir(int fd, const std::string& dir, std::unordered_map<int, std::string>& dirMap,
             int (*addWatchFn)(int, const char*, uint32_t) = ::inotify_add_watch);

void watchTree(int fd, const std::string& root, std::unordered_map<int, std::string>& dirMap,
               int (*addWatchFn)(int, const char*, uint32_t) = ::inotify_add_watch);

bool readEvents(int fd, std::unordered_map<int, std::string>& dirMap, bool* overflowDetected = nullptr);
bool readEvents();

void rebuildCache();
void stopDaemon();
void runIndexDaemon();
void daemonRunner(); // alias for runIndexDaemon()

// Acquires exclusive Index Daemon lock in the user runtime directory (or customRuntimeDir if provided).
// Uses O_NOFOLLOW to prevent symlink traversal attacks. Returns file descriptor >= 0 on success, or -1 on failure.
int acquireDaemonLock(const std::string& customRuntimeDir = "");

// Releases the Index Daemon lock descriptor if open.
void releaseDaemonLock(int& fd);

// State tracking for deferred rebuild scheduling
class RebuildScheduler {
public:
    using Duration = std::chrono::steady_clock::duration;
    using TimePoint = std::chrono::steady_clock::time_point;

    RebuildScheduler(Duration quiet = std::chrono::seconds(2),
                     Duration maxDefer = std::chrono::seconds(10),
                     Duration interval = std::chrono::hours(1))
        : m_quiet(quiet), m_maxDefer(maxDefer), m_interval(interval),
          m_lastBuild(std::chrono::steady_clock::now()), m_dirty(false) {}

    void onEvent(TimePoint now = std::chrono::steady_clock::now()) {
        m_lastEvent = now;
        if (!m_dirty) {
            m_dirty = true;
            m_firstDirty = now;
        }
    }

    bool shouldRebuild(TimePoint now = std::chrono::steady_clock::now()) const {
        if (!m_dirty) {
            return (now >= m_lastBuild + m_interval);
        }
        bool quiet = (now >= m_lastEvent + m_quiet);
        bool tooLong = (now >= m_firstDirty + m_maxDefer);
        return (quiet || tooLong);
    }

    void onRebuildExecuted(TimePoint now = std::chrono::steady_clock::now()) {
        m_lastBuild = now;
        m_dirty = false;
    }

    bool isDirty() const { return m_dirty; }

private:
    Duration m_quiet;
    Duration m_maxDefer;
    Duration m_interval;
    TimePoint m_lastBuild;
    TimePoint m_firstDirty{};
    TimePoint m_lastEvent{};
    bool m_dirty{false};
};
