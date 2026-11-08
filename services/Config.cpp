#include "Config.h"
#include "FileSearch.h"
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

static std::string s_CustomConfigPath = "";
static std::string s_CustomRuntimeDir = "";

void setCustomRuntimeDir(const std::string& path) {
    s_CustomRuntimeDir = path;
}

std::string getUserRuntimeDir() {
    if (!s_CustomRuntimeDir.empty()) {
        struct stat st;
        if (lstat(s_CustomRuntimeDir.c_str(), &st) != 0) {
            mkdir(s_CustomRuntimeDir.c_str(), 0700);
        }
        return s_CustomRuntimeDir;
    }

    uid_t uid = getuid();
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0') {
        std::error_code ec;
        if (fs::is_directory(xdg, ec)) {
            std::string fizzDir = std::string(xdg) + "/fizz";
            struct stat st;
            if (lstat(fizzDir.c_str(), &st) != 0) {
                if (mkdir(fizzDir.c_str(), 0700) != 0 && errno != EEXIST) {
                    std::cerr << "[ERROR] Could not create secure runtime directory: " << fizzDir
                              << " (" << strerror(errno) << ")\n";
                    return "";
                }
                chmod(fizzDir.c_str(), 0700);
            } else {
                if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode) || st.st_uid != uid) {
                    std::cerr << "[ERROR] Insecure or invalid runtime directory: " << fizzDir << "\n";
                    return "";
                }
                if ((st.st_mode & 0777) != 0700) {
                    chmod(fizzDir.c_str(), 0700);
                }
            }
            return fizzDir;
        }
    }

    std::string fallback = "/tmp/fizz-" + std::to_string(uid);

    struct stat st;
    if (lstat(fallback.c_str(), &st) != 0) {
        if (mkdir(fallback.c_str(), 0700) != 0 && errno != EEXIST) {
            std::cerr << "[ERROR] Could not create secure runtime directory: " << fallback
                      << " (" << strerror(errno) << ")\n";
            return "";
        }
        chmod(fallback.c_str(), 0700);
    } else {
        // Exists: ensure it's not a symlink, is a directory, and owned by this uid
        if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode) || st.st_uid != uid) {
            std::cerr << "[ERROR] Insecure or invalid runtime directory: " << fallback << "\n";
            return "";
        }
        if ((st.st_mode & 0777) != 0700) {
            chmod(fallback.c_str(), 0700);
        }
    }
    return fallback;
}

std::string getHomeDir() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return home;
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
        return pw->pw_dir;
    }
    return "";
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string stripQuotes(const std::string& str) {
    if (str.size() >= 2) {
        if ((str.front() == '"' && str.back() == '"') ||
            (str.front() == '\'' && str.back() == '\'')) {
            return str.substr(1, str.size() - 2);
        }
    }
    return str;
}

static std::string expandTilde(const std::string& path) {
    if (path == "~") {
        return getHomeDir();
    }
    if (path.rfind("~/", 0) == 0) {
        return getHomeDir() + path.substr(1);
    }
    return path;
}

static std::string stripTrailingSlashes(std::string path) {
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

static std::string normalizePath(std::string path) {
    path = expandTilde(path);
    return stripTrailingSlashes(std::move(path));
}

static std::vector<std::string> parseCommaSeparatedList(const std::string& val, bool isPathList) {
    std::vector<std::string> items;
    std::stringstream ss(val);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        item = stripQuotes(item);
        if (item.empty()) continue;

        if (isPathList || item == "~" || item.rfind("~/", 0) == 0) {
            item = normalizePath(item);
        } else {
            item = stripTrailingSlashes(item);
        }
        if (!item.empty()) {
            items.push_back(item);
        }
    }
    return items;
}

std::string getConfigPath() {
    if (!s_CustomConfigPath.empty()) {
        return s_CustomConfigPath;
    }
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && xdgConfigHome[0] != '\0') {
        return std::string(xdgConfigHome) + "/fizz/fizz.conf";
    }
    std::string home = getHomeDir();
    if (!home.empty()) {
        return home + "/.config/fizz/fizz.conf";
    }
    return "/tmp/fizz.conf";
}

void setConfigPath(const std::string& path) {
    s_CustomConfigPath = path;
}

FizzConfig FizzConfig::defaults() {
    FizzConfig cfg;
    cfg.searchRoots = getDefaultSearchRoots(true);
    return cfg;
}

FizzConfig loadConfig(const std::string& configPath) {
    std::string path = configPath.empty() ? getConfigPath() : configPath;

    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
        return FizzConfig::defaults();
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return FizzConfig::defaults();
    }

    FizzConfig config = FizzConfig::defaults();
    bool hasSearchRoots = false;
    bool unreadable = false;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        size_t eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) {
            unreadable = true;
            break;
        }

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string val = trim(trimmed.substr(eqPos + 1));

        if (key.empty()) {
            unreadable = true;
            break;
        }

        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lowerKey == "search_roots") {
            config.searchRoots = parseCommaSeparatedList(val, true);
            hasSearchRoots = true;
        } else if (lowerKey == "ignore_rules") {
            config.ignoreRules = parseCommaSeparatedList(val, false);
        } else if (lowerKey == "auto_close_on_unfocus") {
            std::string lowerVal = stripQuotes(val);
            std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lowerVal == "true" || lowerVal == "1") {
                config.autoCloseOnUnfocus = true;
            } else if (lowerVal == "false" || lowerVal == "0") {
                config.autoCloseOnUnfocus = false;
            } else {
                config.autoCloseOnUnfocus = true;
            }
        } else if (lowerKey == "config_version") {
            std::string vStr = stripQuotes(val);
            try {
                size_t idx = 0;
                int v = std::stoi(vStr, &idx);
                if (idx == vStr.size() && v >= 1) {
                    config.config_version = v;
                } else {
                    config.config_version = 1;
                }
            } catch (...) {
                config.config_version = 1;
            }
        } else if (lowerKey == "theme") {
            std::string t = stripQuotes(val);
            std::transform(t.begin(), t.end(), t.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (t == "dark" || t == "light") {
                config.theme = t;
            } else {
                config.theme = "dark";
            }
        } else if (lowerKey == "accent") {
            std::string a = stripQuotes(val);
            std::transform(a.begin(), a.end(), a.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (a == "blue" || a == "green" || a == "emerald" || a == "purple" ||
                a == "amber" || a == "orange" || a == "red" || a == "crimson" ||
                a == "teal" || a == "pink") {
                config.accent = a;
            } else {
                config.accent = "blue";
            }
        } else if (lowerKey == "window_width") {
            std::string wStr = stripQuotes(val);
            try {
                size_t idx = 0;
                int w = std::stoi(wStr, &idx);
                if (idx == wStr.size()) {
                    config.window_width = std::clamp(w, 500, 1600);
                } else {
                    config.window_width = 750;
                }
            } catch (...) {
                config.window_width = 750;
            }
        } else if (lowerKey == "window_height") {
            std::string hStr = stripQuotes(val);
            try {
                size_t idx = 0;
                int h = std::stoi(hStr, &idx);
                if (idx == hStr.size()) {
                    config.window_height = std::clamp(h, 320, 1200);
                } else {
                    config.window_height = 480;
                }
            } catch (...) {
                config.window_height = 480;
            }
        } else if (lowerKey == "font_path") {
            std::string fp = stripQuotes(val);
            if (!fp.empty()) {
                config.font_path = normalizePath(fp);
            } else {
                config.font_path = "";
            }
        } else if (lowerKey == "font_size") {
            std::string fsStr = stripQuotes(val);
            try {
                size_t idx = 0;
                float sz = std::stof(fsStr, &idx);
                if (idx == fsStr.size() && sz > 0.0f) {
                    config.font_size = sz;
                } else {
                    config.font_size = 16.0f;
                }
            } catch (...) {
                config.font_size = 16.0f;
            }
        } else if (lowerKey == "binary_cache_path") {
            std::string cp = stripQuotes(val);
            if (!cp.empty()) {
                config.binary_cache_path = normalizePath(cp);
            } else {
                config.binary_cache_path = "";
            }
        } else if (lowerKey == "view_state_grace_seconds") {
            std::string gsStr = stripQuotes(val);
            try {
                size_t idx = 0;
                int gs = std::stoi(gsStr, &idx);
                if (idx == gsStr.size()) {
                    config.view_state_grace_seconds = std::clamp(gs, 1, 15);
                } else {
                    config.view_state_grace_seconds = 5;
                }
            } catch (...) {
                config.view_state_grace_seconds = 5;
            }
        } else {
            // Permissive: unrecognized keys are retained for round-trip preservation
            config.unrecognizedKeys.push_back({key, stripQuotes(val)});
        }
    }

    if (unreadable) {
        return FizzConfig::defaults();
    }

    if (!hasSearchRoots || config.searchRoots.empty()) {
        config.searchRoots = getDefaultSearchRoots(true);
    }

    return config;
}

static std::atomic<uint64_t> s_ConfigTmpCounter{0};

static bool writeAllBytes(int fd, const char* data, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t written = write(fd, data + total, size - total);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(written);
    }
    return true;
}

static bool checkFileHasConfigVersion(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;
        size_t eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = trim(trimmed.substr(0, eqPos));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        if (key == "config_version") {
            return true;
        }
    }
    return false;
}

static bool backupPreV1Config(const std::string& srcPath, const std::string& backupPath, mode_t mode) {
    struct stat bst;
    if (lstat(backupPath.c_str(), &bst) == 0) {
        if (S_ISLNK(bst.st_mode) || !S_ISREG(bst.st_mode) || bst.st_uid != geteuid()) {
            return false;
        }
        unlink(backupPath.c_str());
    }

    std::ifstream src(srcPath, std::ios::binary);
    if (!src.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
    src.close();

    std::string tmpBackup;
    int bfd = -1;
    for (int attempt = 0; attempt < 100; ++attempt) {
        tmpBackup = backupPath + ".tmp." + std::to_string(getpid()) + "." + std::to_string(++s_ConfigTmpCounter);
        bfd = open(tmpBackup.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, mode);
        if (bfd >= 0) break;
        if (errno != EEXIST) {
            return false;
        }
    }
    if (bfd < 0) return false;

    fchmod(bfd, mode);

    if (!writeAllBytes(bfd, content.data(), content.size())) {
        close(bfd);
        unlink(tmpBackup.c_str());
        return false;
    }

    if (fsync(bfd) != 0) {
        close(bfd);
        unlink(tmpBackup.c_str());
        return false;
    }

    if (close(bfd) != 0) {
        unlink(tmpBackup.c_str());
        return false;
    }

    if (rename(tmpBackup.c_str(), backupPath.c_str()) != 0) {
        unlink(tmpBackup.c_str());
        return false;
    }

    return true;
}

bool saveConfig(const FizzConfig& config, const std::string& configPath) {
    std::string path = configPath.empty() ? getConfigPath() : configPath;
    path = expandTilde(path);
    if (path.empty()) {
        return false;
    }

    fs::path p(path);
    fs::path parent = p.parent_path();
    if (parent.empty()) {
        parent = ".";
    }

    std::error_code ec;
    if (!fs::exists(parent, ec)) {
        if (!fs::create_directories(parent, ec)) {
            return false;
        }
        chmod(parent.c_str(), 0700);
    }

    struct stat parent_st;
    if (stat(parent.c_str(), &parent_st) != 0 || !S_ISDIR(parent_st.st_mode)) {
        return false;
    }

    bool isWorldWritable = (parent_st.st_mode & S_IWOTH) != 0;

    struct stat dest_st;
    bool destExists = (lstat(path.c_str(), &dest_st) == 0);

    if (destExists) {
        if (S_ISLNK(dest_st.st_mode) || !S_ISREG(dest_st.st_mode) || dest_st.st_uid != geteuid()) {
            return false;
        }
    }

    if (isWorldWritable && !destExists) {
        return false;
    }

    mode_t mode = destExists ? (dest_st.st_mode & 0777) : 0600;

    // Migrate unversioned configuration if destination exists and lacks config_version
    if (destExists && !checkFileHasConfigVersion(path)) {
        std::string backupPath = path + ".pre-v1";
        if (!backupPreV1Config(path, backupPath, mode)) {
            return false;
        }
    }

    std::ostringstream ss;
    ss << "# Fizz Configuration File\n";
    ss << "config_version = " << std::max(1, config.config_version) << "\n\n";

    ss << "# Search paths and ignore patterns\n";
    ss << "search_roots = ";
    for (size_t i = 0; i < config.searchRoots.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << config.searchRoots[i];
    }
    ss << "\n";

    ss << "ignore_rules = ";
    for (size_t i = 0; i < config.ignoreRules.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << config.ignoreRules[i];
    }
    ss << "\n\n";

    ss << "# Window behavior\n";
    ss << "auto_close_on_unfocus = " << (config.autoCloseOnUnfocus ? "true" : "false") << "\n\n";

    ss << "# Visual appearance\n";
    ss << "theme = " << config.theme << "\n";
    ss << "accent = " << config.accent << "\n";
    ss << "window_width = " << config.window_width << "\n";
    ss << "window_height = " << config.window_height << "\n";
    ss << "font_path = " << config.font_path << "\n";
    ss << "font_size = " << (config.font_size > 0.0f ? config.font_size : 16.0f) << "\n\n";

    ss << "# Binary cache and view state\n";
    ss << "binary_cache_path = " << config.binary_cache_path << "\n";
    ss << "view_state_grace_seconds = " << config.view_state_grace_seconds << "\n";

    if (!config.unrecognizedKeys.empty()) {
        ss << "\n# Unrecognized keys preserved for forward compatibility\n";
        for (const auto& [k, v] : config.unrecognizedKeys) {
            ss << k << " = " << v << "\n";
        }
    }

    std::string serialized = ss.str();

    std::string tmpPath;
    int fd = -1;
    for (int attempt = 0; attempt < 100; ++attempt) {
        tmpPath = path + ".tmp." + std::to_string(getpid()) + "." + std::to_string(++s_ConfigTmpCounter);
        fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, mode);
        if (fd >= 0) break;
        if (errno != EEXIST) {
            return false;
        }
    }
    if (fd < 0) return false;

    fchmod(fd, mode);

    if (!writeAllBytes(fd, serialized.data(), serialized.size())) {
        close(fd);
        unlink(tmpPath.c_str());
        return false;
    }

    if (fsync(fd) != 0) {
        close(fd);
        unlink(tmpPath.c_str());
        return false;
    }

    if (close(fd) != 0) {
        unlink(tmpPath.c_str());
        return false;
    }

    if (rename(tmpPath.c_str(), path.c_str()) != 0) {
        unlink(tmpPath.c_str());
        return false;
    }

    return true;
}
