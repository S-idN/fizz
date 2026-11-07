#include <iostream>
#include <cstring>
#include <csignal>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#include <pwd.h>
#include <vector>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include "FileSearch.h"
#include "Config.h"
#include "AppLauncher.h"

namespace fs = std::filesystem;

struct ParsedIgnoreRule {
    std::string rule;
    std::string component;
    bool isPrefix;
};

// Ignore rules: skip system mount paths, common cache/vcs directories, and Binary Cache files
static std::vector<ParsedIgnoreRule> s_CustomIgnoreRules;

void setCustomIgnoreRules(const std::vector<std::string>& rules) {
    s_CustomIgnoreRules.clear();
    s_CustomIgnoreRules.reserve(rules.size());
    for (const auto& r : rules) {
        if (r.empty()) continue;
        bool isPrefix = (r[0] == '/');
        std::string comp = isPrefix ? "" : ("/" + r);
        s_CustomIgnoreRules.push_back({r, std::move(comp), isPrefix});
    }
}

static bool hasComponent(std::string_view fullpath, std::string_view comp) {
    size_t pos = fullpath.find(comp);
    while (pos != std::string_view::npos) {
        size_t end = pos + comp.size();
        if (end == fullpath.size() || fullpath[end] == '/') {
            return true;
        }
        pos = fullpath.find(comp, pos + 1);
    }
    return false;
}

bool isPathIgnored(std::string_view fullpath, const char* d_name) {
    if (d_name) {
        if (strcmp(d_name, ".cache") == 0 ||
            strcmp(d_name, ".git") == 0 ||
            strcmp(d_name, "node_modules") == 0 ||
            strcmp(d_name, "Trash") == 0 ||
            // Default sensitive/secret directory exclusions
            strcmp(d_name, ".ssh") == 0 ||
            strcmp(d_name, ".gnupg") == 0 ||
            strcmp(d_name, ".aws") == 0 ||
            strcmp(d_name, ".azure") == 0) {
            return true;
        }
        if (strncmp(d_name, ".fizzLocalCache", 15) == 0) {
            return true;
        }
    }

    static const std::string_view prefixIgnores[] = {"/dev", "/proc", "/sys"};
    for (auto prefix : prefixIgnores) {
        if (fullpath.size() >= prefix.size() && fullpath.substr(0, prefix.size()) == prefix) {
            return true;
        }
    }

    static const std::string_view componentIgnores[] = {
        "/.cache",
        "/.git",
        "/node_modules",
        "/Trash",
        "/.local/share/Trash",
        // Default sensitive/secret path component exclusions
        "/.ssh",
        "/.gnupg",
        "/.aws",
        "/.azure",
        "/.config/gcloud",
        "/.local/share/keyrings"
    };
    for (auto comp : componentIgnores) {
        if (hasComponent(fullpath, comp)) {
            return true;
        }
    }

    // Custom ignore rules configured by the user
    for (const auto& rule : s_CustomIgnoreRules) {
        if (rule.isPrefix) {
            // Path prefix rule
            if (fullpath.size() >= rule.rule.size() && fullpath.substr(0, rule.rule.size()) == rule.rule) {
                if (fullpath.size() == rule.rule.size() || fullpath[rule.rule.size()] == '/') {
                    return true;
                }
            }
        } else {
            // Directory / component name rule
            if (d_name && rule.rule == d_name) {
                return true;
            }
            if (hasComponent(fullpath, rule.component)) {
                return true;
            }
        }
    }

    return false;
}

std::vector<fileEntity> fileStructTable;
std::vector<char> fileTable;

static std::string s_CustomBinaryCachePath = "";

std::vector<std::string> getDefaultSearchRoots(bool existingOnly) {
    std::vector<std::string> candidates;
    std::string home = getHomeDir();

    if (!home.empty()) {
        candidates.push_back(home);
    }

    // User local applications
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome && xdgDataHome[0] != '\0') {
        candidates.push_back(std::string(xdgDataHome) + "/applications");
    } else if (!home.empty()) {
        candidates.push_back(home + "/.local/share/applications");
    }

    // Flatpak user exports
    if (xdgDataHome && xdgDataHome[0] != '\0') {
        candidates.push_back(std::string(xdgDataHome) + "/flatpak/exports/share/applications");
    } else if (!home.empty()) {
        candidates.push_back(home + "/.local/share/flatpak/exports/share/applications");
    }

    // System applications
    candidates.push_back("/usr/local/share/applications");
    candidates.push_back("/usr/share/applications");

    // Flatpak system exports
    candidates.push_back("/var/lib/flatpak/exports/share/applications");

    // Standard XDG_DATA_DIRS if defined
    const char* xdgDataDirs = std::getenv("XDG_DATA_DIRS");
    if (xdgDataDirs && xdgDataDirs[0] != '\0') {
        std::string dirs(xdgDataDirs);
        size_t start = 0;
        while (start < dirs.size()) {
            size_t end = dirs.find(':', start);
            if (end == std::string::npos) end = dirs.size();
            std::string dir = dirs.substr(start, end - start);
            if (!dir.empty()) {
                candidates.push_back(dir + "/applications");
            }
            start = end + 1;
        }
    }

    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    for (const auto& path : candidates) {
        if (path.empty()) continue;
        if (existingOnly) {
            std::error_code ec;
            if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
                continue;
            }
        }
        if (seen.insert(path).second) {
            result.push_back(path);
        }
    }

    return result;
}

std::string getBinaryCachePath() {
    if (!s_CustomBinaryCachePath.empty()) {
        return s_CustomBinaryCachePath;
    }
    std::string home = getHomeDir();
    if (!home.empty()) {
        return home + "/.fizzLocalCache.bin";
    }
    std::string rt = getUserRuntimeDir();
    if (!rt.empty()) {
        return rt + "/.fizzLocalCache.bin";
    }
    return "";
}

void setBinaryCachePath(const std::string& path) {
    s_CustomBinaryCachePath = path;
}

std::string_view classifyPath(std::string_view path, ExecState state) {
    // Check for Desktop Entry - always classified as "app"
    if (isDesktopEntry(path)) {
        return "app";
    }

    if (state == ExecState::Executable) {
        return "exec";
    }

    size_t slash = path.rfind('/');
    size_t dot = path.rfind('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot <= slash + 1)) {
        return "";
    }

    std::string ext(path.substr(dot + 1));
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    static const std::unordered_map<std::string, std::string_view> kExtensionCategories = [] {
        std::unordered_map<std::string, std::string_view> m;
        auto add = [&](std::initializer_list<const char*> exts, std::string_view cat) {
            for (auto e : exts) m[e] = cat;
        };
        add({"txt", "md", "rst", "log", "json", "toml", "yaml", "yml", "ini", "conf", "cfg", "csv"}, "text");
        add({"c", "cc", "cpp", "h", "hpp", "py", "js", "ts", "rs", "go", "java", "sh", "lua", "cmake"}, "code");
        add({"png", "jpg", "jpeg", "gif", "webp", "svg", "bmp"}, "image");
        add({"mp3", "flac", "wav", "ogg", "mp4", "mkv", "webm", "mov"}, "media");
        add({"pdf", "epub", "odt", "docx", "xlsx", "pptx"}, "doc");
        add({"zip", "tar", "gz", "xz", "zst", "7z", "rar"}, "archive");
        return m;
    }();

    auto it = kExtensionCategories.find(ext);
    if (it != kExtensionCategories.end()) {
        return it->second;
    }
    return "";
}

// //Weird wizard shit
// //This is 128 in decimal or 1 0 0 0 0 0 0 0 in Decimal, bit 7 acts as the toggle
// #define FT_EXEC_MASK 0x80

void addToLocalCache(unsigned long inode, unsigned char f_type, size_t f_length, std::string f_name, ExecState isExecutable){
    struct fileEntity fileEnt;
    fileEnt.inode = inode;
    fileEnt.f_offset = fileTable.size();
    fileEnt.f_length = f_length + 1;
    fileEnt.f_type = f_type;
    fileEnt.f_execState = isExecutable;
	//    if (isExecutable == ExecState::Executable) {
	// fileEnt.f_type = f_type | FT_EXEC_MASK;
	//    } else {
	// fileEnt.f_type = f_type;
	//    }
    for(int i=0;i<f_length ;i++){
	fileTable.push_back(f_name[i]);
	// fileTable[fileTableOffset++] = f_name[i];
    }
    fileStructTable.push_back(fileEnt);
    fileTable.push_back('\0');
}

struct DirId {
    dev_t dev;
    ino_t ino;
    bool operator==(const DirId& o) const { return dev == o.dev && ino == o.ino; }
};

struct DirIdHash {
    std::size_t operator()(const DirId& d) const {
        return std::hash<dev_t>()(d.dev) ^ (std::hash<ino_t>()(d.ino) << 1);
    }
};

static volatile sig_atomic_t s_interrupted = 0;
static struct sigaction s_oldSigInt{};
static struct sigaction s_oldSigTerm{};
static bool s_handlersInstalled = false;

void setSearchInterrupted(bool interrupted) {
    s_interrupted = interrupted ? 1 : 0;
}

bool isSearchInterrupted() {
    return s_interrupted != 0;
}

static void searchSignalHandler(int sig) {
    s_interrupted = 1;
    if (sig == SIGINT && s_oldSigInt.sa_handler &&
        s_oldSigInt.sa_handler != SIG_DFL && s_oldSigInt.sa_handler != SIG_IGN) {
        s_oldSigInt.sa_handler(sig);
    } else if (sig == SIGTERM && s_oldSigTerm.sa_handler &&
               s_oldSigTerm.sa_handler != SIG_DFL && s_oldSigTerm.sa_handler != SIG_IGN) {
        s_oldSigTerm.sa_handler(sig);
    }
}

void setupSearchSignalHandlers() {
    if (!s_handlersInstalled) {
        struct sigaction sa{};
        sa.sa_handler = searchSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, &s_oldSigInt);
        sigaction(SIGTERM, &sa, &s_oldSigTerm);
        s_handlersInstalled = true;
    }
}

void restoreSearchSignalHandlers() {
    if (s_handlersInstalled) {
        sigaction(SIGINT, &s_oldSigInt, nullptr);
        sigaction(SIGTERM, &s_oldSigTerm, nullptr);
        s_handlersInstalled = false;
    }
}

struct SearchSignalScope {
    bool installed{false};
    SearchSignalScope() {
        if (!s_handlersInstalled) {
            setupSearchSignalHandlers();
            installed = true;
        }
    }
    ~SearchSignalScope() {
        if (installed) {
            restoreSearchSignalHandlers();
        }
    }
};

static void performSystemFileSearch(const std::string& currPath,
                                   std::unordered_set<std::string>& indexedPaths,
                                   std::unordered_set<DirId, DirIdHash>& visitedDirs) {
    if (isSearchInterrupted()) {
        return;
    }

    struct stat dirStat;
    if (stat(currPath.c_str(), &dirStat) != 0 || !S_ISDIR(dirStat.st_mode)) {
        return; // Gracefully survive missing paths or permission errors (e.g. EACCES)
    }

    if (!visitedDirs.insert({dirStat.st_dev, dirStat.st_ino}).second) {
        return; // Avoid cycles or redundant traversals of overlapping roots via (dev, ino)
    }

    DIR* dirp = opendir(currPath.c_str());
    if (dirp == nullptr) {
        return; // Gracefully survive permission-restricted directories
    }

    struct dirent* dp;
    while (!isSearchInterrupted() && (dp = readdir(dirp)) != nullptr) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        if (isPathIgnored("", dp->d_name)) {
            continue;
        }

        std::string currFileName = currPath + (currPath.back() == '/' ? "" : "/") + dp->d_name;

        if (isPathIgnored(currFileName, dp->d_name)) {
            continue;
        }

        unsigned char dType = dp->d_type;
        if (dType == DT_UNKNOWN) {
            struct stat lst;
            if (lstat(currFileName.c_str(), &lst) == 0) {
                if (S_ISDIR(lst.st_mode)) dType = DT_DIR;
                else if (S_ISREG(lst.st_mode)) dType = DT_REG;
                else if (S_ISLNK(lst.st_mode)) dType = DT_LNK;
            }
        }

        if (dType == DT_LNK) {
            // Check if symlink target is a regular file (e.g. Flatpak .desktop exports).
            // Directory symlinks are intentionally not traversed or indexed to avoid cycles,
            // escaping configured search roots, and inotify watcher mismatches.
            struct stat targetStat;
            if (stat(currFileName.c_str(), &targetStat) == 0 && S_ISREG(targetStat.st_mode)) {
                dType = DT_REG;
            } else {
                continue;
            }
        }

        fileEntity currFileEnt;
        currFileEnt.inode = dp->d_ino;
        currFileEnt.f_length = std::strlen(dp->d_name);
        currFileEnt.f_type = dType;
        // Executable permission is NOT checked during traversal to avoid a stat(2) syscall
        // per regular file. The state is deferred to ExecState::Unknown and resolved lazily
        // via access(X_OK) only when a Search Hit is about to be displayed or launched.
        currFileEnt.f_execState = ExecState::Unknown;

        if (indexedPaths.insert(currFileName).second) {
            addToLocalCache(dp->d_ino, dType, currFileName.size(), currFileName, currFileEnt.f_execState);
        }

        if (dType == DT_DIR) {
            performSystemFileSearch(currFileName, indexedPaths, visitedDirs);
            if (isSearchInterrupted()) {
                break;
            }
        }
    }
    closedir(dirp);
}

static bool writeAll(int fd, const void* data, size_t size) {
    const char* ptr = static_cast<const char*>(data);
    while (size > 0) {
        ssize_t written = write(fd, ptr, size);
        if (written <= 0) {
            if (written < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return false;
        }
        ptr += written;
        size -= written;
    }
    return true;
}

void systemFileSearch(const std::vector<std::string>& searchRoots) {
    SearchSignalScope signalScope;

    fileStructTable.clear();
    fileTable.clear();

    const std::string binaryCachePath = getBinaryCachePath();
    std::error_code ec;
    fs::path parentDir = fs::path(binaryCachePath).parent_path();
    if (!parentDir.empty()) {
        fs::create_directories(parentDir, ec);
    }

    std::unordered_set<std::string> indexedPaths;
    std::unordered_set<DirId, DirIdHash> visitedDirs;

    for (const auto& root : searchRoots) {
        if (isSearchInterrupted()) break;
        if (root.empty()) continue;
        if (isPathIgnored(root, nullptr)) continue;
        performSystemFileSearch(root, indexedPaths, visitedDirs);
    }

    if (isSearchInterrupted()) {
        fileStructTable.clear();
        fileTable.clear();
        return;
    }

    const std::string tmpPath = binaryCachePath + ".tmp." + std::to_string(getpid());
    int fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        std::cerr << "[ERROR] Could not open temporary Binary Cache for writing: " << tmpPath << "\n";
        return;
    }
    fchmod(fd, 0600);

    size_t structCount = fileStructTable.size();
    size_t charCount = fileTable.size();

    auto writeFail = [&]() {
        std::cerr << "[ERROR] Failed to write Binary Cache data: " << tmpPath << "\n";
        close(fd);
        unlink(tmpPath.c_str());
    };

    if (isSearchInterrupted() || !writeAll(fd, &structCount, sizeof(structCount))) {
        writeFail();
        return;
    }
    if (isSearchInterrupted() || !writeAll(fd, &charCount, sizeof(charCount))) {
        writeFail();
        return;
    }
    if (structCount > 0) {
        if (isSearchInterrupted() || !writeAll(fd, fileStructTable.data(), structCount * sizeof(fileEntity))) {
            writeFail();
            return;
        }
    }
    if (charCount > 0) {
        if (isSearchInterrupted() || !writeAll(fd, fileTable.data(), charCount)) {
            writeFail();
            return;
        }
    }

    if (close(fd) != 0) {
        std::cerr << "[ERROR] Could not close temporary Binary Cache: " << tmpPath << "\n";
        unlink(tmpPath.c_str());
        return;
    }

    if (isSearchInterrupted()) {
        unlink(tmpPath.c_str());
        fileStructTable.clear();
        fileTable.clear();
        return;
    }

    if (rename(tmpPath.c_str(), binaryCachePath.c_str()) != 0) {
        std::cerr << "[ERROR] Could not publish Binary Cache via rename: " << strerror(errno) << "\n";
        unlink(tmpPath.c_str());
        return;
    }
}

void systemFileSearch(const std::string& currPath) {
    systemFileSearch(std::vector<std::string>{currPath});
}
