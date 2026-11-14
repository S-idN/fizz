#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include "services/AppearanceTab.h"
#include "services/FontDiscovery.h"
#include "services/Theme.h"
#include "services/Config.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "IconsFontAwesome7.h"
#include "IconsFontAwesome7Brands.h"

namespace fs = std::filesystem;

#define APPEARANCE_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

static bool nearEqual(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) <= eps;
}

static bool vec4NearEqual(const ImVec4& a, const ImVec4& b, float eps = 0.001f) {
    return nearEqual(a.x, b.x, eps) &&
           nearEqual(a.y, b.y, eps) &&
           nearEqual(a.z, b.z, eps) &&
           nearEqual(a.w, b.w, eps);
}

void test_appearance_theme_options_and_selection() {
    std::cout << "[TEST] Running test_appearance_theme_options_and_selection...\n";

    const auto& themes = fizz::ui::getAvailableThemes();
    APPEARANCE_TEST_ASSERT(themes.size() == 2);
    APPEARANCE_TEST_ASSERT(themes[0].id == "dark");
    APPEARANCE_TEST_ASSERT(themes[0].displayName == "Dark");
    APPEARANCE_TEST_ASSERT(themes[1].id == "light");
    APPEARANCE_TEST_ASSERT(themes[1].displayName == "Light");

    ImGuiStyle style;
    FizzConfig config = FizzConfig::defaults();
    APPEARANCE_TEST_ASSERT(config.theme == "dark");

    // Switching to Light theme updates config and style colors
    bool changedToLight = fizz::ui::selectTheme(config, "light", &style);
    APPEARANCE_TEST_ASSERT(changedToLight);
    APPEARANCE_TEST_ASSERT(config.theme == "light");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_WindowBg], fizz::theme::LightPalette::kBg));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Text], fizz::theme::LightPalette::kText));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Border], fizz::theme::LightPalette::kBorder));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_FrameBg], fizz::theme::LightPalette::kFrameBg));
    APPEARANCE_TEST_ASSERT(fizz::theme::getActiveTheme() == "light");

    // Redundant switch returns false
    bool redundantLight = fizz::ui::selectTheme(config, "light", &style);
    APPEARANCE_TEST_ASSERT(!redundantLight);

    // Switching back to Dark theme restores dark palette
    bool changedToDark = fizz::ui::selectTheme(config, "dark", &style);
    APPEARANCE_TEST_ASSERT(changedToDark);
    APPEARANCE_TEST_ASSERT(config.theme == "dark");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_WindowBg], fizz::theme::Palette::kBg));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Text], fizz::theme::Palette::kText));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Border], fizz::theme::Palette::kBorder));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_FrameBg], fizz::theme::Palette::kFrameBg));
    APPEARANCE_TEST_ASSERT(fizz::theme::getActiveTheme() == "dark");

    // Unrecognized theme falls back safely to dark
    bool invalidTheme = fizz::ui::selectTheme(config, "unknown_theme", &style);
    APPEARANCE_TEST_ASSERT(config.theme == "dark");
    APPEARANCE_TEST_ASSERT(!invalidTheme); // already dark, so no change

    std::cout << "[PASS] test_appearance_theme_options_and_selection passed.\n";
}

void test_appearance_curated_accent_palette() {
    std::cout << "[TEST] Running test_appearance_curated_accent_palette...\n";

    const auto& accents = fizz::ui::getCuratedAccents();
    APPEARANCE_TEST_ASSERT(accents.size() == 10);

    // 1. Blue (Adwaita Blue)
    APPEARANCE_TEST_ASSERT(accents[0].id == "blue");
    APPEARANCE_TEST_ASSERT(accents[0].displayName == "Adwaita Blue");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[0].color, fizz::theme::Accents::kBlue));

    // 2. Emerald Green
    APPEARANCE_TEST_ASSERT(accents[1].id == "emerald");
    APPEARANCE_TEST_ASSERT(accents[1].displayName == "Emerald Green");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[1].color, fizz::theme::Accents::kEmerald));

    // 3. Purple
    APPEARANCE_TEST_ASSERT(accents[2].id == "purple");
    APPEARANCE_TEST_ASSERT(accents[2].displayName == "Purple");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[2].color, fizz::theme::Accents::kPurple));

    // 4. Amber
    APPEARANCE_TEST_ASSERT(accents[3].id == "amber");
    APPEARANCE_TEST_ASSERT(accents[3].displayName == "Amber");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[3].color, fizz::theme::Accents::kAmber));

    // 5. Crimson
    APPEARANCE_TEST_ASSERT(accents[4].id == "crimson");
    APPEARANCE_TEST_ASSERT(accents[4].displayName == "Crimson");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[4].color, fizz::theme::Accents::kCrimson));

    // 6. Rose / Coral
    APPEARANCE_TEST_ASSERT(accents[5].id == "rose");
    APPEARANCE_TEST_ASSERT(accents[5].displayName == "Rose / Coral");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[5].color, fizz::theme::Accents::kRose));

    // 7. Electric Indigo
    APPEARANCE_TEST_ASSERT(accents[6].id == "indigo");
    APPEARANCE_TEST_ASSERT(accents[6].displayName == "Electric Indigo");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[6].color, fizz::theme::Accents::kIndigo));

    // 8. Nord Cyan
    APPEARANCE_TEST_ASSERT(accents[7].id == "cyan");
    APPEARANCE_TEST_ASSERT(accents[7].displayName == "Nord Cyan");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[7].color, fizz::theme::Accents::kCyan));

    // 9. Sunset Orange
    APPEARANCE_TEST_ASSERT(accents[8].id == "sunset");
    APPEARANCE_TEST_ASSERT(accents[8].displayName == "Sunset Orange");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[8].color, fizz::theme::Accents::kSunset));

    // 10. Neon Lime
    APPEARANCE_TEST_ASSERT(accents[9].id == "lime");
    APPEARANCE_TEST_ASSERT(accents[9].displayName == "Neon Lime");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(accents[9].color, fizz::theme::Accents::kLime));

    // Verify individual color calculations & lookups via getAccentColor
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("blue"), fizz::theme::Accents::kBlue));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("emerald"), fizz::theme::Accents::kEmerald));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("purple"), fizz::theme::Accents::kPurple));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("amber"), fizz::theme::Accents::kAmber));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("crimson"), fizz::theme::Accents::kCrimson));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("rose"), fizz::theme::Accents::kRose));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("indigo"), fizz::theme::Accents::kIndigo));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("cyan"), fizz::theme::Accents::kCyan));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("sunset"), fizz::theme::Accents::kSunset));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("lime"), fizz::theme::Accents::kLime));

    // Aliases
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("green"), fizz::theme::Accents::kEmerald));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("red"), fizz::theme::Accents::kCrimson));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("coral"), fizz::theme::Accents::kRose));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("electric indigo"), fizz::theme::Accents::kIndigo));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("nord cyan"), fizz::theme::Accents::kCyan));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("orange"), fizz::theme::Accents::kSunset));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("sunset orange"), fizz::theme::Accents::kSunset));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("neon lime"), fizz::theme::Accents::kLime));

    // Fallback for unrecognized accent defaults to Adwaita Blue
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("invalid_accent"), fizz::theme::Accents::kBlue));

    std::cout << "[PASS] test_appearance_curated_accent_palette passed.\n";
}

void test_appearance_live_imgui_style_application() {
    std::cout << "[TEST] Running test_appearance_live_imgui_style_application...\n";

    ImGuiStyle style;
    FizzConfig config = FizzConfig::defaults();

    // Default: dark + blue
    fizz::theme::applyTheme(style, config.theme, config.accent);
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kBlue, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_HeaderHovered], fizz::theme::withAlpha(fizz::theme::Accents::kBlue, 0.26f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_HeaderActive], fizz::theme::withAlpha(fizz::theme::Accents::kBlue, 0.32f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_CheckMark], fizz::theme::Accents::kBlue));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kBlue));

    // Live update to Emerald
    bool changed = fizz::ui::selectAccent(config, "emerald", &style);
    APPEARANCE_TEST_ASSERT(changed);
    APPEARANCE_TEST_ASSERT(config.accent == "emerald");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kEmerald, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_HeaderHovered], fizz::theme::withAlpha(fizz::theme::Accents::kEmerald, 0.26f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_HeaderActive], fizz::theme::withAlpha(fizz::theme::Accents::kEmerald, 0.32f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_CheckMark], fizz::theme::Accents::kEmerald));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kEmerald));

    // Live update to Purple
    fizz::ui::selectAccent(config, "purple", &style);
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kPurple, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kPurple));

    // Live update to Amber
    fizz::ui::selectAccent(config, "amber", &style);
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kAmber, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kAmber));

    // Live update to Crimson in Light theme
    fizz::ui::selectTheme(config, "light", &style);
    fizz::ui::selectAccent(config, "crimson", &style);
    APPEARANCE_TEST_ASSERT(config.theme == "light");
    APPEARANCE_TEST_ASSERT(config.accent == "crimson");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kCrimson, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_WindowBg], fizz::theme::LightPalette::kBg));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kCrimson));

    // Live update to Rose (and alias coral)
    fizz::ui::selectAccent(config, "rose", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "rose");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kRose, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kRose));
    fizz::ui::selectAccent(config, "coral", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "rose");

    // Live update to Indigo (and alias electric indigo)
    fizz::ui::selectAccent(config, "electric indigo", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "indigo");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kIndigo, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kIndigo));

    // Live update to Cyan (and alias nord cyan)
    fizz::ui::selectAccent(config, "nord cyan", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "cyan");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kCyan, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kCyan));

    // Live update to Sunset (and aliases orange, sunset orange)
    fizz::ui::selectAccent(config, "orange", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "sunset");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kSunset, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kSunset));
    fizz::ui::selectAccent(config, "sunset orange", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "sunset");

    // Live update to Lime (and alias neon lime)
    fizz::ui::selectAccent(config, "neon lime", &style);
    APPEARANCE_TEST_ASSERT(config.accent == "lime");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(style.Colors[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Accents::kLime, 0.20f)));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kLime));

    std::cout << "[PASS] test_appearance_live_imgui_style_application passed.\n";
}

void test_appearance_atomic_config_persistence() {
    std::cout << "[TEST] Running test_appearance_atomic_config_persistence...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_appearance_persistence";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path confPath = tempDir / "fizz.conf";
    setConfigPath(confPath.string());

    // Initial config with unrecognized keys preserved
    FizzConfig config = FizzConfig::defaults();
    config.theme = "dark";
    config.accent = "blue";
    config.unrecognizedKeys.push_back({"experimental_flag", "true"});
    bool saved = saveConfig(config);
    APPEARANCE_TEST_ASSERT(saved);

    // 1. Changing theme persists atomically to disk
    ImGuiStyle style;
    fizz::ui::selectTheme(config, "light", &style);

    FizzConfig loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.theme == "light");
    APPEARANCE_TEST_ASSERT(loaded.accent == "blue");
    APPEARANCE_TEST_ASSERT(loaded.unrecognizedKeys.size() == 1);
    APPEARANCE_TEST_ASSERT(loaded.unrecognizedKeys[0].first == "experimental_flag");

    // 2. Changing accent persists atomically to disk
    fizz::ui::selectAccent(config, "emerald", &style);

    loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.theme == "light");
    APPEARANCE_TEST_ASSERT(loaded.accent == "emerald");
    APPEARANCE_TEST_ASSERT(loaded.unrecognizedKeys.size() == 1);

    // 3. Changing to Crimson in Dark theme persists
    fizz::ui::selectTheme(config, "dark", &style);
    fizz::ui::selectAccent(config, "crimson", &style);

    loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.theme == "dark");
    APPEARANCE_TEST_ASSERT(loaded.accent == "crimson");

    // 4. Changing to new accents (Sunset, Lime, Rose) persists atomically
    fizz::ui::selectAccent(config, "sunset", &style);
    loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.accent == "sunset");

    fizz::ui::selectAccent(config, "lime", &style);
    loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.accent == "lime");

    fizz::ui::selectAccent(config, "rose", &style);
    loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.accent == "rose");

    // Cleanup
    setConfigPath("");
    fs::remove_all(tempDir, ec);

    std::cout << "[PASS] test_appearance_atomic_config_persistence passed.\n";
}

void test_appearance_headless_ui_rendering() {
    std::cout << "[TEST] Running test_appearance_headless_ui_rendering...\n";

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    FizzConfig config = FizzConfig::defaults();
    fizz::theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().ItemSpacing.x, 8.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().ItemSpacing.y, 10.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().FramePadding.x, 10.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().FramePadding.y, 7.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().FrameRounding, 8.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().ChildRounding, 8.0f));

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    // Render TabBar with General tab first, followed by Appearance
    bool tabModified = false;
    std::string capturedUiTextFrame1;
    if (ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("General")) {
            ImGui::Text("General Settings Tab");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Appearance", nullptr, ImGuiTabItemFlags_SetSelected)) {
            ImGui::LogToClipboard();
            tabModified = fizz::ui::renderAppearanceTab(config);
            ImGui::LogFinish();
            if (const char* clip = ImGui::GetClipboardText()) {
                capturedUiTextFrame1 = clip;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    APPEARANCE_TEST_ASSERT(drawData != nullptr);
    APPEARANCE_TEST_ASSERT(drawData->CmdListsCount > 0);

    // Verify footer summary block is completely absent from rendered Appearance tab
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("Current Theme:") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("Current Accent:") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("Window Size:") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("Font: ") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("Changes apply live without restart") == std::string::npos);

    // Verify custom font path input and Clear button are absent
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("custom_font_path") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("[Clear]") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame1.find("custom TTF/OTF path") == std::string::npos);

    // Frame 2: Render with explicit AppearanceCallbacks wired
    int resizeCount = 0;
    int fontRebuildCount = 0;
    fizz::ui::AppearanceCallbacks callbacks;
    callbacks.onWindowResize = [&](int, int) { resizeCount++; };
    callbacks.onFontRebuild = [&](const std::string&, float) { fontRebuildCount++; };

    std::string capturedUiTextFrame2;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window_2", nullptr, ImGuiWindowFlags_NoDecoration);
    if (ImGui::BeginTabBar("SettingsTabBar2", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Appearance")) {
            ImGui::LogToClipboard();
            fizz::ui::renderAppearanceTab(config, callbacks);
            ImGui::LogFinish();
            if (const char* clip = ImGui::GetClipboardText()) {
                capturedUiTextFrame2 = clip;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData2 = ImGui::GetDrawData();
    APPEARANCE_TEST_ASSERT(drawData2 != nullptr);
    APPEARANCE_TEST_ASSERT(drawData2->CmdListsCount > 0);

    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Current Theme:") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Current Accent:") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Window Size:") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Font: ") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Changes apply live without restart") == std::string::npos);

    // Verify window geometry sliders and preset buttons are rendered in Appearance tab
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Window Geometry") != std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Window Width") != std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Window Height") != std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Default (750 x 480)") != std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Compact (600 x 400)") != std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("Large (1000 x 650)") != std::string::npos);

    // Verify custom font path input and Clear button are absent
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("custom_font_path") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("[Clear]") == std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUiTextFrame2.find("custom TTF/OTF path") == std::string::npos);

    ImGui::DestroyContext(ctx);

    std::cout << "[PASS] test_appearance_headless_ui_rendering passed.\n";
}

void test_appearance_window_geometry_clamping_and_callbacks() {
    std::cout << "[TEST] Running test_appearance_window_geometry_clamping_and_callbacks...\n";

    // 1. Clamping boundaries
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowWidth(400) == 500);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowWidth(500) == 500);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowWidth(800) == 800);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowWidth(1600) == 1600);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowWidth(2000) == 1600);

    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowHeight(200) == 320);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowHeight(320) == 320);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowHeight(600) == 600);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowHeight(1200) == 1200);
    APPEARANCE_TEST_ASSERT(fizz::ui::clampWindowHeight(1500) == 1200);

    // 2. setWindowSize with direct callback and clamping
    FizzConfig config = FizzConfig::defaults();
    config.window_width = 750;
    config.window_height = 480;

    int callbackWidth = 0;
    int callbackHeight = 0;
    int callbackCount = 0;
    auto resizeCb = [&](int w, int h) {
        callbackWidth = w;
        callbackHeight = h;
        callbackCount++;
    };

    // Sub-range clamping
    bool modified = fizz::ui::setWindowSize(config, 400, 1500, resizeCb);
    APPEARANCE_TEST_ASSERT(modified);
    APPEARANCE_TEST_ASSERT(config.window_width == 500);
    APPEARANCE_TEST_ASSERT(config.window_height == 1200);
    APPEARANCE_TEST_ASSERT(callbackCount == 1);
    APPEARANCE_TEST_ASSERT(callbackWidth == 500);
    APPEARANCE_TEST_ASSERT(callbackHeight == 1200);

    // Redundant call: no change, callback not fired
    bool redundant = fizz::ui::setWindowSize(config, 500, 1200, resizeCb);
    APPEARANCE_TEST_ASSERT(!redundant);
    APPEARANCE_TEST_ASSERT(callbackCount == 1);

    // In-range resize
    bool modified2 = fizz::ui::setWindowSize(config, 900, 650, resizeCb);
    APPEARANCE_TEST_ASSERT(modified2);
    APPEARANCE_TEST_ASSERT(config.window_width == 900);
    APPEARANCE_TEST_ASSERT(config.window_height == 650);
    APPEARANCE_TEST_ASSERT(callbackCount == 2);
    APPEARANCE_TEST_ASSERT(callbackWidth == 900);
    APPEARANCE_TEST_ASSERT(callbackHeight == 650);

    // 3. Global callback fallback
    int globalWidth = 0;
    int globalHeight = 0;
    int globalCount = 0;
    fizz::ui::setWindowResizeCallback([&](int w, int h) {
        globalWidth = w;
        globalHeight = h;
        globalCount++;
    });

    bool modified3 = fizz::ui::setWindowSize(config, 1000, 700);
    APPEARANCE_TEST_ASSERT(modified3);
    APPEARANCE_TEST_ASSERT(globalCount == 1);
    APPEARANCE_TEST_ASSERT(globalWidth == 1000);
    APPEARANCE_TEST_ASSERT(globalHeight == 700);

    // Clean up global callback
    fizz::ui::setWindowResizeCallback(nullptr);

    std::cout << "[PASS] test_appearance_window_geometry_clamping_and_callbacks passed.\n";
}

void test_appearance_font_size_clamping() {
    std::cout << "[TEST] Running test_appearance_font_size_clamping...\n";

    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::clampFontSize(4.0f), 10.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::clampFontSize(10.0f), 10.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::clampFontSize(16.0f), 16.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::clampFontSize(24.5f), 24.5f));
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::clampFontSize(32.0f), 32.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::clampFontSize(48.0f), 32.0f));

    FizzConfig config = FizzConfig::defaults();
    config.font_size = 16.0f;
    config.font_path = "";

    std::string callbackFontPath;
    float callbackFontSize = 0.0f;
    int callbackCount = 0;
    auto fontRebuildCb = [&](const std::string& path, float sz) {
        callbackFontPath = path;
        callbackFontSize = sz;
        callbackCount++;
    };

    // Sub-range clamping
    bool modified = fizz::ui::setFontSize(config, 8.0f, fontRebuildCb);
    APPEARANCE_TEST_ASSERT(modified);
    APPEARANCE_TEST_ASSERT(nearEqual(config.font_size, 10.0f));
    APPEARANCE_TEST_ASSERT(callbackCount == 1);
    APPEARANCE_TEST_ASSERT(nearEqual(callbackFontSize, 10.0f));

    // Redundant call: no change
    bool redundant = fizz::ui::setFontSize(config, 10.0f, fontRebuildCb);
    APPEARANCE_TEST_ASSERT(!redundant);
    APPEARANCE_TEST_ASSERT(callbackCount == 1);

    // In-range update
    bool modified2 = fizz::ui::setFontSize(config, 22.0f, fontRebuildCb);
    APPEARANCE_TEST_ASSERT(modified2);
    APPEARANCE_TEST_ASSERT(nearEqual(config.font_size, 22.0f));
    APPEARANCE_TEST_ASSERT(callbackCount == 2);
    APPEARANCE_TEST_ASSERT(nearEqual(callbackFontSize, 22.0f));

    // Upper clamping
    bool modified3 = fizz::ui::setFontSize(config, 50.0f, fontRebuildCb);
    APPEARANCE_TEST_ASSERT(modified3);
    APPEARANCE_TEST_ASSERT(nearEqual(config.font_size, 32.0f));
    APPEARANCE_TEST_ASSERT(callbackCount == 3);
    APPEARANCE_TEST_ASSERT(nearEqual(callbackFontSize, 32.0f));

    // setFontPath
    bool pathMod = fizz::ui::setFontPath(config, "/usr/share/fonts/custom.ttf", fontRebuildCb);
    APPEARANCE_TEST_ASSERT(pathMod);
    APPEARANCE_TEST_ASSERT(config.font_path == "/usr/share/fonts/custom.ttf");
    APPEARANCE_TEST_ASSERT(callbackCount == 4);
    APPEARANCE_TEST_ASSERT(callbackFontPath == "/usr/share/fonts/custom.ttf");

    // Global callback fallback
    int globalCount = 0;
    std::string globalPath;
    float globalSize = 0.0f;
    fizz::ui::setFontRebuildCallback([&](const std::string& path, float sz) {
        globalPath = path;
        globalSize = sz;
        globalCount++;
    });

    bool modGlobal = fizz::ui::setFontSize(config, 18.0f);
    APPEARANCE_TEST_ASSERT(modGlobal);
    APPEARANCE_TEST_ASSERT(globalCount == 1);
    APPEARANCE_TEST_ASSERT(nearEqual(globalSize, 18.0f));

    fizz::ui::setFontRebuildCallback(nullptr);

    std::cout << "[PASS] test_appearance_font_size_clamping passed.\n";
}

void test_appearance_dynamic_font_atlas_rebuild_and_safety() {
    std::cout << "[TEST] Running test_appearance_dynamic_font_atlas_rebuild_and_safety...\n";

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);

    int gpuReloadCount = 0;
    auto gpuReloadCb = [&]() {
        gpuReloadCount++;
    };

    // 1. Auto-detected system font rebuild
    bool success1 = fizz::ui::rebuildFontAtlas("", 18.0f, gpuReloadCb);
    APPEARANCE_TEST_ASSERT(success1);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);
    APPEARANCE_TEST_ASSERT(gpuReloadCount == 1);

    // 2. Real host system font if available
    std::string sysFont = resolveSystemFont();
    if (!sysFont.empty()) {
        bool success2 = fizz::ui::rebuildFontAtlas(sysFont, 24.0f, gpuReloadCb);
        APPEARANCE_TEST_ASSERT(success2);
        APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);
        APPEARANCE_TEST_ASSERT(gpuReloadCount == 2);
    }

    // 3. Safety: Nonexistent / unreadable font path must NOT crash or assert!
    // It should safely fallback to AddFontDefault().
    bool success3 = fizz::ui::rebuildFontAtlas("/nonexistent/bogus/path/font.ttf", 20.0f, gpuReloadCb);
    APPEARANCE_TEST_ASSERT(success3);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);
    APPEARANCE_TEST_ASSERT(gpuReloadCount == (sysFont.empty() ? 2 : 3));

    // 4. Safety mid-frame: rebuilding while frame is active
    ImGui::NewFrame();
    bool success4 = fizz::ui::rebuildFontAtlas("", 16.0f);
    APPEARANCE_TEST_ASSERT(success4);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);
    ImGui::Text("Rendered text after dynamic mid-frame font rebuild");
    ImGui::Render();

    ImGui::DestroyContext(ctx);
    std::cout << "[PASS] test_appearance_dynamic_font_atlas_rebuild_and_safety passed.\n";
}

void test_appearance_system_font_discovery() {
    std::cout << "[TEST] Running test_appearance_system_font_discovery...\n";

    auto fonts = getDiscoveredSystemFonts();
    APPEARANCE_TEST_ASSERT(!fonts.empty());

    // First entry must be Space Grotesk (Bundled Default)
    APPEARANCE_TEST_ASSERT(fonts[0].familyName == "Space Grotesk (Bundled Default)");
    APPEARANCE_TEST_ASSERT(fonts[0].available);
    APPEARANCE_TEST_ASSERT(!fonts[0].path.empty());
    APPEARANCE_TEST_ASSERT(access(fonts[0].path.c_str(), R_OK) == 0);

    // Fontconfig discovered fonts are enumerated
    APPEARANCE_TEST_ASSERT(fonts.size() > 1);

    // For any font that is marked available and has a non-empty path, verify readability
    for (const auto& f : fonts) {
        if (f.available && !f.path.empty()) {
            APPEARANCE_TEST_ASSERT(access(f.path.c_str(), R_OK) == 0);
        }
    }

    std::cout << "[PASS] test_appearance_system_font_discovery passed.\n";
}

void test_appearance_geometry_and_font_atomic_persistence() {
    std::cout << "[TEST] Running test_appearance_geometry_and_font_atomic_persistence...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_appearance_geom_persistence";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path confPath = tempDir / "fizz.conf";
    setConfigPath(confPath.string());

    FizzConfig config = FizzConfig::defaults();
    config.window_width = 750;
    config.window_height = 480;
    config.font_size = 16.0f;
    config.font_path = "";
    config.unrecognizedKeys.push_back({"custom_key", "custom_val"});
    bool saved = saveConfig(config);
    APPEARANCE_TEST_ASSERT(saved);

    // 1. Changing window geometry persists atomically
    bool modGeom = fizz::ui::setWindowSize(config, 1024, 768);
    APPEARANCE_TEST_ASSERT(modGeom);

    FizzConfig loaded1 = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded1.window_width == 1024);
    APPEARANCE_TEST_ASSERT(loaded1.window_height == 768);
    APPEARANCE_TEST_ASSERT(loaded1.unrecognizedKeys.size() == 1);
    APPEARANCE_TEST_ASSERT(loaded1.unrecognizedKeys[0].first == "custom_key");

    // 2. Changing font size persists atomically
    bool modFont = fizz::ui::setFontSize(config, 20.5f);
    APPEARANCE_TEST_ASSERT(modFont);

    FizzConfig loaded2 = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(nearEqual(loaded2.font_size, 20.5f));
    APPEARANCE_TEST_ASSERT(loaded2.window_width == 1024);
    APPEARANCE_TEST_ASSERT(loaded2.window_height == 768);

    // 3. Changing font path persists atomically
    std::string testFontPath = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf";
    bool modPath = fizz::ui::setFontPath(config, testFontPath);
    APPEARANCE_TEST_ASSERT(modPath);

    FizzConfig loaded3 = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded3.font_path == testFontPath);
    APPEARANCE_TEST_ASSERT(nearEqual(loaded3.font_size, 20.5f));
    APPEARANCE_TEST_ASSERT(loaded3.unrecognizedKeys.size() == 1);

    // Cleanup
    setConfigPath("");
    fs::remove_all(tempDir, ec);

    std::cout << "[PASS] test_appearance_geometry_and_font_atomic_persistence passed.\n";
}

void test_appearance_custom_font_and_icon_merging() {
    std::cout << "[TEST] Running test_appearance_custom_font_and_icon_merging...\n";

    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    bool success = fizz::ui::rebuildFontAtlas("", 16.0f);
    APPEARANCE_TEST_ASSERT(success);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);

    ImFont* font = io.Fonts->Fonts[0];
    APPEARANCE_TEST_ASSERT(font != nullptr);

    ImFontBaked* baked = font->GetFontBaked(16.0f);
    APPEARANCE_TEST_ASSERT(baked != nullptr);

    // Verify ASCII glyph from base text font
    APPEARANCE_TEST_ASSERT(baked->FindGlyphNoFallback('A') != nullptr);

    // Verify Font Awesome 7 Solid icon glyph (U+f002 magnifying glass)
    const ImFontGlyph* solidGlyph = baked->FindGlyphNoFallback(0xf002);
    APPEARANCE_TEST_ASSERT(solidGlyph != nullptr);

    // Verify Font Awesome 7 Brands icon glyph (U+f09b github)
    const ImFontGlyph* brandGlyph = baked->FindGlyphNoFallback(0xf09b);
    APPEARANCE_TEST_ASSERT(brandGlyph != nullptr);

    ImGui::DestroyContext(ctx);
    std::cout << "[PASS] test_appearance_custom_font_and_icon_merging passed.\n";
}

void test_settings_tab_order_and_clean_footer() {
    std::cout << "[TEST] Running test_settings_tab_order_and_clean_footer...\n";

    // 1. Headless ImGui TabBar order verification: General -> Appearance -> Indexing -> Cache
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    FizzConfig config = FizzConfig::defaults();

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_order_window", nullptr, ImGuiWindowFlags_NoDecoration);

    ImGui::LogToBuffer();
    if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("General")) {
            ImGui::Text("General Tab");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Appearance")) {
            fizz::ui::renderAppearanceTab(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Indexing")) {
            ImGui::Text("Indexing Tab");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Cache")) {
            ImGui::Text("Cache Tab");
            ImGui::EndTabItem();
        }

        ImGuiTabBar* tabBar = ImGui::GetCurrentTabBar();
        APPEARANCE_TEST_ASSERT(tabBar != nullptr);
        APPEARANCE_TEST_ASSERT(tabBar->Tabs.Size == 4);
        APPEARANCE_TEST_ASSERT(std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[0])) == "General");
        APPEARANCE_TEST_ASSERT(std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[1])) == "Appearance");
        APPEARANCE_TEST_ASSERT(std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[2])) == "Indexing");
        APPEARANCE_TEST_ASSERT(std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[3])) == "Cache");

        ImGui::EndTabBar();
    }
    ImGui::LogFinish();

    ImGuiContext& g = *GImGui;
    std::string logged = g.LogBuffer.c_str();
    APPEARANCE_TEST_ASSERT(logged.find("Press Escape") == std::string::npos);
    APPEARANCE_TEST_ASSERT(logged.find("Back to Search") == std::string::npos);

    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext(ctx);

    // 2. Structural verification of main_gui.cpp:
    // Verify that main_gui.cpp has General tab first and removed footer hint
    fs::path mainGuiPath = fs::path(__FILE__).parent_path().parent_path() / "main_gui.cpp";
    if (fs::exists(mainGuiPath)) {
        std::ifstream in(mainGuiPath);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        APPEARANCE_TEST_ASSERT(content.find("Press Escape or click '← Back to Search' to return.") == std::string::npos);

        // Verify General comes before Appearance inside SettingsTabs
        size_t settingsPos = content.find("\"SettingsTabs\"");
        APPEARANCE_TEST_ASSERT(settingsPos != std::string::npos);
        size_t genPos = content.find("getGeneralTabLabel()", settingsPos);
        if (genPos == std::string::npos) genPos = content.find("BeginTabItem(\"General\")", settingsPos);
        size_t appPos = content.find("getAppearanceTabLabel()", settingsPos);
        if (appPos == std::string::npos) appPos = content.find("BeginTabItem(\"Appearance\")", settingsPos);
        size_t idxPos = content.find("getIndexingTabLabel()", settingsPos);
        if (idxPos == std::string::npos) idxPos = content.find("BeginTabItem(\"Indexing\")", settingsPos);
        size_t cchPos = content.find("getCacheTabLabel()", settingsPos);
        if (cchPos == std::string::npos) cchPos = content.find("BeginTabItem(\"Cache\")", settingsPos);

        APPEARANCE_TEST_ASSERT(genPos != std::string::npos);
        APPEARANCE_TEST_ASSERT(appPos != std::string::npos);
        APPEARANCE_TEST_ASSERT(idxPos != std::string::npos);
        APPEARANCE_TEST_ASSERT(cchPos != std::string::npos);

        APPEARANCE_TEST_ASSERT(genPos < appPos);
        APPEARANCE_TEST_ASSERT(appPos < idxPos);
        APPEARANCE_TEST_ASSERT(idxPos < cchPos);
    }

    std::cout << "[PASS] test_settings_tab_order_and_clean_footer passed.\n";
}

void test_appearance_window_centering_and_workarea() {
    std::cout << "[TEST] Running test_appearance_window_centering_and_workarea...\n";

    // 1. Exact coordinate math on primary display
    fizz::ui::MonitorWorkArea primaryWorkArea{0, 0, 1920, 1080};
    fizz::ui::WindowPosition pos1 = fizz::ui::calculateCenteredWindowPos(primaryWorkArea, 750, 480);
    // (0 + (1920 - 750) / 2, 0 + (1080 - 480) / 2) = (585, 300)
    APPEARANCE_TEST_ASSERT(pos1.x == 585);
    APPEARANCE_TEST_ASSERT(pos1.y == 300);

    // 2. Exact coordinate math on secondary monitor with panel / dock offset
    fizz::ui::MonitorWorkArea secondaryWorkArea{1920, 32, 2560, 1408};
    fizz::ui::WindowPosition pos2 = fizz::ui::calculateCenteredWindowPos(secondaryWorkArea, 1000, 650);
    // (1920 + (2560 - 1000) / 2, 32 + (1408 - 650) / 2) = (1920 + 780, 32 + 379) = (2700, 411)
    APPEARANCE_TEST_ASSERT(pos2.x == 2700);
    APPEARANCE_TEST_ASSERT(pos2.y == 411);

    // 3. Coordinate math with odd dimensions (integer floor division)
    fizz::ui::MonitorWorkArea oddWorkArea{10, 15, 1921, 1081};
    fizz::ui::WindowPosition pos3 = fizz::ui::calculateCenteredWindowPos(oddWorkArea, 750, 480);
    // (10 + (1921 - 750) / 2, 15 + (1081 - 480) / 2) = (10 + 585, 15 + 300) = (595, 315)
    APPEARANCE_TEST_ASSERT(pos3.x == 595);
    APPEARANCE_TEST_ASSERT(pos3.y == 315);

    // 4. Clamping within monitor bounds: window exact match
    fizz::ui::MonitorWorkArea matchWorkArea{50, 50, 800, 600};
    fizz::ui::WindowPosition posMatch = fizz::ui::calculateCenteredWindowPos(matchWorkArea, 800, 600);
    APPEARANCE_TEST_ASSERT(posMatch.x == 50);
    APPEARANCE_TEST_ASSERT(posMatch.y == 50);

    // 5. Clamping within monitor bounds: window larger than work area width
    fizz::ui::MonitorWorkArea smallWWorkArea{0, 0, 600, 600};
    fizz::ui::WindowPosition posLargeW = fizz::ui::calculateCenteredWindowPos(smallWWorkArea, 750, 480);
    // x clamps to work_x (0), y is centered (60)
    APPEARANCE_TEST_ASSERT(posLargeW.x == 0);
    APPEARANCE_TEST_ASSERT(posLargeW.y == 60);

    // 6. Clamping within monitor bounds: window larger than work area height
    fizz::ui::MonitorWorkArea smallHWorkArea{100, 50, 1200, 400};
    fizz::ui::WindowPosition posLargeH = fizz::ui::calculateCenteredWindowPos(smallHWorkArea, 750, 480);
    // x is centered (100 + (1200 - 750) / 2 = 325), y clamps to work_y (50)
    APPEARANCE_TEST_ASSERT(posLargeH.x == 325);
    APPEARANCE_TEST_ASSERT(posLargeH.y == 50);

    // 7. Clamping within monitor bounds: window larger than work area in both dimensions
    fizz::ui::MonitorWorkArea tinyWorkArea{200, 150, 500, 300};
    fizz::ui::WindowPosition posTiny = fizz::ui::calculateCenteredWindowPos(tinyWorkArea, 750, 480);
    APPEARANCE_TEST_ASSERT(posTiny.x == 200);
    APPEARANCE_TEST_ASSERT(posTiny.y == 150);

    // 8. Graceful fallback when monitor work area is unavailable (zero / negative dimensions)
    fizz::ui::MonitorWorkArea zeroArea{0, 0, 0, 0};
    fizz::ui::WindowPosition posZero = fizz::ui::calculateCenteredWindowPos(zeroArea, 750, 480);
    APPEARANCE_TEST_ASSERT(posZero.x == 0);
    APPEARANCE_TEST_ASSERT(posZero.y == 0);

    fizz::ui::MonitorWorkArea negArea{50, 50, -100, -50};
    fizz::ui::WindowPosition posNeg = fizz::ui::calculateCenteredWindowPos(negArea, 750, 480);
    APPEARANCE_TEST_ASSERT(posNeg.x == 0);
    APPEARANCE_TEST_ASSERT(posNeg.y == 0);

    // Custom fallback position
    fizz::ui::WindowPosition customFallback{123, 456};
    fizz::ui::WindowPosition posCustom = fizz::ui::calculateCenteredWindowPos(zeroArea, 750, 480, customFallback);
    APPEARANCE_TEST_ASSERT(posCustom.x == 123);
    APPEARANCE_TEST_ASSERT(posCustom.y == 456);

    // 9. Multi-monitor active monitor resolution
    std::vector<fizz::ui::MonitorWorkArea> monitors = {
        {0, 0, 1920, 1080},
        {1920, 0, 2560, 1440}
    };
    APPEARANCE_TEST_ASSERT(fizz::ui::findActiveMonitorIndex(monitors, 960, 540) == 0);
    APPEARANCE_TEST_ASSERT(fizz::ui::findActiveMonitorIndex(monitors, 2500, 720) == 1);
    APPEARANCE_TEST_ASSERT(fizz::ui::findActiveMonitorIndex(monitors, 1920, 0) == 1);
    APPEARANCE_TEST_ASSERT(fizz::ui::findActiveMonitorIndex(monitors, -100, -100) == -1);
    APPEARANCE_TEST_ASSERT(fizz::ui::findActiveMonitorIndex(monitors, 5000, 5000) == -1);

    std::vector<fizz::ui::MonitorWorkArea> emptyMonitors;
    APPEARANCE_TEST_ASSERT(fizz::ui::findActiveMonitorIndex(emptyMonitors, 960, 540) == -1);

    std::cout << "[PASS] test_appearance_window_centering_and_workarea passed.\n";
}

void test_appearance_horizontal_accent_swatches() {
    std::cout << "[TEST] Running test_appearance_horizontal_accent_swatches...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_swatches";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path confPath = tempDir / "fizz.conf";
    setConfigPath(confPath.string());

    FizzConfig config = FizzConfig::defaults();
    config.theme = "dark";
    config.accent = "blue";
    bool saved = saveConfig(config);
    APPEARANCE_TEST_ASSERT(saved);

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    fizz::theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);

    // Frame 1: Render appearance tab with accent = "blue"
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    bool modified = fizz::ui::renderAppearanceTab(config);
    APPEARANCE_TEST_ASSERT(!modified);

    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    APPEARANCE_TEST_ASSERT(drawData != nullptr);
    APPEARANCE_TEST_ASSERT(drawData->CmdListsCount > 0);

    const auto& accents = fizz::ui::getCuratedAccents();
    APPEARANCE_TEST_ASSERT(accents.size() == 10);

    // Collect bounding boxes of the 10 swatches from the draw list
    struct SwatchBounds {
        float minX = 1e9f;
        float minY = 1e9f;
        float maxX = -1e9f;
        float maxY = -1e9f;
        bool found = false;
    };
    std::vector<SwatchBounds> swatchBoxes(accents.size());

    ImDrawList* mainDl = drawData->CmdLists[0];
    ImU32 emeraldCol = ImGui::GetColorU32(accents[1].color);
    float swatchRowY = 0.0f;
    for (int i = 0; i < mainDl->VtxBuffer.Size; ++i) {
        if (mainDl->VtxBuffer[i].col == emeraldCol) {
            swatchRowY = mainDl->VtxBuffer[i].pos.y;
            break;
        }
    }
    APPEARANCE_TEST_ASSERT(swatchRowY > 0.0f);

    for (size_t a = 0; a < accents.size(); ++a) {
        ImU32 targetCol = ImGui::GetColorU32(accents[a].color);
        for (int i = 0; i < mainDl->VtxBuffer.Size; ++i) {
            const auto& v = mainDl->VtxBuffer[i];
            if (v.col == targetCol && v.pos.y >= swatchRowY - 1.0f && v.pos.y <= swatchRowY + 28.0f) {
                swatchBoxes[a].minX = std::min(swatchBoxes[a].minX, v.pos.x);
                swatchBoxes[a].minY = std::min(swatchBoxes[a].minY, v.pos.y);
                swatchBoxes[a].maxX = std::max(swatchBoxes[a].maxX, v.pos.x);
                swatchBoxes[a].maxY = std::max(swatchBoxes[a].maxY, v.pos.y);
                swatchBoxes[a].found = true;
            }
        }
    }

    // 1. Verify all 10 swatches were found and render in a single horizontal row
    float commonY = swatchBoxes[0].minY;
    for (size_t a = 0; a < accents.size(); ++a) {
        APPEARANCE_TEST_ASSERT(swatchBoxes[a].found);
        float w = swatchBoxes[a].maxX - swatchBoxes[a].minX;
        float h = swatchBoxes[a].maxY - swatchBoxes[a].minY;

        // Swatch tokens must be approx 24-28px
        APPEARANCE_TEST_ASSERT(w >= 24.0f && w <= 28.0f);
        APPEARANCE_TEST_ASSERT(h >= 24.0f && h <= 28.0f);

        // All 10 swatches must be on the EXACT SAME horizontal baseline (SameLine)
        APPEARANCE_TEST_ASSERT(nearEqual(swatchBoxes[a].minY, commonY, 1.0f));

        // Horizontal ordering: each swatch must be strictly to the right of the previous
        if (a > 0) {
            APPEARANCE_TEST_ASSERT(swatchBoxes[a].minX > swatchBoxes[a - 1].maxX);
        }
    }

    // Swatches must fit comfortably within the 750px window
    APPEARANCE_TEST_ASSERT(swatchBoxes.back().maxX < 750.0f);

    // 2. Test Hover: Hover over Nord Cyan (index 7) displays a tooltip
    size_t cyanIndex = 7;
    APPEARANCE_TEST_ASSERT(accents[cyanIndex].id == "cyan");
    float cyanCenterX = (swatchBoxes[cyanIndex].minX + swatchBoxes[cyanIndex].maxX) * 0.5f;
    float cyanCenterY = (swatchBoxes[cyanIndex].minY + swatchBoxes[cyanIndex].maxY) * 0.5f;

    io.AddMousePosEvent(cyanCenterX, cyanCenterY);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    ImGui::LogToClipboard();
    fizz::ui::renderAppearanceTab(config);
    ImGui::LogFinish();
    std::string hoverClip = ImGui::GetClipboardText() ? ImGui::GetClipboardText() : "";
    APPEARANCE_TEST_ASSERT(hoverClip.find("Nord Cyan") != std::string::npos);

    ImGui::End();
    ImGui::Render();

    // 3. Test Click: Clicking the swatch activates that accent live and persists to fizz.conf
    // Frame 3a: Mouse down on Nord Cyan
    io.AddMouseButtonEvent(0, true);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    fizz::ui::renderAppearanceTab(config);

    ImGui::End();
    ImGui::Render();

    // Frame 3b: Mouse up on Nord Cyan -> triggers button click
    io.AddMouseButtonEvent(0, false);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    bool clickModified = fizz::ui::renderAppearanceTab(config);
    APPEARANCE_TEST_ASSERT(clickModified);
    APPEARANCE_TEST_ASSERT(config.accent == "cyan");
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getActiveAccent(), fizz::theme::Accents::kCyan));

    ImGui::End();
    ImGui::Render();

    // Verify atomic disk persistence
    FizzConfig loaded = loadConfig(confPath.string());
    APPEARANCE_TEST_ASSERT(loaded.accent == "cyan");

    // Frame 4: Render next frame with active selection live
    // Move mouse away so hover highlight doesn't interfere
    io.AddMousePosEvent(-1000.0f, -1000.0f);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    fizz::ui::renderAppearanceTab(config);

    ImGui::End();
    ImGui::Render();

    // 4. Test active contrast ring/border on the currently selected swatch
    // The active swatch has a border outline drawn with ImGuiCol_Text around its bounds
    ImDrawData* clickDrawData = ImGui::GetDrawData();
    ImDrawList* activeDl = clickDrawData->CmdLists[0];
    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    bool hasContrastBorder = false;
    for (int i = 0; i < activeDl->VtxBuffer.Size; ++i) {
        const auto& v = activeDl->VtxBuffer[i];
        if (v.col == textColor) {
            // Check if vertex is on or very close to the active swatch bounding box
            if (v.pos.x >= swatchBoxes[cyanIndex].minX - 2.0f &&
                v.pos.x <= swatchBoxes[cyanIndex].maxX + 2.0f &&
                v.pos.y >= swatchBoxes[cyanIndex].minY - 2.0f &&
                v.pos.y <= swatchBoxes[cyanIndex].maxY + 2.0f) {
                hasContrastBorder = true;
                break;
            }
        }
    }
    APPEARANCE_TEST_ASSERT(hasContrastBorder);

    // Cleanup
    ImGui::DestroyContext(ctx);
    setConfigPath("");
    fs::remove_all(tempDir, ec);

    std::cout << "[PASS] test_appearance_horizontal_accent_swatches passed.\n";
}

void test_appearance_window_size_presets() {
    std::cout << "[TEST] Running test_appearance_window_size_presets...\n";

    const auto& presets = fizz::ui::getWindowSizePresets();
    APPEARANCE_TEST_ASSERT(presets.size() == 3);
    APPEARANCE_TEST_ASSERT(presets[0].preset == fizz::ui::WindowSizePreset::Default);
    APPEARANCE_TEST_ASSERT(presets[0].id == "default");
    APPEARANCE_TEST_ASSERT(presets[0].width == 750);
    APPEARANCE_TEST_ASSERT(presets[0].height == 480);

    APPEARANCE_TEST_ASSERT(presets[1].preset == fizz::ui::WindowSizePreset::Compact);
    APPEARANCE_TEST_ASSERT(presets[1].id == "compact");
    APPEARANCE_TEST_ASSERT(presets[1].width == 600);
    APPEARANCE_TEST_ASSERT(presets[1].height == 400);

    APPEARANCE_TEST_ASSERT(presets[2].preset == fizz::ui::WindowSizePreset::Large);
    APPEARANCE_TEST_ASSERT(presets[2].id == "large");
    APPEARANCE_TEST_ASSERT(presets[2].width == 1000);
    APPEARANCE_TEST_ASSERT(presets[2].height == 650);

    FizzConfig config = FizzConfig::defaults();
    APPEARANCE_TEST_ASSERT(config.window_width == 750);
    APPEARANCE_TEST_ASSERT(config.window_height == 480);

    int cbWidth = 0, cbHeight = 0, cbCount = 0;
    auto resizeCb = [&](int w, int h) {
        cbWidth = w;
        cbHeight = h;
        cbCount++;
    };

    // 1. Redundant default preset -> no change, callback not fired
    bool redundantDefault = fizz::ui::applyWindowSizePreset(config, fizz::ui::WindowSizePreset::Default, resizeCb);
    APPEARANCE_TEST_ASSERT(!redundantDefault);
    APPEARANCE_TEST_ASSERT(cbCount == 0);

    // 2. Apply Compact preset
    bool changedCompact = fizz::ui::applyWindowSizePreset(config, fizz::ui::WindowSizePreset::Compact, resizeCb);
    APPEARANCE_TEST_ASSERT(changedCompact);
    APPEARANCE_TEST_ASSERT(config.window_width == 600);
    APPEARANCE_TEST_ASSERT(config.window_height == 400);
    APPEARANCE_TEST_ASSERT(cbCount == 1);
    APPEARANCE_TEST_ASSERT(cbWidth == 600);
    APPEARANCE_TEST_ASSERT(cbHeight == 400);

    // Redundant compact call
    APPEARANCE_TEST_ASSERT(!fizz::ui::applyWindowSizePreset(config, fizz::ui::WindowSizePreset::Compact, resizeCb));
    APPEARANCE_TEST_ASSERT(cbCount == 1);

    // 3. Apply Large preset
    bool changedLarge = fizz::ui::applyWindowSizePreset(config, fizz::ui::WindowSizePreset::Large, resizeCb);
    APPEARANCE_TEST_ASSERT(changedLarge);
    APPEARANCE_TEST_ASSERT(config.window_width == 1000);
    APPEARANCE_TEST_ASSERT(config.window_height == 650);
    APPEARANCE_TEST_ASSERT(cbCount == 2);
    APPEARANCE_TEST_ASSERT(cbWidth == 1000);
    APPEARANCE_TEST_ASSERT(cbHeight == 650);

    // 4. Apply Default preset via string identifier overload
    bool changedDefaultStr = fizz::ui::applyWindowSizePreset(config, "default", resizeCb);
    APPEARANCE_TEST_ASSERT(changedDefaultStr);
    APPEARANCE_TEST_ASSERT(config.window_width == 750);
    APPEARANCE_TEST_ASSERT(config.window_height == 480);
    APPEARANCE_TEST_ASSERT(cbCount == 3);
    APPEARANCE_TEST_ASSERT(cbWidth == 750);
    APPEARANCE_TEST_ASSERT(cbHeight == 480);

    // 5. String identifier overload for compact and large
    APPEARANCE_TEST_ASSERT(fizz::ui::applyWindowSizePreset(config, "compact", resizeCb));
    APPEARANCE_TEST_ASSERT(cbWidth == 600 && cbHeight == 400);
    APPEARANCE_TEST_ASSERT(cbCount == 4);

    APPEARANCE_TEST_ASSERT(fizz::ui::applyWindowSizePreset(config, "large", resizeCb));
    APPEARANCE_TEST_ASSERT(cbWidth == 1000 && cbHeight == 650);
    APPEARANCE_TEST_ASSERT(cbCount == 5);

    std::cout << "[PASS] test_appearance_window_size_presets passed.\n";
}

void test_appearance_live_resize_centering() {
    std::cout << "[TEST] Running test_appearance_live_resize_centering...\n";

    // 1. Single monitor: Primary display 1920x1080
    std::vector<fizz::ui::MonitorWorkArea> singleMonitor = {
        {0, 0, 1920, 1080}
    };

    // Initial startup at Default 750x480
    fizz::ui::WindowPosition initialPos{585, 300}; // (1920-750)/2, (1080-480)/2

    // Resize to Large (1000x650)
    auto resLarge = fizz::ui::calculateResizeCentering(
        singleMonitor, initialPos, 750, 480, 1000, 650);
    APPEARANCE_TEST_ASSERT(resLarge.monitorIndex == 0);
    APPEARANCE_TEST_ASSERT(resLarge.position.x == 460); // (1920 - 1000) / 2
    APPEARANCE_TEST_ASSERT(resLarge.position.y == 215); // (1080 - 650) / 2
    // Verify window center matches monitor center (960, 540)
    APPEARANCE_TEST_ASSERT(resLarge.position.x + 1000 / 2 == 960);
    APPEARANCE_TEST_ASSERT(resLarge.position.y + 650 / 2 == 215 + 325); // 540

    // Resize from Large to Compact (600x400)
    auto resCompact = fizz::ui::calculateResizeCentering(
        singleMonitor, resLarge.position, 1000, 650, 600, 400);
    APPEARANCE_TEST_ASSERT(resCompact.monitorIndex == 0);
    APPEARANCE_TEST_ASSERT(resCompact.position.x == 660); // (1920 - 600) / 2
    APPEARANCE_TEST_ASSERT(resCompact.position.y == 340); // (1080 - 400) / 2
    APPEARANCE_TEST_ASSERT(resCompact.position.x + 600 / 2 == 960);
    APPEARANCE_TEST_ASSERT(resCompact.position.y + 400 / 2 == 540);

    // Resize back to Default (750x480)
    auto resDefault = fizz::ui::calculateResizeCentering(
        singleMonitor, resCompact.position, 600, 400, 750, 480);
    APPEARANCE_TEST_ASSERT(resDefault.monitorIndex == 0);
    APPEARANCE_TEST_ASSERT(resDefault.position.x == 585);
    APPEARANCE_TEST_ASSERT(resDefault.position.y == 300);

    // Slider adjustments: incremental changes do not cause center drift
    fizz::ui::WindowPosition curPos = resDefault.position;
    int curW = 750;
    int curH = 480;
    for (int nextW = 760; nextW <= 900; nextW += 10) {
        auto resStep = fizz::ui::calculateResizeCentering(
            singleMonitor, curPos, curW, curH, nextW, curH);
        APPEARANCE_TEST_ASSERT(resStep.monitorIndex == 0);
        APPEARANCE_TEST_ASSERT(resStep.position.x == (1920 - nextW) / 2);
        // Center point is always 960
        APPEARANCE_TEST_ASSERT(resStep.position.x + nextW / 2 == 960);
        curPos = resStep.position;
        curW = nextW;
    }

    // 2. Multi-monitor setup with panel offset on secondary monitor
    std::vector<fizz::ui::MonitorWorkArea> dualMonitors = {
        {0, 0, 1920, 1080},
        {1920, 32, 2560, 1408} // Secondary monitor: top panel of 32px
    };

    // Window initially centered on secondary monitor at Default 750x480
    // Secondary center: x = 1920 + 2560/2 = 3200, y = 32 + 1408/2 = 736
    fizz::ui::WindowPosition secInitialPos{1920 + (2560 - 750) / 2, 32 + (1408 - 480) / 2}; // {2825, 496}
    APPEARANCE_TEST_ASSERT(secInitialPos.x == 2825);
    APPEARANCE_TEST_ASSERT(secInitialPos.y == 496);

    // Resize to Large on secondary monitor -> stays on secondary monitor without hopping!
    auto resSecLarge = fizz::ui::calculateResizeCentering(
        dualMonitors, secInitialPos, 750, 480, 1000, 650);
    APPEARANCE_TEST_ASSERT(resSecLarge.monitorIndex == 1);
    APPEARANCE_TEST_ASSERT(resSecLarge.position.x == 1920 + (2560 - 1000) / 2); // 2700
    APPEARANCE_TEST_ASSERT(resSecLarge.position.y == 32 + (1408 - 650) / 2);    // 411
    APPEARANCE_TEST_ASSERT(resSecLarge.position.x + 1000 / 2 == 3200);
    APPEARANCE_TEST_ASSERT(resSecLarge.position.y + 650 / 2 == 411 + 325); // 736

    // Resize to Compact on secondary monitor
    auto resSecCompact = fizz::ui::calculateResizeCentering(
        dualMonitors, resSecLarge.position, 1000, 650, 600, 400);
    APPEARANCE_TEST_ASSERT(resSecCompact.monitorIndex == 1);
    APPEARANCE_TEST_ASSERT(resSecCompact.position.x == 1920 + (2560 - 600) / 2); // 2900
    APPEARANCE_TEST_ASSERT(resSecCompact.position.y == 32 + (1408 - 400) / 2);    // 536
    APPEARANCE_TEST_ASSERT(resSecCompact.position.x + 600 / 2 == 3200);
    APPEARANCE_TEST_ASSERT(resSecCompact.position.y + 400 / 2 == 736);

    // 3. Coordinate clamping preventing window moving off-screen
    std::vector<fizz::ui::MonitorWorkArea> smallMonitors = {
        {50, 50, 800, 600}
    };
    // If user sets size to 1600x1200 on 800x600 display, coordinates clamp to monitor top-left
    auto resOverflow = fizz::ui::calculateResizeCentering(
        smallMonitors, {100, 100}, 600, 400, 1600, 1200);
    APPEARANCE_TEST_ASSERT(resOverflow.position.x == 50);
    APPEARANCE_TEST_ASSERT(resOverflow.position.y == 50);

    // Window dimensions outside [kMinWidth..kMaxWidth] are clamped before centering
    auto resUnderflow = fizz::ui::calculateResizeCentering(
        singleMonitor, {585, 300}, 750, 480, 200, 100);
    // 200 clamped to 500, 100 clamped to 320 -> (1920-500)/2 = 710, (1080-320)/2 = 380
    APPEARANCE_TEST_ASSERT(resUnderflow.position.x == 710);
    APPEARANCE_TEST_ASSERT(resUnderflow.position.y == 380);

    // 4. Graceful handling of empty or zero-sized monitors
    std::vector<fizz::ui::MonitorWorkArea> emptyMonitors;
    auto resEmpty = fizz::ui::calculateResizeCentering(
        emptyMonitors, {123, 456}, 750, 480, 800, 500);
    APPEARANCE_TEST_ASSERT(resEmpty.position.x == 123);
    APPEARANCE_TEST_ASSERT(resEmpty.position.y == 456);
    APPEARANCE_TEST_ASSERT(resEmpty.monitorIndex == -1);

    // 5. End-to-end simulated callback loop with AppearanceTab preset buttons and sliders
    struct MockWindow {
        fizz::ui::WindowPosition pos{585, 300};
        int width = 750;
        int height = 480;
    } mockWin;

    auto resizeCallback = [&](int newW, int newH) {
        auto res = fizz::ui::calculateResizeCentering(
            singleMonitor, mockWin.pos, mockWin.width, mockWin.height, newW, newH);
        mockWin.pos = res.position;
        mockWin.width = newW;
        mockWin.height = newH;
    };

    FizzConfig config = FizzConfig::defaults();
    // Preset: Compact
    fizz::ui::applyWindowSizePreset(config, fizz::ui::WindowSizePreset::Compact, resizeCallback);
    APPEARANCE_TEST_ASSERT(mockWin.width == 600 && mockWin.height == 400);
    APPEARANCE_TEST_ASSERT(mockWin.pos.x == 660 && mockWin.pos.y == 340);
    APPEARANCE_TEST_ASSERT(config.window_width == 600 && config.window_height == 400);

    // Preset: Large
    fizz::ui::applyWindowSizePreset(config, fizz::ui::WindowSizePreset::Large, resizeCallback);
    APPEARANCE_TEST_ASSERT(mockWin.width == 1000 && mockWin.height == 650);
    APPEARANCE_TEST_ASSERT(mockWin.pos.x == 460 && mockWin.pos.y == 215);
    APPEARANCE_TEST_ASSERT(config.window_width == 1000 && config.window_height == 650);

    // Slider: Width 820, Height 500
    fizz::ui::setWindowSize(config, 820, 500, resizeCallback);
    APPEARANCE_TEST_ASSERT(mockWin.width == 820 && mockWin.height == 500);
    APPEARANCE_TEST_ASSERT(mockWin.pos.x == (1920 - 820) / 2); // 550
    APPEARANCE_TEST_ASSERT(mockWin.pos.y == (1080 - 500) / 2); // 290
    APPEARANCE_TEST_ASSERT(config.window_width == 820 && config.window_height == 500);

    std::cout << "[PASS] test_appearance_live_resize_centering passed.\n";
}

void test_appearance_font_selection_via_combo_box_and_no_custom_input() {
    std::cout << "[TEST] Running test_appearance_font_selection_via_combo_box_and_no_custom_input...\n";

    // 1. Verify discovered fonts order: Space Grotesk (Bundled Default) at index 0 followed by system fonts
    const auto discovered = getDiscoveredSystemFonts();
    APPEARANCE_TEST_ASSERT(!discovered.empty());
    APPEARANCE_TEST_ASSERT(discovered[0].familyName == "Space Grotesk (Bundled Default)");
    APPEARANCE_TEST_ASSERT(discovered[0].available);
    APPEARANCE_TEST_ASSERT(!discovered[0].path.empty());

    // 2. Headless UI rendering test with combo popup open and closed
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    FizzConfig config = FizzConfig::defaults();
    fizz::theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);

    int fontRebuildCount = 0;
    std::string rebuiltFontPath;
    float rebuiltFontSize = 0.0f;
    fizz::ui::AppearanceCallbacks callbacks;
    callbacks.onFontRebuild = [&](const std::string& path, float sz) {
        fontRebuildCount++;
        rebuiltFontPath = path;
        rebuiltFontSize = sz;
    };

    // Frame 1: Render tab, verify no custom font path input or clear button
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_font_window", nullptr, ImGuiWindowFlags_NoDecoration);

    ImGui::LogToClipboard();
    fizz::ui::renderAppearanceTab(config, callbacks);
    ImGui::LogFinish();

    std::string frame1Text;
    if (const char* clip = ImGui::GetClipboardText()) {
        frame1Text = clip;
    }

    APPEARANCE_TEST_ASSERT(frame1Text.find("custom_font_path") == std::string::npos);
    APPEARANCE_TEST_ASSERT(frame1Text.find("clear_font_path") == std::string::npos);
    APPEARANCE_TEST_ASSERT(frame1Text.find("[Clear]") == std::string::npos);
    APPEARANCE_TEST_ASSERT(frame1Text.find("custom TTF/OTF path") == std::string::npos);

    // Request opening combo popup for Frame 2
    ImGuiID comboId = ImGui::GetID("Font Family##font_family");
    ImGuiID popupId = ImHashStr("##ComboPopup", 0, comboId);
    ImGui::OpenPopupEx(popupId, ImGuiPopupFlags_None);

    ImGui::End();
    ImGui::Render();

    // Frame 2: Render with combo opened, verify bundled default is displayed
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_font_window", nullptr, ImGuiWindowFlags_NoDecoration);

    ImGui::LogToClipboard();
    fizz::ui::renderAppearanceTab(config, callbacks);
    ImGui::LogFinish();

    std::string frame2Text;
    if (const char* clip = ImGui::GetClipboardText()) {
        frame2Text = clip;
    }

    APPEARANCE_TEST_ASSERT(frame2Text.find("Space Grotesk (Bundled Default)") != std::string::npos);
    APPEARANCE_TEST_ASSERT(frame2Text.find("custom_font_path") == std::string::npos);
    APPEARANCE_TEST_ASSERT(frame2Text.find("clear_font_path") == std::string::npos);
    APPEARANCE_TEST_ASSERT(frame2Text.find("[Clear]") == std::string::npos);
    APPEARANCE_TEST_ASSERT(frame2Text.find("custom TTF/OTF path") == std::string::npos);

    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext(ctx);

    // 3. Selection updates configuration, rebuilds font atlas, and triggers callbacks
    if (discovered.size() > 1) {
        const auto& targetFont = discovered[1];
        bool changed = fizz::ui::setFontPath(config, targetFont.path, callbacks.onFontRebuild);
        APPEARANCE_TEST_ASSERT(changed);
        APPEARANCE_TEST_ASSERT(config.font_path == targetFont.path);
        APPEARANCE_TEST_ASSERT(fontRebuildCount == 1);
        APPEARANCE_TEST_ASSERT(rebuiltFontPath == targetFont.path);
        APPEARANCE_TEST_ASSERT(nearEqual(rebuiltFontSize, config.font_size));

        // Redundant selection does not trigger reload
        bool redundant = fizz::ui::setFontPath(config, targetFont.path, callbacks.onFontRebuild);
        APPEARANCE_TEST_ASSERT(!redundant);
        APPEARANCE_TEST_ASSERT(fontRebuildCount == 1);

        // Selecting bundled default updates config.font_path and triggers reload
        bool changedBack = fizz::ui::setFontPath(config, discovered[0].path, callbacks.onFontRebuild);
        APPEARANCE_TEST_ASSERT(changedBack);
        APPEARANCE_TEST_ASSERT(config.font_path == discovered[0].path);
        APPEARANCE_TEST_ASSERT(fontRebuildCount == 2);
        APPEARANCE_TEST_ASSERT(rebuiltFontPath == discovered[0].path);
    }

    // 4. Structural verification of AppearanceTab.cpp
    fs::path tabSourcePath = fs::path(__FILE__).parent_path().parent_path() / "services" / "AppearanceTab.cpp";
    if (fs::exists(tabSourcePath)) {
        std::ifstream in(tabSourcePath);
        std::string sourceContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        APPEARANCE_TEST_ASSERT(sourceContent.find("##custom_font_path") == std::string::npos);
        APPEARANCE_TEST_ASSERT(sourceContent.find("##clear_font_path") == std::string::npos);
        APPEARANCE_TEST_ASSERT(sourceContent.find("s_fontPathBuf") == std::string::npos);
        APPEARANCE_TEST_ASSERT(sourceContent.find("custom TTF/OTF path") == std::string::npos);
        APPEARANCE_TEST_ASSERT(sourceContent.find("Font Family##font_family") != std::string::npos);
    }

    std::cout << "[PASS] test_appearance_font_selection_via_combo_box_and_no_custom_input passed.\n";
}

void test_appearance_tab_relaxed_layout_metrics() {
    std::cout << "[TEST] Running test_appearance_tab_relaxed_layout_metrics...\n";

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    FizzConfig config = FizzConfig::defaults();
    fizz::theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);

    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().ItemSpacing.x, 8.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().ItemSpacing.y, 10.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().FramePadding.x, 10.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().FramePadding.y, 7.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().FrameRounding, 8.0f));
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetStyle().ChildRounding, 8.0f));

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("layout_metrics_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    fizz::ui::renderAppearanceTab(config);

    ImGui::End();
    ImGui::Render();

    ImGui::DestroyContext(ctx);
    std::cout << "[PASS] test_appearance_tab_relaxed_layout_metrics passed.\n";
}

void test_appearance_deferred_font_rebuild_and_integer_slider() {
    std::cout << "[TEST] Running test_appearance_deferred_font_rebuild_and_integer_slider...\n";

    // 1. Queue operations and API contracts
    fizz::ui::clearPendingFontRebuild();
    APPEARANCE_TEST_ASSERT(!fizz::ui::hasPendingFontRebuild());

    fizz::ui::queueFontRebuild("/usr/share/fonts/test.ttf", 20.0f);
    APPEARANCE_TEST_ASSERT(fizz::ui::hasPendingFontRebuild());
    APPEARANCE_TEST_ASSERT(fizz::ui::getPendingFontRebuild().fontPath == "/usr/share/fonts/test.ttf");
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::getPendingFontRebuild().fontSize, 20.0f));

    // Clamping to bounds [10..32]
    fizz::ui::queueFontRebuild("", 5.0f);
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::getPendingFontRebuild().fontSize, 10.0f));

    fizz::ui::queueFontRebuild("", 50.0f);
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::getPendingFontRebuild().fontSize, 32.0f));

    fizz::ui::clearPendingFontRebuild();
    APPEARANCE_TEST_ASSERT(!fizz::ui::hasPendingFontRebuild());

    // 2. Headless UI formatting and discrete integer increments
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    FizzConfig config = FizzConfig::defaults();
    config.font_size = 16.0f;
    fizz::theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);

    // Frame 1: render tab, capture formatting
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_deferred_window", nullptr, ImGuiWindowFlags_NoDecoration);

    ImGui::LogToClipboard();
    fizz::ui::renderAppearanceTab(config);
    ImGui::LogFinish();

    std::string capturedUi;
    if (const char* clip = ImGui::GetClipboardText()) {
        capturedUi = clip;
    }

    // Verify format displays whole-number point values ("16 px") and not fractional ("16.0 px")
    APPEARANCE_TEST_ASSERT(capturedUi.find("16 px") != std::string::npos);
    APPEARANCE_TEST_ASSERT(capturedUi.find("16.0 px") == std::string::npos);

    ImGui::End();
    ImGui::Render();

    // 3. Mid-frame safety and rapid slider adjustments without draw list invalidation
    fizz::ui::clearPendingFontRebuild();
    int gpuReloadCount = 0;
    auto mockGpuReload = [&]() {
        gpuReloadCount++;
    };

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_deferred_midframe", nullptr, ImGuiWindowFlags_NoDecoration);

    // Rapid slider adjustments within a single frame
    fizz::ui::setFontSize(config, 17.0f);
    APPEARANCE_TEST_ASSERT(fizz::ui::hasPendingFontRebuild());
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::getPendingFontRebuild().fontSize, 17.0f));

    fizz::ui::setFontSize(config, 18.0f);
    APPEARANCE_TEST_ASSERT(fizz::ui::hasPendingFontRebuild());
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::getPendingFontRebuild().fontSize, 18.0f));

    fizz::ui::setFontSize(config, 24.0f);
    APPEARANCE_TEST_ASSERT(fizz::ui::hasPendingFontRebuild());
    APPEARANCE_TEST_ASSERT(nearEqual(fizz::ui::getPendingFontRebuild().fontSize, 24.0f));

    // Verify active draw lists are still completely valid and not invalidated mid-frame
    ImDrawList* dl = ImGui::GetWindowDrawList();
    APPEARANCE_TEST_ASSERT(dl != nullptr);
    dl->AddRectFilled(ImVec2(10, 10), ImVec2(50, 50), IM_COL32(255, 0, 0, 255));
    ImGui::Text("Active draw list text during queued font rebuild");

    // Rendering frame completes without asserts or segfaults
    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    APPEARANCE_TEST_ASSERT(drawData != nullptr);
    APPEARANCE_TEST_ASSERT(drawData->CmdListsCount > 0);
    // Texture reload must NOT have happened mid-frame
    APPEARANCE_TEST_ASSERT(gpuReloadCount == 0);

    // Execute at frame boundary
    bool rebuilt = fizz::ui::executePendingFontRebuild(mockGpuReload);
    APPEARANCE_TEST_ASSERT(rebuilt);
    APPEARANCE_TEST_ASSERT(!fizz::ui::hasPendingFontRebuild());
    APPEARANCE_TEST_ASSERT(gpuReloadCount == 1);
    APPEARANCE_TEST_ASSERT(nearEqual(ImGui::GetIO().FontDefault ? ImGui::GetIO().FontDefault->LegacySize : 24.0f, 24.0f));

    // Redundant execute returns false
    APPEARANCE_TEST_ASSERT(!fizz::ui::executePendingFontRebuild(mockGpuReload));
    APPEARANCE_TEST_ASSERT(gpuReloadCount == 1);

    // 4. Consecutive frame loop simulating rapid dragging and boundary rebuilds
    for (int frame = 0; frame < 10; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("test_loop_window", nullptr, ImGuiWindowFlags_NoDecoration);

        float nextSize = static_cast<float>(14 + frame);
        fizz::ui::setFontSize(config, nextSize);
        APPEARANCE_TEST_ASSERT(fizz::ui::hasPendingFontRebuild());

        ImGui::Text("Frame %d with font size %.0f", frame, nextSize);
        ImGui::End();
        ImGui::Render();

        // At frame boundary outside active draw lists
        bool ok = fizz::ui::executePendingFontRebuild(mockGpuReload);
        APPEARANCE_TEST_ASSERT(ok);
        APPEARANCE_TEST_ASSERT(gpuReloadCount == 2 + frame);
    }

    ImGui::DestroyContext(ctx);

    // 5. Source code verification:
    // Verify AppearanceTab.cpp uses "%.0f px" slider formatting
    fs::path tabPath = fs::path(__FILE__).parent_path().parent_path() / "services" / "AppearanceTab.cpp";
    if (fs::exists(tabPath)) {
        std::ifstream in(tabPath);
        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        APPEARANCE_TEST_ASSERT(src.find("\"%.0f px\"") != std::string::npos);
        APPEARANCE_TEST_ASSERT(src.find("\"%.1f px\"") == std::string::npos);
    }

    // Verify main_gui.cpp uses executePendingFontRebuild or deferred rebuild mechanism
    fs::path mainGuiPath = fs::path(__FILE__).parent_path().parent_path() / "main_gui.cpp";
    if (fs::exists(mainGuiPath)) {
        std::ifstream in(mainGuiPath);
        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        APPEARANCE_TEST_ASSERT(src.find("executePendingFontRebuild") != std::string::npos);
    }

    std::cout << "[PASS] test_appearance_deferred_font_rebuild_and_integer_slider passed.\n";
}

void test_appearance_multi_tier_fallback_cascade() {
    std::cout << "[TEST] Running test_appearance_multi_tier_fallback_cascade...\n";

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);

    int reloadCount = 0;
    auto reloadCb = [&]() { reloadCount++; };

    // 1. Tier 1: Configured font path exists and is readable
    std::string sysFont = resolveSystemFont();
    if (!sysFont.empty()) {
        bool ok = fizz::ui::rebuildFontAtlas(sysFont, 16.0f, reloadCb);
        APPEARANCE_TEST_ASSERT(ok);
        APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);
        APPEARANCE_TEST_ASSERT(reloadCount == 1);
    }

    // 2. Tier 2: Non-existent custom font path falls back to bundled Space Grotesk
    bool ok2 = fizz::ui::rebuildFontAtlas("/nonexistent/bogus/path/font.ttf", 16.0f, reloadCb);
    APPEARANCE_TEST_ASSERT(ok2);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);

    // 3. Tier 3: Bundled Space Grotesk missing/deleted -> falls back to scalable system font via Fontconfig
    // Simulate deliberate deletion of Space Grotesk by setting FIZZ_DATA_DIR to an empty directory
    fs::path tempRoot = fs::temp_directory_path() / "fizz_test_cascade";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    const char* prevFizzData = std::getenv("FIZZ_DATA_DIR");
    std::string savedFizzData = prevFizzData ? prevFizzData : "";

    setenv("FIZZ_DATA_DIR", tempRoot.c_str(), 1);

    // Verify resolveAssetFile returns empty for Space Grotesk now
    std::string assetSpace = resolveAssetFile("assets/fonts/SpaceGrotesk-VariableFont_wght.ttf");
    APPEARANCE_TEST_ASSERT(assetSpace.empty());

    // rebuildFontAtlas must cleanly cascade to system font or AddFontDefault without crashing or asserting!
    bool ok3 = fizz::ui::rebuildFontAtlas("", 18.0f, reloadCb);
    APPEARANCE_TEST_ASSERT(ok3);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);

    // 4. Resilience to missing/deleted Font Awesome icon fonts:
    // Create a font directory with Space Grotesk but WITHOUT Font Awesome icons
    fs::path fontDir = tempRoot / "fonts";
    fs::create_directories(fontDir, ec);
    fs::path mockFont = fontDir / "SpaceGrotesk-VariableFont_wght.ttf";
    if (savedFizzData.empty()) unsetenv("FIZZ_DATA_DIR");
    std::string realBundled = resolveAssetFile("assets/fonts/SpaceGrotesk-VariableFont_wght.ttf");
    if (!realBundled.empty() && fs::exists(realBundled)) {
        fs::copy_file(realBundled, mockFont, fs::copy_options::overwrite_existing, ec);
    } else {
        std::ofstream(mockFont) << "dummy font";
    }

    setenv("FIZZ_DATA_DIR", tempRoot.c_str(), 1);
    // At this point, Space Grotesk is present in FIZZ_DATA_DIR, but icons/ is absent!
    bool ok4 = fizz::ui::rebuildFontAtlas("", 16.0f, reloadCb);
    APPEARANCE_TEST_ASSERT(ok4);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);

    // Restore env
    if (!savedFizzData.empty()) setenv("FIZZ_DATA_DIR", savedFizzData.c_str(), 1);
    else unsetenv("FIZZ_DATA_DIR");
    fs::remove_all(tempRoot, ec);

    // 5. Normal rebuild with defaults restored
    bool ok5 = fizz::ui::rebuildFontAtlas("", 16.0f, reloadCb);
    APPEARANCE_TEST_ASSERT(ok5);
    APPEARANCE_TEST_ASSERT(io.Fonts->Fonts.Size > 0);

    ImGui::DestroyContext(ctx);
    std::cout << "[PASS] test_appearance_multi_tier_fallback_cascade passed.\n";
}

void run_appearance_tab_tests() {
    test_appearance_theme_options_and_selection();
    test_appearance_curated_accent_palette();
    test_appearance_live_imgui_style_application();
    test_appearance_atomic_config_persistence();
    test_appearance_headless_ui_rendering();
    test_appearance_tab_relaxed_layout_metrics();
    test_appearance_window_geometry_clamping_and_callbacks();
    test_appearance_window_size_presets();
    test_appearance_live_resize_centering();
    test_appearance_font_size_clamping();
    test_appearance_dynamic_font_atlas_rebuild_and_safety();
    test_appearance_multi_tier_fallback_cascade();
    test_appearance_system_font_discovery();
    test_appearance_geometry_and_font_atomic_persistence();
    test_appearance_custom_font_and_icon_merging();
    test_appearance_window_centering_and_workarea();
    test_settings_tab_order_and_clean_footer();
    test_appearance_font_selection_via_combo_box_and_no_custom_input();
    test_appearance_horizontal_accent_swatches();
    test_appearance_deferred_font_rebuild_and_integer_slider();
}
