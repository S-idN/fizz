#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include "imgui.h"
#include "Config.h"
#include "Theme.h"

namespace fizz::ui {

// Window size constraints
constexpr int kMinWidth = 500;
constexpr int kMaxWidth = 1600;
constexpr int kMinHeight = 320;
constexpr int kMaxHeight = 1200;

// Font size constraints
constexpr float kMinFontSize = 10.0f;
constexpr float kMaxFontSize = 32.0f;
constexpr float kDefaultFontSize = 16.0f;

// Boundary clamping helpers
int clampWindowWidth(int width);
int clampWindowHeight(int height);
float clampFontSize(float fontSize);

// Monitor work area rectangle (in screen coordinates)
struct MonitorWorkArea {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool empty() const {
        return width <= 0 || height <= 0;
    }
};

// 2D window position coordinates
struct WindowPosition {
    int x = 0;
    int y = 0;

    bool operator==(const WindowPosition& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const WindowPosition& other) const {
        return !(*this == other);
    }
};

// Calculates the centered window position within a monitor work area.
// Coordinates are centered at:
//   target_x = work_x + (work_width - window_width) / 2
//   target_y = work_y + (work_height - window_height) / 2
// Clamps coordinates so that the window stays within monitor bounds.
// If work area is empty or unavailable (width <= 0 or height <= 0),
// gracefully falls back to fallbackPos.
WindowPosition calculateCenteredWindowPos(
    const MonitorWorkArea& workArea,
    int windowWidth,
    int windowHeight,
    const WindowPosition& fallbackPos = {0, 0});

// Finds the index of the active monitor containing the given window center point.
// Returns -1 if no monitor contains the point or if monitors is empty.
int findActiveMonitorIndex(
    const std::vector<MonitorWorkArea>& monitors,
    int centerX,
    int centerY);

// Information about an available theme option
struct ThemeOption {
    std::string id;          // "dark", "light"
    std::string displayName; // "Dark", "Light"
};

// Information about a curated accent palette choice
struct CuratedAccent {
    std::string id;          // "blue", "emerald", "purple", "amber", "crimson", "rose", "indigo", "cyan", "sunset", "lime"
    std::string displayName; // e.g. "Adwaita Blue", "Rose / Coral", etc.
    ImVec4 color;            // RGBA color
};

// Callbacks for live window resizing and font atlas rebuilding
struct AppearanceCallbacks {
    std::function<void(int width, int height)> onWindowResize;
    std::function<void(const std::string& fontPath, float fontSize)> onFontRebuild;
    std::function<void()> onGpuTextureReload;
};

// Global / default callback setters
void setWindowResizeCallback(std::function<void(int width, int height)> cb);
void setFontRebuildCallback(std::function<void(const std::string& fontPath, float fontSize)> cb);

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

// Sets window size clamped between [500..1600] and [320..1200], persists to config,
// and triggers the resize callback. Returns true if modified.
bool setWindowSize(FizzConfig& config, int width, int height,
                   const std::function<void(int, int)>& onResize = nullptr);

// Rebuilds the Dear ImGui font atlas safely at runtime:
// Clears fonts, loads custom TTF/OTF or discovers system font,
// falls back to AddFontDefault, bakes atlas, and reloads GPU textures.
bool rebuildFontAtlas(const std::string& fontPath, float fontSize,
                      const std::function<void()>& onGpuTextureReload = nullptr,
                      ImFontAtlas* atlas = nullptr);

// Sets font size clamped between [10.0f..32.0f], rebuilds font atlas,
// persists to config, and triggers font rebuild callback. Returns true if modified.
bool setFontSize(FizzConfig& config, float fontSize,
                 const std::function<void(const std::string&, float)>& onFontRebuild = nullptr);

// Sets font path, rebuilds font atlas, persists to config,
// and triggers font rebuild callback. Returns true if modified.
bool setFontPath(FizzConfig& config, const std::string& fontPath,
                 const std::function<void(const std::string&, float)>& onFontRebuild = nullptr);

// Renders the Appearance tab content inside the active Dear ImGui window/tab.
// Automatically applies changes live and saves configuration atomically.
// Returns true if either theme, accent, window geometry, or font was modified this frame.
bool renderAppearanceTab(FizzConfig& config, const AppearanceCallbacks& callbacks = {});

} // namespace fizz::ui

// Compatibility helpers for Dear ImGui backend texture refresh across versions
inline void ImGui_ImplOpenGL3_DestroyFontsTexture() {}
inline bool ImGui_ImplOpenGL3_CreateFontsTexture() { return true; }
