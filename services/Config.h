#pragma once
#include <string>
#include <vector>

struct FizzConfig {
    std::vector<std::string> searchRoots;
    std::vector<std::string> ignoreRules;
    bool autoCloseOnUnfocus = true;
};

std::string getHomeDir();
std::string getConfigPath();
void setConfigPath(const std::string& path);

FizzConfig loadConfig(const std::string& configPath = "");
