#include <poll.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <iostream>
#include "FileCacheBuilder.h"
#include "FileSearch.h"
#include "Config.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto kInterval = 1h;   
constexpr auto kQuiet    = 2s;   
constexpr auto kMaxDefer = 10s;  

static volatile sig_atomic_t gStop = 0;

static int inoFd = -1;                                
static std::unordered_map<int, std::string> dirOf;   
static bool gOverflowOccurred = false;

static std::vector<std::string> resolveSearchRoots(const std::vector<std::string>& configuredRoots) {
    std::vector<std::string> existingRoots;
    for (const auto& root : configuredRoots) {
        std::error_code ec;
        if (fs::exists(root, ec) && fs::is_directory(root, ec)) {
            existingRoots.push_back(root);
        }
    }
    if (existingRoots.empty()) {
        existingRoots = getDefaultSearchRoots(true);
    }
    return existingRoots;
}

void rebuildCache() {
    if (gStop) {
        setSearchInterrupted(true);
        return;
    }
    setSearchInterrupted(false);
    auto config = loadConfig();
    setCustomIgnoreRules(config.ignoreRules);
    auto roots = resolveSearchRoots(config.searchRoots);
    systemFileSearch(roots);
}

int watchDir(int fd, const std::string& dir, std::unordered_map<int, std::string>& dirMap,
             int (*addWatchFn)(int, const char*, uint32_t)) {
    int wd = addWatchFn(fd, dir.c_str(),
                        IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd >= 0) {
        dirMap[wd] = dir;
    } else {
        if (errno == ENOSPC) {
            std::cerr << "[WARNING] inotify_add_watch failed for '" << dir << "': "
                      << "kernel watch limit reached (ENOSPC). "
                      << "Increase limit with 'sysctl fs.inotify.max_user_watches'.\n";
        }
    }
    return wd;
}

void watchTree(int fd, const std::string& root, std::unordered_map<int, std::string>& dirMap,
               int (*addWatchFn)(int, const char*, uint32_t)) {
    std::string rootName = fs::path(root).filename().string();
    if (isPathIgnored(root, rootName.empty() ? nullptr : rootName.c_str())) return;
    watchDir(fd, root, dirMap, addWatchFn);
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec) || it->is_symlink(ec)) continue;
        if (isPathIgnored(it->path().string(), it->path().filename().c_str())) {
            it.disable_recursion_pending();
            continue;
        }
        watchDir(fd, it->path().string(), dirMap, addWatchFn);
    }
}

static void watch(const std::string& dir) {
    watchDir(inoFd, dir, dirOf);
}

static void watchTree(const std::string& root) {
    watchTree(inoFd, root, dirOf);
}

bool readEvents(int fd, std::unordered_map<int, std::string>& dirMap, bool* overflowDetected) {
    alignas(inotify_event) char buf[16 * 1024];
    bool changed = false;
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        for (char* p = buf; p < buf + n; ) {
            auto* ev = reinterpret_cast<inotify_event*>(p);
            p += sizeof(inotify_event) + ev->len;

            if (ev->mask & IN_Q_OVERFLOW) {
                std::cerr << "[WARNING] inotify event queue overflowed (IN_Q_OVERFLOW); scheduling deferred full rebuild.\n";
                if (overflowDetected) {
                    *overflowDetected = true;
                }
                changed = true;
                continue;
            }

            if (ev->mask & IN_IGNORED) {
                dirMap.erase(ev->wd);
                continue;
            }

            if (ev->len) {
                std::string fullPath = dirMap.count(ev->wd) ? (dirMap[ev->wd] + "/" + ev->name) : "";
                if (isPathIgnored(fullPath, ev->name)) continue;
            }

            changed = true;
            if ((ev->mask & IN_ISDIR) && (ev->mask & (IN_CREATE | IN_MOVED_TO)) && dirMap.count(ev->wd)) {
                watchTree(fd, dirMap[ev->wd] + "/" + ev->name, dirMap);
            }
        }
    }
    return changed;
}

bool readEvents() {
    return readEvents(inoFd, dirOf, &gOverflowOccurred);
}

int acquireDaemonLock(const std::string& customRuntimeDir) {
    std::string rtDir = !customRuntimeDir.empty() ? customRuntimeDir : getUserRuntimeDir();
    if (rtDir.empty()) {
        return -1;
    }
    std::string path = rtDir + "/fizzd.lock";
    int fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void releaseDaemonLock(int& fd) {
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
        fd = -1;
    }
}

static void runDaemon() {
    auto config = loadConfig();
    setCustomIgnoreRules(config.ignoreRules);

    auto existingRoots = resolveSearchRoots(config.searchRoots);
    if (existingRoots.empty()) return;

    inoFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    for (const auto& root : existingRoots) {
        watchTree(root);
    }

    rebuildCache();
    RebuildScheduler scheduler(kQuiet, kMaxDefer, kInterval);
    scheduler.onRebuildExecuted();

    pollfd pfd{inoFd, POLLIN, 0};
    while (!gStop) {
        poll(&pfd, 1, 250);  

        if ((pfd.revents & POLLIN) && readEvents()) {
            scheduler.onEvent();
        }

        auto now = Clock::now();
        if (scheduler.shouldRebuild(now)) {
            if (gOverflowOccurred) {
                for (const auto& root : existingRoots) {
                    watchTree(root);
                }
                gOverflowOccurred = false;
            }
            rebuildCache();
            scheduler.onRebuildExecuted(Clock::now());
        }
    }
    close(inoFd);
}

void stopDaemon() {
    gStop = 1;
}

void runIndexDaemon() {
    int lock = acquireDaemonLock();
    if (lock < 0) return;   
    runDaemon();
    releaseDaemonLock(lock);
}

void daemonRunner() {
    runIndexDaemon();
}

#ifdef FIZZD_MAIN
static void onSignal(int) {
    gStop = 1;
    setSearchInterrupted(true);
}

int main() {
    signal(SIGTERM, onSignal);
    signal(SIGINT,  onSignal);
    daemonRunner();
    return 0;
}
#endif
