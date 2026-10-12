#include <iostream>
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <string>
#include <cstdlib>
#include "FileSearch.h"
#include "InvertedSearchIndex.h"

#define BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

void runDebouncedDaemon() {
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        std::cerr << "[DAEMON ERROR] Failed to initialize inotify\n";
        return;
    }

    const char* homeDir = std::getenv("HOME");
    std::string homePath = (homeDir != nullptr) ? homeDir : "";

    if (homePath.empty()) {
        std::cerr << "[DAEMON ERROR] Could not determine user HOME directory\n";
        close(fd);
        return;
    }

    std::vector<std::string> watchPaths = {
        homePath + "/Downloads",
        homePath + "/Documents",
        homePath + "/Wallpapers"
    };

    for (const auto& path : watchPaths) {
        if (inotify_add_watch(fd, path.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM) < 0) {
            std::cerr << "[DAEMON WARNING] Could not watch path: " << path << "\n";
        }
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    char buffer[BUF_LEN];
    std::cout << "[DAEMON] Event-driven indexer running. Monitoring for changes...\n";

    while (true) {
        int pollRet = poll(&pfd, 1, -1); 
        if (pollRet <= 0) continue;

        std::cout << "[DAEMON] Change detected. Debouncing bulk operations...\n";

        while (true) {
            int nestedPoll = poll(&pfd, 1, 2000); 
            if (nestedPoll > 0) {
                int length = read(fd, buffer, BUF_LEN);
                (void)length; 
            } else {
                break;
            }
        }

        std::cout << "[DAEMON] Activity settled. Rebuilding binary cache cache...\n";
        
        systemFileSearch(homePath.c_str()); 
        createInvertedSearchIndex();
        
        std::cout << "[DAEMON] Cache updated successfully. Going back to sleep.\n";
    }

    close(fd);
}

int main() {
    const char* homeDir = std::getenv("HOME");
    std::string homePath = (homeDir != nullptr) ? homeDir : "/";

    std::cout << "[DAEMON] Performing startup cache initialization...\n";
    systemFileSearch(homePath.c_str());
    createInvertedSearchIndex();

    runDebouncedDaemon();
    return 0;
}

