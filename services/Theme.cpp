#include <algorithm>
#include <cctype>
#include "Theme.h"

namespace fizz::theme {

static ImVec4 g_activeAccent = Accents::kBlue;
static std::string g_activeTheme = "dark";

static std::string toLower(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

ImVec4 withAlpha(ImVec4 color, float alpha) {
    color.w = std::clamp(alpha, 0.0f, 1.0f);
    return color;
}

IndicatorGeometry calculateIndicatorGeometry(const ImVec2& itemMin, const ImVec2& itemMax) {
    float startX = itemMin.x + Metrics::kIndicatorMarginX;
    float endX   = startX + Metrics::kIndicatorWidth;

    float startY = itemMin.y + Metrics::kIndicatorMarginY;
    float endY   = itemMax.y - Metrics::kIndicatorMarginY;
    if (endY < startY) {
        endY = startY;
    }

    return IndicatorGeometry{
        ImVec2(startX, startY),
        ImVec2(endX, endY),
        Metrics::kIndicatorRounding
    };
}

ImVec4 getAccentColor(std::string_view accentName) {
    std::string lower = toLower(accentName);
    if (lower == "emerald" || lower == "green") {
        return Accents::kEmerald;
    }
    if (lower == "purple") {
        return Accents::kPurple;
    }
    if (lower == "amber" || lower == "orange") {
        return Accents::kAmber;
    }
    if (lower == "crimson" || lower == "red") {
        return Accents::kCrimson;
    }
    // Default and "blue"
    return Accents::kBlue;
}

ImVec4 getActiveAccent() {
    return g_activeAccent;
}

std::string getActiveTheme() {
    return g_activeTheme;
}

void setActiveThemeAndAccent(std::string_view theme, std::string_view accent) {
    std::string lowerTheme = toLower(theme);
    g_activeTheme = (lowerTheme == "light") ? "light" : "dark";
    g_activeAccent = getAccentColor(accent);
}

void applyTheme(ImGuiStyle& style, std::string_view theme, std::string_view accent) {
    setActiveThemeAndAccent(theme, accent);
    bool isLight = (g_activeTheme == "light");
    ImVec4 accentColor = g_activeAccent;

    style.WindowRounding    = Metrics::kWindowRounding;
    style.FrameRounding     = Metrics::kFrameRounding;
    style.ChildRounding     = Metrics::kChildRounding;
    style.PopupRounding     = Metrics::kPopupRounding;
    style.SelectableRounding= Metrics::kSelectableRounding;
    style.ScrollbarRounding = Metrics::kScrollbarRounding;

    style.WindowBorderSize  = Metrics::kWindowBorderSize;
    style.FrameBorderSize   = Metrics::kFrameBorderSize;
    style.ChildBorderSize   = Metrics::kChildBorderSize;

    style.WindowPadding     = Metrics::kWindowPadding;
    style.FramePadding      = Metrics::kFramePadding;
    style.ItemSpacing       = Metrics::kItemSpacing;
    style.ItemInnerSpacing  = Metrics::kItemInnerSpacing;

    ImVec4* c = style.Colors;

    if (isLight) {
        c[ImGuiCol_WindowBg]       = LightPalette::kBg;
        c[ImGuiCol_ChildBg]        = {0.0f, 0.0f, 0.0f, 0.0f};
        c[ImGuiCol_PopupBg]        = LightPalette::kBg;
        c[ImGuiCol_Border]         = LightPalette::kBorder;
        c[ImGuiCol_BorderShadow]   = {0.0f, 0.0f, 0.0f, 0.0f};
        c[ImGuiCol_Text]           = LightPalette::kText;
        c[ImGuiCol_TextDisabled]   = LightPalette::kDim;
        c[ImGuiCol_FrameBg]        = LightPalette::kFrameBg;
        c[ImGuiCol_FrameBgHovered] = LightPalette::kFrameBgHover;
        c[ImGuiCol_FrameBgActive]  = LightPalette::kFrameBgActive;
        c[ImGuiCol_Separator]      = LightPalette::kBorder;

        c[ImGuiCol_Header]         = withAlpha(accentColor, 0.20f);
        c[ImGuiCol_HeaderHovered]  = withAlpha(accentColor, 0.26f);
        c[ImGuiCol_HeaderActive]   = withAlpha(accentColor, 0.32f);

        c[ImGuiCol_Button]         = LightPalette::kFrameBg;
        c[ImGuiCol_ButtonHovered]  = LightPalette::kFrameBgHover;
        c[ImGuiCol_ButtonActive]   = LightPalette::kFrameBgActive;

        c[ImGuiCol_CheckMark]      = accentColor;
        c[ImGuiCol_SliderGrab]     = accentColor;
        c[ImGuiCol_SliderGrabActive] = withAlpha(accentColor, 0.80f);

        c[ImGuiCol_Tab]            = LightPalette::kFrameBg;
        c[ImGuiCol_TabHovered]     = withAlpha(accentColor, 0.25f);
        c[ImGuiCol_TabActive]      = withAlpha(accentColor, 0.40f);
        c[ImGuiCol_TabUnfocused]   = LightPalette::kFrameBg;
        c[ImGuiCol_TabUnfocusedActive] = withAlpha(accentColor, 0.20f);

        c[ImGuiCol_NavHighlight]   = accentColor;

        c[ImGuiCol_ScrollbarBg]          = {0.0f, 0.0f, 0.0f, 0.0f};
        c[ImGuiCol_ScrollbarGrab]        = withAlpha(LightPalette::kText, 0.18f);
        c[ImGuiCol_ScrollbarGrabHovered] = withAlpha(LightPalette::kText, 0.30f);
        c[ImGuiCol_ScrollbarGrabActive]  = withAlpha(LightPalette::kText, 0.45f);
    } else {
        c[ImGuiCol_WindowBg]       = Palette::kBg;
        c[ImGuiCol_ChildBg]        = {0.0f, 0.0f, 0.0f, 0.0f};
        c[ImGuiCol_PopupBg]        = Palette::kBg;
        c[ImGuiCol_Border]         = Palette::kBorder;
        c[ImGuiCol_BorderShadow]   = {0.0f, 0.0f, 0.0f, 0.0f};
        c[ImGuiCol_Text]           = Palette::kText;
        c[ImGuiCol_TextDisabled]   = Palette::kDim;
        c[ImGuiCol_FrameBg]        = Palette::kFrameBg;
        c[ImGuiCol_FrameBgHovered] = Palette::kFrameBgHover;
        c[ImGuiCol_FrameBgActive]  = Palette::kFrameBgActive;
        c[ImGuiCol_Separator]      = Palette::kBorder;

        c[ImGuiCol_Header]         = withAlpha(accentColor, 0.20f);
        c[ImGuiCol_HeaderHovered]  = withAlpha(accentColor, 0.26f);
        c[ImGuiCol_HeaderActive]   = withAlpha(accentColor, 0.32f);

        c[ImGuiCol_Button]         = Palette::kFrameBg;
        c[ImGuiCol_ButtonHovered]  = Palette::kFrameBgHover;
        c[ImGuiCol_ButtonActive]   = Palette::kFrameBgActive;

        c[ImGuiCol_CheckMark]      = accentColor;
        c[ImGuiCol_SliderGrab]     = accentColor;
        c[ImGuiCol_SliderGrabActive] = withAlpha(accentColor, 0.80f);

        c[ImGuiCol_Tab]            = Palette::kFrameBg;
        c[ImGuiCol_TabHovered]     = withAlpha(accentColor, 0.35f);
        c[ImGuiCol_TabActive]      = withAlpha(accentColor, 0.50f);
        c[ImGuiCol_TabUnfocused]   = Palette::kFrameBg;
        c[ImGuiCol_TabUnfocusedActive] = withAlpha(accentColor, 0.30f);

        c[ImGuiCol_NavHighlight]   = accentColor;

        c[ImGuiCol_ScrollbarBg]          = {0.0f, 0.0f, 0.0f, 0.0f};
        c[ImGuiCol_ScrollbarGrab]        = withAlpha(Palette::kText, 0.18f);
        c[ImGuiCol_ScrollbarGrabHovered] = withAlpha(Palette::kText, 0.30f);
        c[ImGuiCol_ScrollbarGrabActive]  = withAlpha(Palette::kText, 0.45f);
    }
}

} // namespace fizz::theme
