#include "SearchUi.h"
#include "Theme.h"
#include "AppLauncher.h"
#include <algorithm>

namespace fizz::ui {

SearchPillGeometry calculateSearchPillGeometry(const ImVec2& origin, float width, float height, float fontSize) {
    float pillW = std::max(0.0f, width);
    float pillH = std::max(0.0f, height);
    ImVec2 pillMin = origin;
    ImVec2 pillMax = {origin.x + pillW, origin.y + pillH};

    float promptX = pillMin.x + Metrics::kSearchPillPaddingX;
    float promptY = pillMin.y + (pillH - fontSize) * 0.5f;

    float inputX = promptX + Metrics::kPromptGlyphWidth + Metrics::kPromptSpacing;
    float inputY = pillMin.y;
    float inputW = std::max(0.0f, pillMax.x - Metrics::kSearchPillPaddingX - inputX);

    return SearchPillGeometry{
        pillMin,
        pillMax,
        Metrics::kSearchPillRounding,
        ImVec2(promptX, promptY),
        ImVec2(inputX, inputY),
        inputW
    };
}

std::string formatHitCount(int count) {
    if (count <= 0) {
        return "0 hits";
    }
    if (count == 1) {
        return "1 hit";
    }
    return std::to_string(count) + " hits";
}

const std::vector<ShortcutItem>& getNavigationShortcuts() {
    static const std::vector<ShortcutItem> shortcuts = {
        {"↵", "Open"},
        {"⇧↵", "Folder"},
        {"Esc", "Quit"}
    };
    return shortcuts;
}

const ImWchar* getUiGlyphRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x2190, 0x21FF, // Arrows (includes ↵ U+21B5, ⇧ U+21E7)
        0x26A0, 0x26A0, // Warning sign ⚠️
        0,
    };
    return ranges;
}

FooterLayout calculateFooterLayout(float footerHeight, float separatorSpacing) {
    float clampedFooterH = std::max(0.0f, footerHeight);
    float clampedSpacing = std::max(0.0f, separatorSpacing);
    float reserved = clampedFooterH + clampedSpacing;

    return FooterLayout{
        clampedFooterH,
        reserved,
        -reserved
    };
}

BadgeGeometry calculateBadgeGeometry(const ImVec2& rowMin, const ImVec2& rowMax, float textWidth, float fontSize) {
    float badgeW = textWidth + 2.0f * Metrics::kBadgePaddingX;
    float badgeMinX = rowMax.x - Metrics::kBadgeMarginRight - badgeW;
    float badgeMaxX = rowMax.x - Metrics::kBadgeMarginRight;
    float badgeMinY = rowMin.y + Metrics::kBadgeMarginY;
    float badgeMaxY = rowMax.y - Metrics::kBadgeMarginY;
    float badgeH = std::max(0.0f, badgeMaxY - badgeMinY);
    float rounding = badgeH * 0.5f;

    float textX = badgeMinX + Metrics::kBadgePaddingX;
    float textY = badgeMinY + (badgeH - fontSize) * 0.5f;

    return BadgeGeometry{
        ImVec2(badgeMinX, badgeMinY),
        ImVec2(badgeMaxX, badgeMaxY),
        ImVec2(textX, textY),
        badgeW,
        badgeH,
        rounding
    };
}

CategoryBadge getCategoryBadge(std::string_view path, ExecState state) {
    std::string_view cat = classifyPath(path, state);
    if (cat.empty()) {
        return {"", ImVec4(0, 0, 0, 0)};
    }

    if (cat == "app") {
        if (!isDesktopEntryTrusted(path)) {
            return {"untrusted-app", theme::Palette::kAmber};
        }
        return {"app", theme::Palette::kAccent};
    }

    struct CategoryMapping {
        std::string_view cat;
        std::string_view label;
        ImVec4 color;
    };

    static constexpr CategoryMapping kTable[] = {
        {"exec",    "exec",    theme::Palette::kTagOrange},
        {"code",    "code",    theme::Palette::kTagPurple},
        {"doc",     "doc",     theme::Palette::kTagTeal},
        {"media",   "media",   theme::Palette::kTagGreen},
        {"image",   "image",   theme::Palette::kTagPink},
        {"archive", "archive", theme::Palette::kTagGrey},
        {"text",    "text",    theme::Palette::kTagBlue},
    };

    for (const auto& item : kTable) {
        if (cat == item.cat) {
            return {item.label, item.color};
        }
    }

    return {"", ImVec4(0, 0, 0, 0)};
}

std::string truncatePath(std::string_view path, float maxWidth) {
    return truncatePath(path, maxWidth, [](std::string_view s) {
        return ImGui::CalcTextSize(s.data(), s.data() + s.size()).x;
    });
}

RowLayout calculateRowLayout(const ImVec2& rowMin, const ImVec2& rowMax,
                             std::string_view fileName, std::string_view dirPath,
                             const CategoryBadge& badge, float fontSize) {
    return calculateRowLayout(rowMin, rowMax, fileName, dirPath, badge, [](std::string_view s) {
        return ImGui::CalcTextSize(s.data(), s.data() + s.size()).x;
    }, fontSize);
}

void renderCategoryBadge(ImDrawList* dl, const CategoryBadge& badge, const BadgeGeometry& geom) {
    if (!dl || badge.label.empty() || geom.width <= 0.0f || geom.height <= 0.0f) {
        return;
    }

    // Translucent filled background pill
    dl->AddRectFilled(geom.min, geom.max, ImGui::GetColorU32(theme::withAlpha(badge.color, 0.16f)), geom.rounding);

    // Subtle crisp border outline
    dl->AddRect(geom.min, geom.max, ImGui::GetColorU32(theme::withAlpha(badge.color, 0.45f)), geom.rounding);

    // Centered category text
    dl->AddText(geom.textPos, ImGui::GetColorU32(badge.color), badge.label.data(), badge.label.data() + badge.label.size());
}

void renderRowContent(ImDrawList* dl, const RowLayout& layout, std::string_view name, const CategoryBadge& badge) {
    if (!dl) return;

    dl->PushClipRect(layout.min, {layout.textRightLimit, layout.max.y}, true);
    dl->AddText(layout.namePos, ImGui::GetColorU32(theme::Palette::kText), name.data(), name.data() + name.size());
    if (!layout.truncatedDir.empty()) {
        dl->AddText(layout.dirPos, ImGui::GetColorU32(theme::Palette::kDim),
                    layout.truncatedDir.data(), layout.truncatedDir.data() + layout.truncatedDir.size());
    }
    dl->PopClipRect();

    if (layout.hasBadge) {
        renderCategoryBadge(dl, badge, layout.badgeGeom);
    }
}

bool renderSearchPill(char* buf, size_t bufSize, std::string_view placeholder, bool autoFocus) {
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    float pillH = Metrics::kSearchPillHeight;

    auto geom = calculateSearchPillGeometry(cursor, availW, pillH, ImGui::GetFontSize());

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Draw pill background and border
    dl->AddRectFilled(geom.pillMin, geom.pillMax, ImGui::GetColorU32(theme::Palette::kFrameBg), geom.rounding);
    dl->AddRect(geom.pillMin, geom.pillMax, ImGui::GetColorU32(theme::Palette::kBorder), geom.rounding);

    // Draw prominent prompt indicator in Adwaita blue accent
    dl->AddText(geom.promptPos, ImGui::GetColorU32(theme::Palette::kAccent), ">");

    // Position and render InputTextWithHint inside the pill container
    ImGui::SetCursorScreenPos(geom.inputPos);
    ImGui::SetNextItemWidth(geom.inputWidth);

    if (autoFocus) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, (pillH - ImGui::GetFontSize()) * 0.5f));

    bool changed = ImGui::InputTextWithHint("##q", placeholder.data(), buf, bufSize);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    // Advance cursor past the pill height
    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + pillH + theme::Metrics::kItemSpacing.y));

    return changed;
}

void renderStatusFooter(int hitCount, std::string_view warningMsg) {
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    float footerH = Metrics::kFooterHeight;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Clean separation line above footer
    dl->AddLine(
        ImVec2(cursor.x, cursor.y),
        ImVec2(cursor.x + availW, cursor.y),
        ImGui::GetColorU32(theme::Palette::kBorder)
    );

    float centerY = cursor.y + (footerH - ImGui::GetFontSize()) * 0.5f + 2.0f;

    // Search hit count or warning on left
    if (!warningMsg.empty()) {
        ImVec2 warnPos = {cursor.x + 4.0f, centerY};
        dl->AddText(warnPos, ImGui::GetColorU32(theme::Palette::kAmber), warningMsg.data(), warningMsg.data() + warningMsg.size());
    } else {
        std::string hitsText = formatHitCount(hitCount);
        ImVec2 hitsPos = {cursor.x + 4.0f, centerY};
        dl->AddText(hitsPos, ImGui::GetColorU32(theme::Palette::kDim), hitsText.c_str());
    }

    // Keyboard navigation shortcuts formatted legibly on right
    const auto& shortcuts = getNavigationShortcuts();

    float badgePadX = 5.0f;
    float itemGap = 12.0f;
    float keyLabelGap = 5.0f;

    struct MeasuredShortcut {
        float keyW;
        float labelW;
        float badgeW;
        float itemW;
    };
    std::vector<MeasuredShortcut> measured(shortcuts.size());
    float totalShortcutsW = 0.0f;
    for (size_t i = 0; i < shortcuts.size(); ++i) {
        measured[i].keyW = ImGui::CalcTextSize(shortcuts[i].key.data(), shortcuts[i].key.data() + shortcuts[i].key.size()).x;
        measured[i].labelW = ImGui::CalcTextSize(shortcuts[i].action.data(), shortcuts[i].action.data() + shortcuts[i].action.size()).x;
        measured[i].badgeW = measured[i].keyW + 2.0f * badgePadX;
        measured[i].itemW = measured[i].badgeW + keyLabelGap + measured[i].labelW;
        totalShortcutsW += measured[i].itemW;
        if (i + 1 < shortcuts.size()) {
            totalShortcutsW += itemGap;
        }
    }

    float curX = cursor.x + availW - totalShortcutsW - 4.0f;
    for (size_t i = 0; i < shortcuts.size(); ++i) {
        const auto& item = shortcuts[i];
        const auto& m = measured[i];
        float badgeH = ImGui::GetFontSize() + 4.0f;
        float badgeY = cursor.y + (footerH - badgeH) * 0.5f + 2.0f;

        // Key badge background & subtle border
        ImVec2 bMin = {curX, badgeY};
        ImVec2 bMax = {curX + m.badgeW, badgeY + badgeH};
        dl->AddRectFilled(bMin, bMax, ImGui::GetColorU32(theme::Palette::kFrameBg), 4.0f);
        dl->AddRect(bMin, bMax, ImGui::GetColorU32(theme::Palette::kBorder), 4.0f);

        // Key indicator text
        ImVec2 keyTextPos = {curX + badgePadX, centerY};
        dl->AddText(keyTextPos, ImGui::GetColorU32(theme::Palette::kText), item.key.data(), item.key.data() + item.key.size());
        curX += m.badgeW + keyLabelGap;

        // Action label
        ImVec2 labelPos = {curX, centerY};
        dl->AddText(labelPos, ImGui::GetColorU32(theme::Palette::kDim), item.action.data(), item.action.data() + item.action.size());
        curX += m.labelW + itemGap;
    }

    // Advance layout cursor to reserve footer area cleanly
    ImGui::Dummy(ImVec2(availW, footerH));
}

} // namespace fizz::ui
