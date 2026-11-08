#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "imgui.h"
#include "Config.h"
#include "Theme.h"

namespace fizz::ui {

// Information about an available theme option
struct ThemeOption {
    std::string id;          // "dark", "light"
    std::string displayName; // "Dark", "Light"
};

// Information about a curated accent palette choice
struct CuratedAccent {
    std::string id;          // "blue", "emerald", "purple", "amber", "crimson"
    std::string displayName; // "Adwaita Blue", "Emerald Green", "Purple", "Amber", "Crimson"
    ImVec4 color;            // RGBA color
};

// Returns list of available themes
const std::vector<ThemeOption>& getAvailableThemes();

// Returns list of curated accent colors
const std::vector<CuratedAccent>& getCuratedAccents();

// Selects a theme ("dark" or "light") for the given config, applies it to ImGuiStyle,
// and atomically persists to disk via saveConfig(). Returns true if theme changed.
bool selectTheme(FizzConfig& config, std::string_view themeId, ImGuiStyle* style = nullptr);

// Selects an accent color for the given config, applies it to ImGuiStyle,
// and atomically persists to disk via saveConfig(). Returns true if accent changed.
bool selectAccent(FizzConfig& config, std::string_view accentId, ImGuiStyle* style = nullptr);

// Renders the Appearance tab content inside the active Dear ImGui window/tab.
// Automatically applies changes live and saves configuration atomically.
// Returns true if either theme or accent was modified this frame.
bool renderAppearanceTab(FizzConfig& config);

} // namespace fizz::ui
