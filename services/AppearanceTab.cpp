#include "AppearanceTab.h"
#include <algorithm>
#include <cctype>

namespace fizz::ui {

static std::string toLower(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

const std::vector<ThemeOption>& getAvailableThemes() {
    static const std::vector<ThemeOption> s_themes = {
        {"dark", "Dark"},
        {"light", "Light"}
    };
    return s_themes;
}

const std::vector<CuratedAccent>& getCuratedAccents() {
    static const std::vector<CuratedAccent> s_accents = {
        {"blue", "Adwaita Blue", theme::Accents::kBlue},
        {"emerald", "Emerald Green", theme::Accents::kEmerald},
        {"purple", "Purple", theme::Accents::kPurple},
        {"amber", "Amber", theme::Accents::kAmber},
        {"crimson", "Crimson", theme::Accents::kCrimson}
    };
    return s_accents;
}

static std::string normalizeAccentId(std::string_view accentId) {
    std::string lower = toLower(accentId);
    if (lower == "emerald" || lower == "green") {
        return "emerald";
    }
    if (lower == "purple") {
        return "purple";
    }
    if (lower == "amber" || lower == "orange") {
        return "amber";
    }
    if (lower == "crimson" || lower == "red") {
        return "crimson";
    }
    return "blue";
}

bool selectTheme(FizzConfig& config, std::string_view themeId, ImGuiStyle* style) {
    std::string lower = toLower(themeId);
    std::string normalized = (lower == "light") ? "light" : "dark";

    bool changed = (config.theme != normalized);
    config.theme = normalized;

    if (style != nullptr) {
        theme::applyTheme(*style, config.theme, config.accent);
    } else if (ImGui::GetCurrentContext() != nullptr) {
        theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);
    } else {
        theme::setActiveThemeAndAccent(config.theme, config.accent);
    }

    saveConfig(config);
    return changed;
}

bool selectAccent(FizzConfig& config, std::string_view accentId, ImGuiStyle* style) {
    std::string normalized = normalizeAccentId(accentId);

    bool changed = (config.accent != normalized);
    config.accent = normalized;

    if (style != nullptr) {
        theme::applyTheme(*style, config.theme, config.accent);
    } else if (ImGui::GetCurrentContext() != nullptr) {
        theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);
    } else {
        theme::setActiveThemeAndAccent(config.theme, config.accent);
    }

    saveConfig(config);
    return changed;
}

bool renderAppearanceTab(FizzConfig& config) {
    bool modified = false;

    ImGui::Spacing();
    ImGui::TextColored(theme::getActiveAccent(), "Interface Theme");
    ImGui::Spacing();

    bool isDark = (config.theme != "light");
    if (ImGui::RadioButton("Dark Theme##theme_dark", isDark)) {
        if (!isDark) {
            selectTheme(config, "dark");
            modified = true;
        }
    }
    ImGui::SameLine(160.0f);
    if (ImGui::RadioButton("Light Theme##theme_light", !isDark)) {
        if (isDark) {
            selectTheme(config, "light");
            modified = true;
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Controls window surface, typography contrast, and panel backgrounds.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(theme::getActiveAccent(), "Accent Color");
    ImGui::Spacing();

    const auto& accents = getCuratedAccents();
    std::string activeNorm = normalizeAccentId(config.accent);

    for (size_t i = 0; i < accents.size(); ++i) {
        const auto& choice = accents[i];
        bool isSelected = (activeNorm == choice.id);

        ImGui::PushID((int)i);

        std::string radioId = "##radio_" + choice.id;
        if (ImGui::RadioButton(radioId.c_str(), isSelected)) {
            if (!isSelected) {
                selectAccent(config, choice.id);
                modified = true;
            }
        }
        ImGui::SameLine();

        ImVec2 chipSize(18.0f, 18.0f);
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        p0.y += 1.0f;
        ImVec2 p1(p0.x + chipSize.x, p0.y + chipSize.y);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(choice.color), 4.0f);
        if (isSelected) {
            dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Text), 4.0f, 0, 1.5f);
        }

        std::string chipBtnId = "##chip_" + choice.id;
        if (ImGui::InvisibleButton(chipBtnId.c_str(), chipSize)) {
            if (!isSelected) {
                selectAccent(config, choice.id);
                modified = true;
            }
        }
        ImGui::SameLine();

        std::string labelId = choice.displayName + "##label_" + choice.id;
        if (ImGui::Selectable(labelId.c_str(), isSelected, 0, ImVec2(160.0f, 0.0f))) {
            if (!isSelected) {
                selectAccent(config, choice.id);
                modified = true;
            }
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Highlights selected search hits, active navigation tabs, buttons, and app badges.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Current Theme: %s", (config.theme == "light" ? "Light" : "Dark"));
    ImGui::SameLine(180.0f);
    ImGui::Text("Current Accent: %s", config.accent.c_str());
    ImGui::Spacing();
    ImGui::TextDisabled("Changes apply live without restart and automatically persist to fizz.conf.");

    return modified;
}

} // namespace fizz::ui
