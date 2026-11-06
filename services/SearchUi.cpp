#include "SearchUi.h"
#include "Theme.h"
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
    float inputY = pillMin.y + (pillH - fontSize) * 0.5f;
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

std::vector<ShortcutItem> getNavigationShortcuts() {
    return {
        {"↵", "Open"},
        {"⇧↵", "Folder"},
        {"Esc", "Quit"}
    };
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

FooterLayout calculateFooterLayout(float contentHeight, float footerHeight, float separatorSpacing) {
    float clampedFooterH = std::max(0.0f, footerHeight);
    float clampedSpacing = std::max(0.0f, separatorSpacing);
    float reserved = clampedFooterH + clampedSpacing;
    float childH = -reserved;

    return FooterLayout{
        clampedFooterH,
        reserved,
        childH
    };
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

    if (!warningMsg.empty()) {
        // Prominent warning state (e.g. untrusted desktop entry)
        ImVec2 warnPos = {cursor.x + 4.0f, centerY};
        dl->AddText(warnPos, ImGui::GetColorU32(theme::Palette::kAmber), warningMsg.data(), warningMsg.data() + warningMsg.size());
    } else {
        // Match count on left: "N hits"
        std::string hitsText = formatHitCount(hitCount);
        ImVec2 hitsPos = {cursor.x + 4.0f, centerY};
        dl->AddText(hitsPos, ImGui::GetColorU32(theme::Palette::kDim), hitsText.c_str());

        // Keyboard navigation shortcuts formatted legibly on right
        auto shortcuts = getNavigationShortcuts();

        float badgePadX = 5.0f;
        float itemGap = 12.0f;
        float keyLabelGap = 5.0f;

        float totalShortcutsW = 0.0f;
        for (size_t i = 0; i < shortcuts.size(); ++i) {
            float keyW = ImGui::CalcTextSize(shortcuts[i].key.c_str()).x;
            float badgeW = keyW + 2.0f * badgePadX;
            float labelW = ImGui::CalcTextSize(shortcuts[i].action.c_str()).x;
            totalShortcutsW += badgeW + keyLabelGap + labelW;
            if (i + 1 < shortcuts.size()) {
                totalShortcutsW += itemGap;
            }
        }

        float curX = cursor.x + availW - totalShortcutsW - 4.0f;
        for (const auto& item : shortcuts) {
            float keyW = ImGui::CalcTextSize(item.key.c_str()).x;
            float badgeW = keyW + 2.0f * badgePadX;
            float badgeH = ImGui::GetFontSize() + 4.0f;
            float badgeY = cursor.y + (footerH - badgeH) * 0.5f + 2.0f;

            // Key badge background & subtle border
            ImVec2 bMin = {curX, badgeY};
            ImVec2 bMax = {curX + badgeW, badgeY + badgeH};
            dl->AddRectFilled(bMin, bMax, ImGui::GetColorU32(theme::Palette::kFrameBg), 4.0f);
            dl->AddRect(bMin, bMax, ImGui::GetColorU32(theme::Palette::kBorder), 4.0f);

            // Key indicator text
            ImVec2 keyTextPos = {curX + badgePadX, centerY};
            dl->AddText(keyTextPos, ImGui::GetColorU32(theme::Palette::kText), item.key.c_str());
            curX += badgeW + keyLabelGap;

            // Action label
            ImVec2 labelPos = {curX, centerY};
            dl->AddText(labelPos, ImGui::GetColorU32(theme::Palette::kDim), item.action.c_str());
            float labelW = ImGui::CalcTextSize(item.action.c_str()).x;
            curX += labelW + itemGap;
        }
    }
}

} // namespace fizz::ui
