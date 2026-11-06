#pragma once

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

// Configures Dear ImGui styling with Adwaita dark theme and soft rounding metrics
void applyTheme(ImGuiStyle& style);

} // namespace fizz::theme
