#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "FileSearch.h"

enum class DesktopLauncherBackend {
    Auto,
    Gio,
    GtkLaunch,
    ExecLine
};

bool isDesktopEntry(std::string_view path);

// Extract and parse command arguments from a Desktop Entry's [Desktop Entry] Exec= line
std::vector<std::string> parseDesktopExec(const std::string& desktopEntryPath);

std::vector<std::string> buildLaunchCommand(
    std::string_view path,
    ExecState state,
    bool showFolder,
    DesktopLauncherBackend backend = DesktopLauncherBackend::Auto
);

bool launchTarget(std::string_view path, ExecState state, bool showFolder);
