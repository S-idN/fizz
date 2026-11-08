#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <filesystem>
#include <sys/stat.h>
#include "services/AppearanceTab.h"
#include "services/Theme.h"
#include "services/Config.h"
#include "imgui.h"

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
    APPEARANCE_TEST_ASSERT(accents.size() == 5);

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

    // Verify individual color calculations & lookups via getAccentColor
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("blue"), fizz::theme::Accents::kBlue));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("emerald"), fizz::theme::Accents::kEmerald));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("purple"), fizz::theme::Accents::kPurple));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("amber"), fizz::theme::Accents::kAmber));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("crimson"), fizz::theme::Accents::kCrimson));

    // Aliases
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("green"), fizz::theme::Accents::kEmerald));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("red"), fizz::theme::Accents::kCrimson));
    APPEARANCE_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("orange"), fizz::theme::Accents::kAmber));

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
    if (ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Appearance")) {
            tabModified = fizz::ui::renderAppearanceTab(config);
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

    ImGui::DestroyContext(ctx);

    std::cout << "[PASS] test_appearance_headless_ui_rendering passed.\n";
}

void run_appearance_tab_tests() {
    test_appearance_theme_options_and_selection();
    test_appearance_curated_accent_palette();
    test_appearance_live_imgui_style_application();
    test_appearance_atomic_config_persistence();
    test_appearance_headless_ui_rendering();
}
