#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "imgui.h"

namespace fizz::ui {

// Default search input placeholder hint text
constexpr std::string_view kDefaultPlaceholderText = "Search files and apps...";

// Layout and sizing metrics for modern UI components
struct Metrics {
    static constexpr float kSearchPillHeight   = 38.0f;
    static constexpr float kSearchPillRounding = 10.0f;
    static constexpr float kSearchPillPaddingX = 12.0f;
    static constexpr float kSearchPillPaddingY = 6.0f;
    static constexpr float kPromptGlyphWidth   = 12.0f;
    static constexpr float kPromptSpacing      = 8.0f;
    static constexpr float kFooterHeight       = 28.0f;
};

// Geometry bounds and positioning for integrated search pill
struct SearchPillGeometry {
    ImVec2 pillMin;
    ImVec2 pillMax;
    float rounding;
    ImVec2 promptPos;
    ImVec2 inputPos;
    float inputWidth;
};

// Navigation shortcut indicator definition
struct ShortcutItem {
    std::string_view key;
    std::string_view action;
};

// Layout calculations for the pinned status footer
struct FooterLayout {
    float footerHeight;
    float reservedBottomSpace;
    float reservedChildHeight; // Negative height offset for BeginChild
};

// Calculates geometry for the integrated search pill container, prompt, and input box
SearchPillGeometry calculateSearchPillGeometry(const ImVec2& origin, float width, float height, float fontSize = 16.0f);

// Formats search hit count string (e.g. "0 hits", "1 hit", "12 hits")
std::string formatHitCount(int count);

// Returns standard keyboard navigation shortcuts
const std::vector<ShortcutItem>& getNavigationShortcuts();

// Returns glyph ranges covering Latin, arrows (↵, ⇧), and symbols for font loading
const ImWchar* getUiGlyphRanges();

// Calculates layout dimensions for pinning the status footer cleanly below the search hits list
FooterLayout calculateFooterLayout(float footerHeight, float separatorSpacing = 4.0f);

// Renders the modern integrated search pill at top with prompt glyph and placeholder hint
bool renderSearchPill(char* buf, size_t bufSize, std::string_view placeholder = kDefaultPlaceholderText, bool autoFocus = true);

// Renders the pinned status footer at window bottom with search hit count and keyboard shortcuts
void renderStatusFooter(int hitCount, std::string_view warningMsg = "");

} // namespace fizz::ui
