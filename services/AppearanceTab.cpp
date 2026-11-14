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

WindowPosition calculateCenteredWindowPos(
    const MonitorWorkArea& workArea,
    int windowWidth,
    int windowHeight,
    const WindowPosition& fallbackPos) {
    if (workArea.width <= 0 || workArea.height <= 0) {
        return fallbackPos;
    }

    int targetX = 0;
    if (windowWidth <= workArea.width) {
        targetX = workArea.x + (workArea.width - windowWidth) / 2;
        targetX = std::clamp(targetX, workArea.x, workArea.x + workArea.width - windowWidth);
    } else {
        targetX = workArea.x;
    }

    int targetY = 0;
    if (windowHeight <= workArea.height) {
        targetY = workArea.y + (workArea.height - windowHeight) / 2;
        targetY = std::clamp(targetY, workArea.y, workArea.y + workArea.height - windowHeight);
    } else {
        targetY = workArea.y;
    }

    return {targetX, targetY};
}

int findActiveMonitorIndex(
    const std::vector<MonitorWorkArea>& monitors,
    int centerX,
    int centerY) {
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        if (m.width > 0 && m.height > 0) {
            if (centerX >= m.x && centerX < m.x + m.width &&
                centerY >= m.y && centerY < m.y + m.height) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

CenteredResizeResult calculateResizeCentering(
    const std::vector<MonitorWorkArea>& monitors,
    const WindowPosition& currentPos,
    int currentWidth,
    int currentHeight,
    int newWidth,
    int newHeight) {
    int clampedW = clampWindowWidth(newWidth);
    int clampedH = clampWindowHeight(newHeight);

    if (monitors.empty()) {
        return {currentPos, -1};
    }

    int curW = currentWidth > 0 ? currentWidth : clampedW;
    int curH = currentHeight > 0 ? currentHeight : clampedH;
    int centerX = currentPos.x + curW / 2;
    int centerY = currentPos.y + curH / 2;

    int activeIdx = findActiveMonitorIndex(monitors, centerX, centerY);
    if (activeIdx < 0 || activeIdx >= static_cast<int>(monitors.size())) {
        activeIdx = 0; // fallback to primary monitor
    }

    const auto& workArea = monitors[activeIdx];
    WindowPosition target = calculateCenteredWindowPos(workArea, clampedW, clampedH, currentPos);

    return {target, activeIdx};
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

const std::vector<WindowPresetInfo>& getWindowSizePresets() {
    static const std::vector<WindowPresetInfo> s_presets = {
        {WindowSizePreset::Default, "default", "Default (750 x 480)", kDefaultWindowWidth, kDefaultWindowHeight},
        {WindowSizePreset::Compact, "compact", "Compact (600 x 400)", kCompactWindowWidth, kCompactWindowHeight},
        {WindowSizePreset::Large,   "large",   "Large (1000 x 650)",  kLargeWindowWidth,   kLargeWindowHeight}
    };
    return s_presets;
}

bool applyWindowSizePreset(FizzConfig& config, WindowSizePreset preset,
                           const std::function<void(int, int)>& onResize) {
    switch (preset) {
        case WindowSizePreset::Default:
            return setWindowSize(config, kDefaultWindowWidth, kDefaultWindowHeight, onResize);
        case WindowSizePreset::Compact:
            return setWindowSize(config, kCompactWindowWidth, kCompactWindowHeight, onResize);
        case WindowSizePreset::Large:
            return setWindowSize(config, kLargeWindowWidth, kLargeWindowHeight, onResize);
    }
    return false;
}

bool applyWindowSizePreset(FizzConfig& config, std::string_view presetId,
                           const std::function<void(int, int)>& onResize) {
    std::string lower(presetId);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "compact") {
        return applyWindowSizePreset(config, WindowSizePreset::Compact, onResize);
    } else if (lower == "large") {
        return applyWindowSizePreset(config, WindowSizePreset::Large, onResize);
    } else {
        return applyWindowSizePreset(config, WindowSizePreset::Default, onResize);
    }
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
        {"crimson", "Crimson", theme::Accents::kCrimson},
        {"rose", "Rose / Coral", theme::Accents::kRose},
        {"indigo", "Electric Indigo", theme::Accents::kIndigo},
        {"cyan", "Nord Cyan", theme::Accents::kCyan},
        {"sunset", "Sunset Orange", theme::Accents::kSunset},
        {"lime", "Neon Lime", theme::Accents::kLime}
    };
    return s_accents;
}

static std::string normalizeAccentId(std::string_view accentId) {
    return theme::normalizeAccent(accentId);
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

    const ImVec2 swatchSize(kSwatchSize, kSwatchSize);

    for (size_t i = 0; i < accents.size(); ++i) {
        const auto& choice = accents[i];
        bool isSelected = (activeNorm == choice.id);

        ImGui::PushID(static_cast<int>(i));

        if (i > 0) {
            ImGui::SameLine(0.0f, kSwatchSpacing);
        }

        std::string swatchBtnId = "##swatch_" + choice.id;
        if (ImGui::InvisibleButton(swatchBtnId.c_str(), swatchSize)) {
            if (!isSelected) {
                selectAccent(config, choice.id);
                modified = true;
            }
        }

        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();

        bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            ImGui::SetTooltip("%s", choice.displayName.c_str());
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Render filled color swatch
        dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(choice.color), kSwatchRounding);

        // Render active contrast ring / border or hover highlight
        if (isSelected) {
            dl->AddRect(pMin, pMax, ImGui::GetColorU32(ImGuiCol_Text), kSwatchRounding, 0, 2.0f);
        } else if (hovered) {
            ImU32 hoverCol = (config.theme == "light")
                ? IM_COL32(0, 0, 0, 80)
                : IM_COL32(255, 255, 255, 100);
            dl->AddRect(pMin, pMax, hoverCol, kSwatchRounding, 0, 1.5f);
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
        if (applyWindowSizePreset(config, WindowSizePreset::Default, callbacks.onWindowResize)) {
            modified = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Compact (600 x 400)##preset_compact")) {
        if (applyWindowSizePreset(config, WindowSizePreset::Compact, callbacks.onWindowResize)) {
            modified = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Large (1000 x 650)##preset_large")) {
        if (applyWindowSizePreset(config, WindowSizePreset::Large, callbacks.onWindowResize)) {
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

    ImGui::Spacing();
    ImGui::TextDisabled("Select bundled or system font family. Dynamically rebuilds font atlas.");

    return modified;
}

} // namespace fizz::ui
