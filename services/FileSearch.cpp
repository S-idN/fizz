#include <iostream>
#include <cstring>
#include <sys/stat.h>
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

namespace fs = std::filesystem;

// Ignore rules: skip system mount paths, common cache/vcs directories, and Binary Cache files
static std::vector<std::string> s_CustomIgnoreRules;

void setCustomIgnoreRules(const std::vector<std::string>& rules) {
    s_CustomIgnoreRules = rules;
}

const std::vector<std::string>& getCustomIgnoreRules() {
    return s_CustomIgnoreRules;
}

bool isPathIgnored(std::string_view fullpath, const char* d_name) {
    if (d_name) {
        if (strcmp(d_name, ".cache") == 0 ||
            strcmp(d_name, ".git") == 0 ||
            strcmp(d_name, "node_modules") == 0 ||
            strcmp(d_name, "Trash") == 0) {
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
        "/.local/share/Trash"
    };
    for (auto comp : componentIgnores) {
        size_t pos = fullpath.find(comp);
        if (pos != std::string_view::npos) {
            size_t end = pos + comp.size();
            if (end == fullpath.size() || fullpath[end] == '/') {
                return true;
            }
        }
    }

    // Custom ignore rules configured by the user
    for (const auto& rule : s_CustomIgnoreRules) {
        if (rule.empty()) continue;

        if (rule[0] == '/') {
            // Path prefix rule
            if (fullpath.size() >= rule.size() && fullpath.substr(0, rule.size()) == rule) {
                if (fullpath.size() == rule.size() || fullpath[rule.size()] == '/') {
                    return true;
                }
            }
        } else {
            // Directory / component name rule
            if (d_name && rule == d_name) {
                return true;
            }
            std::string comp = "/" + rule;
            size_t pos = fullpath.find(comp);
            if (pos != std::string_view::npos) {
                size_t end = pos + comp.size();
                if (end == fullpath.size() || fullpath[end] == '/') {
                    return true;
                }
            }
        }
    }

    return false;
}

std::vector<fileEntity> fileStructTable;
std::vector<char> fileTable;

static std::string s_CustomBinaryCachePath = "";

static std::string getHomeDir() {
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
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.fizzLocalCache.bin";
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
        return std::string(pw->pw_dir) + "/.fizzLocalCache.bin";
    }
    return "/tmp/.fizzLocalCache.bin";
}

void setBinaryCachePath(const std::string& path) {
    s_CustomBinaryCachePath = path;
}

std::string_view classifyPath(std::string_view path, ExecState state) {
    // Check for .desktop extension (case-insensitive) - always classified as "app"
    constexpr std::string_view kDesktopExt = ".desktop";
    if (path.size() >= kDesktopExt.size()) {
        std::string_view tail = path.substr(path.size() - kDesktopExt.size());
        bool matches = true;
        for (size_t i = 0; i < kDesktopExt.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(tail[i])) != kDesktopExt[i]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return "app";
        }
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

static void performSystemFileSearch(const std::string& currPath,
                                   std::unordered_set<std::string>& indexedPaths,
                                   std::unordered_set<std::string>& visitedDirs) {
    if (!visitedDirs.insert(currPath).second) {
        return; // Avoid cycles or redundant traversals of overlapping roots
    }

    DIR* dirp = opendir(currPath.c_str());
    if (dirp == nullptr) {
        return; // Gracefully survive permission-restricted paths (e.g. EACCES)
    }

    struct dirent* dp;
    while ((dp = readdir(dirp)) != nullptr) {
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
        if (dType == DT_UNKNOWN || dType == DT_LNK) {
            struct stat targetStat;
            if (stat(currFileName.c_str(), &targetStat) == 0) {
                if (S_ISDIR(targetStat.st_mode)) dType = DT_DIR;
                else if (S_ISREG(targetStat.st_mode)) dType = DT_REG;
            }
        }

        fileEntity currFileEnt;
        currFileEnt.inode = dp->d_ino;
        currFileEnt.f_length = std::strlen(dp->d_name);
        currFileEnt.f_type = dType;
        currFileEnt.f_execState = ExecState::NotExecutable;

        if (dType == DT_REG) {
            struct stat st;
            if (stat(currFileName.c_str(), &st) == 0) {
                bool isExec = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
                currFileEnt.f_execState = static_cast<ExecState>(isExec);
            }
        }

        if (indexedPaths.insert(currFileName).second) {
            addToLocalCache(dp->d_ino, dType, currFileName.size(), currFileName, currFileEnt.f_execState);
        }

        if (dType == DT_DIR) {
            performSystemFileSearch(currFileName, indexedPaths, visitedDirs);
        }
    }
    closedir(dirp);
}

void systemFileSearch(const std::vector<std::string>& searchRoots) {
    fileStructTable.clear();
    fileTable.clear();

    const std::string binaryCachePath = getBinaryCachePath();
    std::ofstream binaryCacheStream(binaryCachePath, std::ios::binary | std::ios::trunc);
    if (!binaryCacheStream.is_open()) {
        std::cerr << "[ERROR] Could not open Binary Cache for writing: " << binaryCachePath << "\n";
        return;
    }

    std::unordered_set<std::string> indexedPaths;
    std::unordered_set<std::string> visitedDirs;

    for (const auto& root : searchRoots) {
        if (root.empty()) continue;
        if (isPathIgnored(root, nullptr)) continue;
        performSystemFileSearch(root, indexedPaths, visitedDirs);
    }

    size_t structCount = fileStructTable.size();
    size_t charCount = fileTable.size();

    binaryCacheStream.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
    binaryCacheStream.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));

    if (structCount > 0) {
        binaryCacheStream.write(reinterpret_cast<const char*>(fileStructTable.data()), 
                                structCount * sizeof(fileEntity));
    }

    if (charCount > 0) {
        binaryCacheStream.write(fileTable.data(), charCount);
    }

    binaryCacheStream.close();
}

void systemFileSearch(const std::string& currPath) {
    systemFileSearch(std::vector<std::string>{currPath});
}
