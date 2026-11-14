#pragma once

#include <string>
#include <string_view>
#include "imgui.h"

namespace fizz::theme {

// Color definitions matching GNOME Adwaita Dark palette
struct Palette {
    static constexpr ImVec4 kAccent        = {0.208f, 0.518f, 0.894f, 1.00f}; // Adwaita Blue (#3584e4)
    static constexpr ImVec4 kBg            = {0.141f, 0.141f, 0.141f, 0.94f}; // Adwaita Dark Surface (#242424 @ 94%)
    static constexpr ImVec4 kBorder        = {1.000f, 1.000f, 1.000f, 0.12f}; // Crisp translucent border highlight
    static constexpr ImVec4 kText          = {0.965f, 0.961f, 0.957f, 1.00f}; // Adwaita Light Text (#f6f5f4)
    static constexpr ImVec4 kDim           = {0.600f, 0.600f, 0.600f, 1.00f}; // Secondary / Muted Text
    static constexpr ImVec4 kFrameBg       = {1.000f, 1.000f, 1.000f, 0.06f}; // Elevated input frame surface
    static constexpr ImVec4 kFrameBgHover  = {1.000f, 1.000f, 1.000f, 0.09f};
    static constexpr ImVec4 kFrameBgActive = {1.000f, 1.000f, 1.000f, 0.12f};
    static constexpr ImVec4 kAmber         = {0.950f, 0.700f, 0.100f, 1.00f}; // Warning highlight

    // Category tag badge colors
    static constexpr ImVec4 kTagOrange     = {0.950f, 0.650f, 0.250f, 1.00f};
    static constexpr ImVec4 kTagBlue       = {0.450f, 0.650f, 0.950f, 1.00f};
    static constexpr ImVec4 kTagPurple     = {0.700f, 0.550f, 0.950f, 1.00f};
    static constexpr ImVec4 kTagPink       = {0.950f, 0.500f, 0.650f, 1.00f};
    static constexpr ImVec4 kTagGreen      = {0.550f, 0.850f, 0.450f, 1.00f};
    static constexpr ImVec4 kTagGrey       = {0.600f, 0.650f, 0.670f, 1.00f};
    static constexpr ImVec4 kTagTeal       = {0.200f, 0.780f, 0.720f, 1.00f}; // Crisp teal for doc
    static constexpr ImVec4 kTagAction     = {0.780f, 0.350f, 0.920f, 1.00f}; // Distinct violet for builtin actions
};

// Color definitions matching GNOME Adwaita Light palette
struct LightPalette {
    static constexpr ImVec4 kBg            = {0.965f, 0.961f, 0.957f, 0.96f}; // Adwaita Light Surface (#f6f5f4 @ 96%)
    static constexpr ImVec4 kBorder        = {0.000f, 0.000f, 0.000f, 0.12f}; // Crisp translucent border highlight
    static constexpr ImVec4 kText          = {0.141f, 0.141f, 0.141f, 1.00f}; // Adwaita Dark Text (#242424)
    static constexpr ImVec4 kDim           = {0.450f, 0.450f, 0.450f, 1.00f}; // Secondary / Muted Text
    static constexpr ImVec4 kFrameBg       = {0.000f, 0.000f, 0.000f, 0.05f}; // Light elevated input frame surface
    static constexpr ImVec4 kFrameBgHover  = {0.000f, 0.000f, 0.000f, 0.08f};
    static constexpr ImVec4 kFrameBgActive = {0.000f, 0.000f, 0.000f, 0.12f};
    static constexpr ImVec4 kAmber         = {0.850f, 0.550f, 0.050f, 1.00f}; // Warning highlight
};

// Curated accent color palette choices
struct Accents {
    static constexpr ImVec4 kBlue    = {0.208f, 0.518f, 0.894f, 1.00f}; // Adwaita Blue (#3584e4)
    static constexpr ImVec4 kEmerald = {0.180f, 0.761f, 0.494f, 1.00f}; // Emerald Green (#2ec27e)
    static constexpr ImVec4 kPurple  = {0.569f, 0.255f, 0.675f, 1.00f}; // Purple (#9141ac)
    static constexpr ImVec4 kAmber   = {0.949f, 0.702f, 0.102f, 1.00f}; // Amber (#f2b31a)
    static constexpr ImVec4 kCrimson = {0.878f, 0.106f, 0.141f, 1.00f}; // Crimson (#e01b24)
    static constexpr ImVec4 kRose    = {0.957f, 0.247f, 0.369f, 1.00f}; // Rose / Coral (#f43f5e)
    static constexpr ImVec4 kIndigo  = {0.388f, 0.400f, 0.945f, 1.00f}; // Electric Indigo (#6366f1)
    static constexpr ImVec4 kCyan    = {0.024f, 0.714f, 0.831f, 1.00f}; // Nord Cyan (#06b6d4)
    static constexpr ImVec4 kSunset  = {0.976f, 0.451f, 0.086f, 1.00f}; // Sunset Orange (#f97316)
    static constexpr ImVec4 kLime    = {0.518f, 0.800f, 0.086f, 1.00f}; // Neon Lime (#84cc16)
};

// Layout and soft rounding metrics
struct Metrics {
    static constexpr float kWindowRounding     = 12.0f;
    static constexpr float kFrameRounding      = 8.0f;
    static constexpr float kChildRounding      = 8.0f;
    static constexpr float kPopupRounding      = 8.0f;
    static constexpr float kSelectableRounding = 6.0f;
    static constexpr float kScrollbarRounding  = 8.0f;

    static constexpr float kWindowBorderSize   = 1.0f;
    static constexpr float kFrameBorderSize    = 0.0f;
    static constexpr float kChildBorderSize    = 1.0f;

    static constexpr ImVec2 kWindowPadding     = {16.0f, 14.0f};
    static constexpr ImVec2 kFramePadding      = {10.0f, 8.0f};
    static constexpr ImVec2 kItemSpacing       = {0.0f, 4.0f};
    static constexpr ImVec2 kItemInnerSpacing  = {6.0f, 4.0f};

    static constexpr float kRowHeight          = 24.0f;
    static constexpr float kRowTextIndent      = 16.0f;
    static constexpr float kIndicatorWidth     = 4.0f;
    static constexpr float kIndicatorMarginY   = 3.0f;
    static constexpr float kIndicatorMarginX   = 4.0f;
    static constexpr float kIndicatorRounding  = 2.0f;
};

// Indicator geometry calculation
struct IndicatorGeometry {
    ImVec2 min;
    ImVec2 max;
    float rounding;
};

// Modifies the alpha channel of an ImVec4 color
ImVec4 withAlpha(ImVec4 color, float alpha);

// Calculates row indicator strip bounds
IndicatorGeometry calculateIndicatorGeometry(const ImVec2& itemMin, const ImVec2& itemMax);

// Normalizes an accent color name or alias to canonical ID ("blue", "emerald", "purple", "amber", "crimson", "rose", "indigo", "cyan", "sunset", "lime").
// Falls back to "blue" if unrecognized.
std::string normalizeAccent(std::string_view accentName);

// Returns the color associated with an accent color ID ("blue", "emerald", "purple", "amber", "crimson", "rose", "indigo", "cyan", "sunset", "lime").
// Handles aliases (e.g. "green" -> emerald, "red" -> crimson, "orange" -> sunset, "coral" -> rose).
// Falls back to Adwaita Blue if unrecognized.
ImVec4 getAccentColor(std::string_view accentName);

// Returns the active accent color
ImVec4 getActiveAccent();

// Returns the active theme name ("dark" or "light")
std::string getActiveTheme();

// Sets active theme and accent state without requiring ImGuiStyle
void setActiveThemeAndAccent(std::string_view theme, std::string_view accent);

// Configures Dear ImGui styling with theme (Dark/Light) and accent color, applying soft rounding metrics
void applyTheme(ImGuiStyle& style, std::string_view theme = "dark", std::string_view accent = "blue");

} // namespace fizz::theme
