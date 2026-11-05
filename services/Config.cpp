#include "Config.h"
#include "FileSearch.h"
#include <cstdlib>
#include <cctype>
#include <pwd.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

static std::string s_CustomConfigPath = "";

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

static std::string normalizePath(std::string path) {
    path = trim(path);
    path = stripQuotes(path);
    path = expandTilde(path);
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

static std::vector<std::string> parseCommaSeparatedList(const std::string& val, bool isPathList) {
    std::vector<std::string> items;
    std::stringstream ss(val);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        item = stripQuotes(item);
        if (isPathList) {
            item = normalizePath(item);
        } else {
            while (item.size() > 1 && item.back() == '/') {
                item.pop_back();
            }
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

FizzConfig loadConfig(const std::string& configPath) {
    std::string path = configPath.empty() ? getConfigPath() : configPath;

    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
        return FizzConfig{ getDefaultSearchRoots(true), {} };
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return FizzConfig{ getDefaultSearchRoots(true), {} };
    }

    FizzConfig config;
    bool hasSearchRoots = false;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        size_t eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string val = trim(trimmed.substr(eqPos + 1));

        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lowerKey == "search_roots" || lowerKey == "searchroots") {
            config.searchRoots = parseCommaSeparatedList(val, true);
            hasSearchRoots = true;
        } else if (lowerKey == "ignore_rules" || lowerKey == "ignorerules") {
            config.ignoreRules = parseCommaSeparatedList(val, false);
        }
    }

    if (!hasSearchRoots || config.searchRoots.empty()) {
        config.searchRoots = getDefaultSearchRoots(true);
    }

    return config;
}
