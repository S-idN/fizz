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
#include "FileSearch.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto kInterval = 1h;   
constexpr auto kQuiet    = 2s;   
constexpr auto kMaxDefer = 10s;  

static volatile sig_atomic_t gStop = 0;

static int inoFd;                                
static std::unordered_map<int, std::string> dirOf;   

static void rebuild() {
    systemFileSearch("/");
}

static bool ignored(const char* name) {
    static const char* skip[] = {".cache", ".git", "node_modules", "Flatpak", "Trash"};
    for (auto s : skip)
        if (strcmp(name, s) == 0) return true;
    return strncmp(name, ".fizzLocalCache", 15) == 0;
}

static void watch(const std::string& dir) {
    int wd = inotify_add_watch(inoFd, dir.c_str(),
                               IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd >= 0) dirOf[wd] = dir;
}

static void watchTree(const std::string& root) {
    watch(root);
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec) || it->is_symlink(ec)) continue;
        if (ignored(it->path().filename().c_str())) {
            it.disable_recursion_pending();
            continue;
        }
        watch(it->path().string());
    }
}

static bool readEvents() {
    alignas(inotify_event) char buf[16 * 1024];
    bool changed = false;
    ssize_t n;
    while ((n = read(inoFd, buf, sizeof buf)) > 0) {
        for (char* p = buf; p < buf + n; ) {
            auto* ev = reinterpret_cast<inotify_event*>(p);
            p += sizeof(inotify_event) + ev->len;

            if (ev->mask & IN_IGNORED) { dirOf.erase(ev->wd); continue; }   
            if (ev->len && ignored(ev->name)) continue;

            changed = true;
            if ((ev->mask & IN_ISDIR) && (ev->mask & (IN_CREATE | IN_MOVED_TO)) && dirOf.count(ev->wd))
                watchTree(dirOf[ev->wd] + "/" + ev->name);
        }
    }
    return changed;
}

static int acquireLock() {
    const char* rt = std::getenv("XDG_RUNTIME_DIR");
    std::string path = std::string(rt && *rt ? rt : "/tmp") + "/fizzd.lock";
    int fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) < 0) return -1;
    return fd;
}

static void runDaemon() {
    const char* home = std::getenv("HOME");
    if (!home) return;

    inoFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    watchTree(home);

    rebuild();
    auto lastBuild = Clock::now();
    bool dirty = false;
    Clock::time_point firstDirty, lastEvent;

    pollfd pfd{inoFd, POLLIN, 0};
    while (!gStop) {
        poll(&pfd, 1, 1000);  

        if ((pfd.revents & POLLIN) && readEvents()) {
            lastEvent = Clock::now();
            if (!dirty) { dirty = true; firstDirty = lastEvent; }
        }

        auto now = Clock::now();
        bool quiet   = dirty && now >= lastEvent + kQuiet;
        bool tooLong = dirty && now >= firstDirty + kMaxDefer;
        bool hourly  = now >= lastBuild + kInterval;

        if (quiet || tooLong || hourly) {
            rebuild();
            lastBuild = Clock::now();
            dirty = false;
        }
    }
    close(inoFd);
}

void daemonRunner() {
    int lock = acquireLock();
    if (lock < 0) return;   
    runDaemon();
    close(lock);
}

#ifdef FIZZD_MAIN
static void onSignal(int) { gStop = 1; }

int main() {
    signal(SIGTERM, onSignal);
    signal(SIGINT,  onSignal);
    daemonRunner();
    return 0;
}
#endif
