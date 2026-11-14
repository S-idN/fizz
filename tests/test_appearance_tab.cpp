#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>
#include "services/AppearanceTab.h"
#include "services/FontDiscovery.h"
#include "services/Theme.h"
#include "services/Config.h"
#include "imgui.h"
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

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("settings_test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    // Render TabBar with Appearance tab
    bool tabModified = false;
    std::string capturedUiTextFrame1;
    if (ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Appearance")) {
            ImGui::LogToClipboard();
            tabModified = fizz::ui::renderAppearanceTab(config);
            ImGui::LogFinish();
            if (const char* clip = ImGui::GetClipboardText()) {
                capturedUiTextFrame1 = clip;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("General")) {
            ImGui::Text("General Settings Tab");
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

    // First entry must be System Default
    APPEARANCE_TEST_ASSERT(fonts[0].familyName.find("Default") != std::string::npos);
    APPEARANCE_TEST_ASSERT(fonts[0].path.empty());
    APPEARANCE_TEST_ASSERT(fonts[0].available);

    // Check known font families exist in the returned cascade list
    auto hasFamily = [&](const std::string& name) {
        return std::any_of(fonts.begin(), fonts.end(), [&](const DiscoveredFont& f) {
            return f.familyName.find(name) != std::string::npos;
        });
    };

    APPEARANCE_TEST_ASSERT(hasFamily("Adwaita"));
    APPEARANCE_TEST_ASSERT(hasFamily("Cantarell"));
    APPEARANCE_TEST_ASSERT(hasFamily("Google Sans"));
    APPEARANCE_TEST_ASSERT(hasFamily("DejaVu"));
    APPEARANCE_TEST_ASSERT(hasFamily("JetBrains"));

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

void run_appearance_tab_tests() {
    test_appearance_theme_options_and_selection();
    test_appearance_curated_accent_palette();
    test_appearance_live_imgui_style_application();
    test_appearance_atomic_config_persistence();
    test_appearance_headless_ui_rendering();
    test_appearance_window_geometry_clamping_and_callbacks();
    test_appearance_font_size_clamping();
    test_appearance_dynamic_font_atlas_rebuild_and_safety();
    test_appearance_system_font_discovery();
    test_appearance_geometry_and_font_atomic_persistence();
    test_appearance_custom_font_and_icon_merging();
}
