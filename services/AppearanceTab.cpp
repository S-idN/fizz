#include "AppearanceTab.h"
#include "FontDiscovery.h"
#include "SearchUi.h"
#include "imgui_internal.h"
#include "IconsFontAwesome7.h"
#include "IconsFontAwesome7Brands.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <climits>
#include <unistd.h>

namespace fizz::ui {

static std::function<void(int, int)> s_globalWindowResizeCallback = nullptr;
static std::function<void(const std::string&, float)> s_globalFontRebuildCallback = nullptr;

int clampWindowWidth(int width) {
    return std::clamp(width, kMinWidth, kMaxWidth);
}

int clampWindowHeight(int height) {
    return std::clamp(height, kMinHeight, kMaxHeight);
}

float clampFontSize(float fontSize) {
    return std::clamp(fontSize, kMinFontSize, kMaxFontSize);
}

void setWindowResizeCallback(std::function<void(int width, int height)> cb) {
    s_globalWindowResizeCallback = std::move(cb);
}

void setFontRebuildCallback(std::function<void(const std::string& fontPath, float fontSize)> cb) {
    s_globalFontRebuildCallback = std::move(cb);
}

bool setWindowSize(FizzConfig& config, int width, int height,
                   const std::function<void(int, int)>& onResize) {
    int clampedW = clampWindowWidth(width);
    int clampedH = clampWindowHeight(height);

    bool changed = (config.window_width != clampedW || config.window_height != clampedH);
    if (!changed) {
        return false;
    }

    config.window_width = clampedW;
    config.window_height = clampedH;

    if (onResize) {
        onResize(clampedW, clampedH);
    } else if (s_globalWindowResizeCallback) {
        s_globalWindowResizeCallback(clampedW, clampedH);
    }

    saveConfig(config);
    return true;
}

bool rebuildFontAtlas(const std::string& fontPath, float fontSize,
                      const std::function<void()>& onGpuTextureReload,
                      ImFontAtlas* atlas) {
    if (!atlas) {
        if (!ImGui::GetCurrentContext()) {
            return false;
        }
        atlas = ImGui::GetIO().Fonts;
    }
    if (!atlas) {
        return false;
    }

    float clampedSize = clampFontSize(fontSize);

    // Save and temporarily unlock atlas if locked by ImGui frame
    bool wasLocked = atlas->Locked;
    atlas->Locked = false;

    atlas->Clear();

    std::string resolved;
    if (!fontPath.empty()) {
        resolved = fontPath;
    } else {
        std::string customDefault = resolveAssetFile("assets/fonts/SpaceGrotesk-VariableFont_wght.ttf");
        if (!customDefault.empty()) {
            resolved = customDefault;
        } else {
            resolved = resolveSystemFont();
        }
    }

    ImFont* font = nullptr;

    if (!resolved.empty() && access(resolved.c_str(), R_OK) == 0) {
        ImFontConfig cfg;
        cfg.Flags |= ImFontFlags_NoLoadError;
        font = atlas->AddFontFromFileTTF(resolved.c_str(), clampedSize, &cfg, fizz::ui::getUiGlyphRanges());
    }

    if (!font) {
        font = atlas->AddFontDefault();
    }

    // Merge Font Awesome 7 Solid and Brands icon fonts into base font
    ImFontConfig iconConfig;
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphMinAdvanceX = clampedSize;

    static const ImWchar icons_fa_ranges[] = {
        ICON_MIN_FA,
        ICON_MAX_FA,
        0
    };
    std::string solidIconPath = resolveAssetFile("assets/icons/Font Awesome 7 Free-Solid-900.otf");
    if (!solidIconPath.empty() && access(solidIconPath.c_str(), R_OK) == 0) {
        atlas->AddFontFromFileTTF(solidIconPath.c_str(), clampedSize, &iconConfig, icons_fa_ranges);
    }

    static const ImWchar icons_fab_ranges[] = {
        ICON_MIN_FAB,
        ICON_MAX_FAB,
        0
    };
    std::string brandIconPath = resolveAssetFile("assets/icons/Font Awesome 7 Brands-Regular-400.otf");
    if (!brandIconPath.empty() && access(brandIconPath.c_str(), R_OK) == 0) {
        atlas->AddFontFromFileTTF(brandIconPath.c_str(), clampedSize, &iconConfig, icons_fab_ranges);
    }

    atlas->Build();
    atlas->Locked = wasLocked;

    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        io.FontDefault = font;
        ImGuiContext& g = *ImGui::GetCurrentContext();
        g.Font = font;
        if (font) {
            g.FontSize = font->LegacySize > 0.0f ? font->LegacySize : clampedSize;
        }
    }

    if (onGpuTextureReload) {
        onGpuTextureReload();
    }

    return true;
}

bool setFontSize(FizzConfig& config, float fontSize,
                 const std::function<void(const std::string&, float)>& onFontRebuild) {
    float clamped = clampFontSize(fontSize);
    if (std::fabs(config.font_size - clamped) < 0.01f) {
        return false;
    }

    config.font_size = clamped;

    if (onFontRebuild) {
        onFontRebuild(config.font_path, clamped);
    } else if (s_globalFontRebuildCallback) {
        s_globalFontRebuildCallback(config.font_path, clamped);
    } else {
        rebuildFontAtlas(config.font_path, clamped);
    }

    saveConfig(config);
    return true;
}

bool setFontPath(FizzConfig& config, const std::string& fontPath,
                 const std::function<void(const std::string&, float)>& onFontRebuild) {
    if (config.font_path == fontPath) {
        return false;
    }

    config.font_path = fontPath;

    if (onFontRebuild) {
        onFontRebuild(config.font_path, config.font_size);
    } else if (s_globalFontRebuildCallback) {
        s_globalFontRebuildCallback(config.font_path, config.font_size);
    } else {
        rebuildFontAtlas(config.font_path, config.font_size);
    }

    saveConfig(config);
    return true;
}

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

bool renderAppearanceTab(FizzConfig& config, const AppearanceCallbacks& callbacks) {
    (void)callbacks;
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

    // 3. Window Geometry section
    ImGui::TextColored(theme::getActiveAccent(), "Window Geometry");
    ImGui::Spacing();

    int winWidth = config.window_width;
    int winHeight = config.window_height;

    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::SliderInt("Window Width##win_w", &winWidth, kMinWidth, kMaxWidth, "%d px")) {
        if (setWindowSize(config, winWidth, config.window_height, callbacks.onWindowResize)) {
            modified = true;
        }
    }

    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::SliderInt("Window Height##win_h", &winHeight, kMinHeight, kMaxHeight, "%d px")) {
        if (setWindowSize(config, config.window_width, winHeight, callbacks.onWindowResize)) {
            modified = true;
        }
    }

    ImGui::Spacing();
    if (ImGui::SmallButton("Default (750 x 480)##preset_default")) {
        if (setWindowSize(config, 750, 480, callbacks.onWindowResize)) {
            modified = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Compact (600 x 400)##preset_compact")) {
        if (setWindowSize(config, 600, 400, callbacks.onWindowResize)) {
            modified = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Large (1000 x 650)##preset_large")) {
        if (setWindowSize(config, 1000, 650, callbacks.onWindowResize)) {
            modified = true;
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Window size clamped between 500x320 and 1600x1200. Window resizes live.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 4. Typography & Font section
    ImGui::TextColored(theme::getActiveAccent(), "Typography & Font");
    ImGui::Spacing();

    float fontSize = config.font_size;
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::SliderFloat("Font Size##font_sz", &fontSize, kMinFontSize, kMaxFontSize, "%.1f px")) {
        if (setFontSize(config, fontSize, callbacks.onFontRebuild)) {
            modified = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset (16px)##reset_font_sz")) {
        if (setFontSize(config, kDefaultFontSize, callbacks.onFontRebuild)) {
            modified = true;
        }
    }

    const auto systemFonts = getDiscoveredSystemFonts();
    std::string currentFontLabel = "Custom Font";
    if (config.font_path.empty()) {
        currentFontLabel = !systemFonts.empty() ? systemFonts[0].familyName : "Space Grotesk (Bundled Default)";
    } else {
        for (const auto& sf : systemFonts) {
            if (sf.path == config.font_path) {
                currentFontLabel = sf.familyName;
                break;
            }
        }
    }

    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("Font Family##font_family", currentFontLabel.c_str())) {
        for (const auto& sf : systemFonts) {
            bool isSelected = (config.font_path == sf.path) ||
                              (config.font_path.empty() && !systemFonts.empty() && sf.path == systemFonts[0].path);
            std::string itemLabel = sf.familyName;
            if (!sf.available && !sf.path.empty()) {
                itemLabel += " (not found)";
            }
            if (ImGui::Selectable(itemLabel.c_str(), isSelected)) {
                if (setFontPath(config, sf.path, callbacks.onFontRebuild)) {
                    modified = true;
                }
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    static char s_fontPathBuf[512] = "";
    if (!ImGui::IsItemActive()) {
        std::strncpy(s_fontPathBuf, config.font_path.c_str(), sizeof(s_fontPathBuf) - 1);
        s_fontPathBuf[sizeof(s_fontPathBuf) - 1] = '\0';
    }

    ImGui::SetNextItemWidth(380.0f);
    if (ImGui::InputTextWithHint("Font Path##custom_font_path", "Path to .ttf or .otf file (empty for default)", s_fontPathBuf, sizeof(s_fontPathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (setFontPath(config, s_fontPathBuf, callbacks.onFontRebuild)) {
            modified = true;
        }
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (setFontPath(config, s_fontPathBuf, callbacks.onFontRebuild)) {
            modified = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##clear_font_path")) {
        s_fontPathBuf[0] = '\0';
        if (setFontPath(config, "", callbacks.onFontRebuild)) {
            modified = true;
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Select standard system fonts or configure custom TTF/OTF path. Dynamically rebuilds font atlas.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 5. Status / Summary
    ImGui::Text("Current Theme: %s", (config.theme == "light" ? "Light" : "Dark"));
    ImGui::SameLine(180.0f);
    ImGui::Text("Current Accent: %s", config.accent.c_str());
    ImGui::Text("Window Size: %d x %d px", config.window_width, config.window_height);
    ImGui::SameLine(240.0f);
    std::string fontDisplay = config.font_path.empty() ? "System Default" : config.font_path;
    ImGui::Text("Font: %s (%.1f px)", fontDisplay.c_str(), config.font_size);
    ImGui::Spacing();
    ImGui::TextDisabled("Changes apply live without restart and automatically persist to fizz.conf.");

    return modified;
}

} // namespace fizz::ui
