#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "imgui.h"
#include "FileSearch.h"
#include "Theme.h"

namespace fizz::ui {

// Category badge representation with high-contrast palette color
struct CategoryBadge {
    std::string_view label;
    ImVec4 color;
};

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
    static constexpr float kBadgePaddingX      = 7.0f;
    static constexpr float kBadgeMarginY       = 3.0f;
    static constexpr float kBadgeMarginRight   = 10.0f;
    static constexpr float kBadgeSpacing       = 8.0f;
    static constexpr float kBadgeRounding      = 9.0f;
    static constexpr float kFileNameDirSpacing = 16.0f;
};

// Geometry bounds and positioning for category badge pill
struct BadgeGeometry {
    ImVec2 min;
    ImVec2 max;
    ImVec2 textPos;
    float width;
    float height;
    float rounding;
};

// Layout calculations and positioned elements for a search hit row
struct RowLayout {
    ImVec2 min;
    ImVec2 max;
    ImVec2 namePos;
    float nameWidth;
    ImVec2 dirPos;
    float dirWidth;
    std::string truncatedDir;
    bool hasBadge;
    BadgeGeometry badgeGeom;
    float textRightLimit;
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

// Calculates geometry for compact rounded pill badge on row right edge
BadgeGeometry calculateBadgeGeometry(const ImVec2& rowMin, const ImVec2& rowMax, float textWidth, float fontSize = 16.0f);

// Returns the category badge label and high-contrast color for a file path and ExecState
CategoryBadge getCategoryBadge(std::string_view path, ExecState state);

// Intelligently truncates directory path with an ellipsis (...) to fit within maxWidth
template<typename MeasureFn>
std::string truncatePath(std::string_view path, float maxWidth, MeasureFn&& measureFn) {
    if (path.empty() || maxWidth <= 0.0f) {
        return "";
    }

    if (measureFn(path) <= maxWidth) {
        return std::string(path);
    }

    static constexpr std::string_view kEllipsis = "...";
    float ellipsisW = measureFn(kEllipsis);
    if (maxWidth < ellipsisW) {
        return "";
    }

    bool isAbsolute = (path.front() == '/');
    std::vector<std::string_view> components;
    size_t start = 0;
    while (start < path.size()) {
        size_t slash = path.find('/', start);
        if (slash == std::string_view::npos) {
            std::string_view comp = path.substr(start);
            if (!comp.empty()) components.push_back(comp);
            break;
        }
        if (slash > start) {
            components.push_back(path.substr(start, slash - start));
        }
        start = slash + 1;
    }

    if (components.size() >= 2) {
        std::string bestCandidate;
        size_t bestScore = 0;

        auto buildCandidate = [&](size_t headCount, size_t tailCount) -> std::string {
            std::string s;
            s.reserve(path.size() + 4);
            if (isAbsolute) {
                s.push_back('/');
            }
            for (size_t k = 0; k < headCount; ++k) {
                s.append(components[k]);
                s.push_back('/');
            }
            s.append(kEllipsis);

            for (size_t k = components.size() - tailCount; k < components.size(); ++k) {
                s.push_back('/');
                s.append(components[k]);
            }
            return s;
        };

        for (size_t tail = 1; tail < components.size(); ++tail) {
            for (size_t head = 0; head + tail < components.size(); ++head) {
                std::string cand = buildCandidate(head, tail);
                float w = measureFn(cand);
                if (w <= maxWidth) {
                    size_t score = (head + tail) * 1000 + cand.size();
                    if (score > bestScore) {
                        bestCandidate = std::move(cand);
                        bestScore = score;
                    }
                }
            }
        }

        if (!bestCandidate.empty()) {
            return bestCandidate;
        }
    }

    // Fallback: character-level middle truncation (headChars + "..." + tailChars)
    size_t low = 0;
    size_t high = path.size() - 1;
    std::string bestCharCandidate = std::string(kEllipsis);

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        size_t headLen = mid / 2;
        size_t tailLen = mid - headLen;

        std::string cand;
        cand.reserve(mid + 3);
        cand.append(path.substr(0, headLen));
        cand.append(kEllipsis);
        cand.append(path.substr(path.size() - tailLen));

        if (measureFn(cand) <= maxWidth) {
            bestCharCandidate = std::move(cand);
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    return bestCharCandidate;
}

// Intelligently truncates directory path using default ImGui text measurement
std::string truncatePath(std::string_view path, float maxWidth);

// Calculates dynamic row layout allocating width for filename, category badge, and truncated directory path
template<typename MeasureFn>
RowLayout calculateRowLayout(const ImVec2& rowMin, const ImVec2& rowMax,
                             std::string_view fileName, std::string_view dirPath,
                             const CategoryBadge& badge, MeasureFn&& measureFn,
                             float fontSize = 16.0f) {
    float rowH = std::max(0.0f, rowMax.y - rowMin.y);
    float startX = rowMin.x + theme::Metrics::kRowTextIndent;
    float textY = rowMin.y + (rowH - fontSize) * 0.5f;

    bool hasBadge = !badge.label.empty();
    BadgeGeometry badgeGeom{};
    float textRightLimit = rowMax.x - Metrics::kBadgeMarginRight;

    if (hasBadge) {
        float badgeTextW = measureFn(badge.label);
        badgeGeom = calculateBadgeGeometry(rowMin, rowMax, badgeTextW, fontSize);
        textRightLimit = badgeGeom.min.x - Metrics::kBadgeSpacing;
    }

    float nameW = measureFn(fileName);
    ImVec2 namePos = {startX, textY};

    // Calculate directory path start and available width
    float dirStartX = startX + nameW + Metrics::kFileNameDirSpacing;
    float availDirWidth = std::max(0.0f, textRightLimit - dirStartX);

    std::string truncatedDir;
    float dirW = 0.0f;
    if (!dirPath.empty() && availDirWidth > 0.0f) {
        truncatedDir = truncatePath(dirPath, availDirWidth, measureFn);
        dirW = measureFn(truncatedDir);
    }

    ImVec2 dirPos = {dirStartX, textY};

    return RowLayout{
        rowMin,
        rowMax,
        namePos,
        nameW,
        dirPos,
        dirW,
        std::move(truncatedDir),
        hasBadge,
        badgeGeom,
        textRightLimit
    };
}

// Calculates dynamic row layout using default ImGui text measurement
RowLayout calculateRowLayout(const ImVec2& rowMin, const ImVec2& rowMax,
                             std::string_view fileName, std::string_view dirPath,
                             const CategoryBadge& badge, float fontSize = 16.0f);

// Renders category badge as a compact, rounded pill with translucent fill and crisp outline
void renderCategoryBadge(ImDrawList* dl, const CategoryBadge& badge, const BadgeGeometry& geom);

// Renders search hit row content (clipped text and category badge pill) cleanly into the draw list
void renderRowContent(ImDrawList* dl, const RowLayout& layout, std::string_view name, const CategoryBadge& badge);

// Renders the modern integrated search pill at top with prompt glyph and placeholder hint
bool renderSearchPill(char* buf, size_t bufSize, std::string_view placeholder = kDefaultPlaceholderText, bool autoFocus = true);

// Renders the pinned status footer at window bottom with search hit count and keyboard shortcuts
void renderStatusFooter(int hitCount, std::string_view warningMsg = "");

} // namespace fizz::ui
