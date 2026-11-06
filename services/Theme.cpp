#include <algorithm>
#include "Theme.h"

namespace fizz::theme {

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

void applyTheme(ImGuiStyle& style) {
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

    // Selection highlight: Adwaita Blue translucent accent.
    // Hover intensifies the selection rather than dimming it.
    c[ImGuiCol_Header]         = withAlpha(Palette::kAccent, 0.20f);
    c[ImGuiCol_HeaderHovered]  = withAlpha(Palette::kAccent, 0.26f);
    c[ImGuiCol_HeaderActive]   = withAlpha(Palette::kAccent, 0.32f);

    // Scrollbar styling matching Adwaita dark
    c[ImGuiCol_ScrollbarBg]          = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_ScrollbarGrab]        = withAlpha(Palette::kText, 0.18f);
    c[ImGuiCol_ScrollbarGrabHovered] = withAlpha(Palette::kText, 0.30f);
    c[ImGuiCol_ScrollbarGrabActive]  = withAlpha(Palette::kText, 0.45f);
}

} // namespace fizz::theme
