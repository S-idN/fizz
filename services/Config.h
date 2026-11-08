#pragma once
#include <string>
#include <vector>
#include <utility>

struct FizzConfig {
    std::vector<std::string> searchRoots;
    std::vector<std::string> ignoreRules;
    bool autoCloseOnUnfocus = true;

    // Schema version
    int config_version = 1;

    // Visual appearance
    std::string theme = "dark";
    std::string accent = "blue";
    int window_width = 750;
    int window_height = 480;
    std::string font_path = "";
    float font_size = 16.0f;

    // Binary cache path
    std::string binary_cache_path = "";

    // View state grace duration (seconds)
    int view_state_grace_seconds = 5;

    // Preserved unrecognized keys for round-trip persistence
    std::vector<std::pair<std::string, std::string>> unrecognizedKeys;

    static FizzConfig defaults();
};

std::string getHomeDir();
std::string getConfigPath();
void setConfigPath(const std::string& path);

std::string getUserRuntimeDir();
void setCustomRuntimeDir(const std::string& path);

FizzConfig loadConfig(const std::string& configPath = "");
