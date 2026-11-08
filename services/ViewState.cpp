#include "ViewState.h"
#include "Config.h"
#include "ViewMode.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <cctype>

namespace fs = std::filesystem;

std::string getViewStateFilePath() {
    std::string rt = getUserRuntimeDir();
    if (rt.empty()) {
        return "";
    }
    return rt + "/view_state";
}

bool clearViewState(const std::string& customPath) {
    std::string path = !customPath.empty() ? customPath : getViewStateFilePath();
    if (path.empty()) {
        return true;
    }
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        return true;
    }
    if (unlink(path.c_str()) != 0) {
        return errno == ENOENT;
    }
    return true;
}

bool saveViewState(ViewMode mode, const std::string& customPath, int64_t customTimestamp) {
    if (mode == ViewMode::Search) {
        return clearViewState(customPath);
    }

    std::string path = !customPath.empty() ? customPath : getViewStateFilePath();
    if (path.empty()) {
        return false;
    }

    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        return false;
    }

    if (fchmod(fd, 0600) != 0) {
        close(fd);
        return false;
    }

    int64_t now = (customTimestamp > 0) ? customTimestamp : static_cast<int64_t>(std::time(nullptr));
    std::string content = "mode=" + std::string(viewModeToString(mode)) + "\n" +
                          "timestamp=" + std::to_string(now) + "\n";

    const char* buf = content.data();
    size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t written = write(fd, buf, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return false;
        }
        buf += written;
        remaining -= written;
    }

    close(fd);
    return true;
}

ViewMode loadViewState(int graceSeconds, const std::string& customPath, int64_t currentTimestamp, uid_t expectedUid) {
    std::string path = !customPath.empty() ? customPath : getViewStateFilePath();
    if (path.empty()) {
        return ViewMode::Search;
    }

    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        return ViewMode::Search;
    }

    // Validate that it is not a symlink, is a regular file, and is owned by expected UID
    if (S_ISLNK(st.st_mode)) {
        unlink(path.c_str());
        return ViewMode::Search;
    }

    if (!S_ISREG(st.st_mode)) {
        return ViewMode::Search;
    }

    if (st.st_uid != expectedUid) {
        return ViewMode::Search;
    }

    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return ViewMode::Search;
    }

    struct stat fst;
    if (fstat(fd, &fst) != 0 || !S_ISREG(fst.st_mode) || fst.st_uid != expectedUid) {
        close(fd);
        if (fst.st_uid == expectedUid) {
            clearViewState(path);
        }
        return ViewMode::Search;
    }

    if (fst.st_size == 0 || fst.st_size > 4096) {
        close(fd);
        clearViewState(path);
        return ViewMode::Search;
    }

    char buffer[4096];
    ssize_t bytesRead = 0;
    while (bytesRead < fst.st_size) {
        ssize_t r = read(fd, buffer + bytesRead, fst.st_size - bytesRead);
        if (r < 0) {
            if (errno == EINTR) continue;
            close(fd);
            clearViewState(path);
            return ViewMode::Search;
        }
        if (r == 0) break;
        bytesRead += r;
    }
    close(fd);

    if (bytesRead <= 0) {
        clearViewState(path);
        return ViewMode::Search;
    }

    std::string content(buffer, bytesRead);
    std::istringstream stream(content);
    std::string line;
    std::string modeStr;
    int64_t savedTimestamp = -1;
    bool hasMode = false;
    bool hasTimestamp = false;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t eqPos = trimmed.find('=');
        if (eqPos != std::string::npos) {
            std::string key = trimmed.substr(0, eqPos);
            std::string val = trimmed.substr(eqPos + 1);
            size_t kStart = key.find_first_not_of(" \t");
            size_t kEnd = key.find_last_not_of(" \t");
            if (kStart != std::string::npos) key = key.substr(kStart, kEnd - kStart + 1);
            size_t vStart = val.find_first_not_of(" \t");
            size_t vEnd = val.find_last_not_of(" \t");
            if (vStart != std::string::npos) val = val.substr(vStart, vEnd - vStart + 1);

            for (auto& c : key) c = std::tolower(static_cast<unsigned char>(c));

            if (key == "mode" || key == "view_mode") {
                modeStr = val;
                hasMode = true;
            } else if (key == "timestamp" || key == "time") {
                try {
                    size_t idx = 0;
                    savedTimestamp = std::stoll(val, &idx);
                    if (idx == val.size()) {
                        hasTimestamp = true;
                    }
                } catch (...) {
                    savedTimestamp = -1;
                }
            }
        }
    }

    if (!hasMode || !hasTimestamp || savedTimestamp < 0) {
        clearViewState(path);
        return ViewMode::Search;
    }

    ViewMode parsedMode = stringToViewMode(modeStr);
    if (parsedMode == ViewMode::Search) {
        clearViewState(path);
        return ViewMode::Search;
    }

    int64_t now = (currentTimestamp > 0) ? currentTimestamp : static_cast<int64_t>(std::time(nullptr));
    int64_t elapsed = now - savedTimestamp;

    if (elapsed < 0 || elapsed > graceSeconds) {
        clearViewState(path);
        return ViewMode::Search;
    }

    return parsedMode;
}
