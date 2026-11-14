#include <iostream>
#include <cstdlib>
#include <cmath>
#include "services/Theme.h"

#define THEME_TEST_ASSERT(cond) \
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

void test_theme_soft_rounding_metrics() {
    std::cout << "[TEST] Running test_theme_soft_rounding_metrics...\n";

    ImGuiStyle style;
    // Set non-target initial values
    style.WindowRounding = 0.f;
    style.FrameRounding = 0.f;
    style.ChildRounding = 0.f;
    style.PopupRounding = 0.f;
    style.SelectableRounding = 0.f;
    style.ScrollbarRounding = 0.f;

    fizz::theme::applyTheme(style);

    THEME_TEST_ASSERT(nearEqual(style.WindowRounding, 12.0f));
    THEME_TEST_ASSERT(nearEqual(style.FrameRounding, 8.0f));
    THEME_TEST_ASSERT(nearEqual(style.ChildRounding, 8.0f));
    THEME_TEST_ASSERT(nearEqual(style.PopupRounding, 8.0f));
    THEME_TEST_ASSERT(nearEqual(style.SelectableRounding, 6.0f));
    THEME_TEST_ASSERT(nearEqual(style.ScrollbarRounding, 8.0f));

    THEME_TEST_ASSERT(nearEqual(style.WindowBorderSize, 1.0f));
    THEME_TEST_ASSERT(nearEqual(style.FrameBorderSize, 0.0f));
    THEME_TEST_ASSERT(nearEqual(style.ChildBorderSize, 1.0f));

    THEME_TEST_ASSERT(nearEqual(style.WindowPadding.x, 16.0f));
    THEME_TEST_ASSERT(nearEqual(style.WindowPadding.y, 14.0f));
    THEME_TEST_ASSERT(nearEqual(style.FramePadding.x, 10.0f));
    THEME_TEST_ASSERT(nearEqual(style.FramePadding.y, 8.0f));
    THEME_TEST_ASSERT(nearEqual(style.ItemSpacing.x, 0.0f));
    THEME_TEST_ASSERT(nearEqual(style.ItemSpacing.y, 4.0f));

    std::cout << "[PASS] test_theme_soft_rounding_metrics passed.\n";
}

void test_theme_with_alpha() {
    std::cout << "[TEST] Running test_theme_with_alpha...\n";

    ImVec4 base = {0.2f, 0.4f, 0.6f, 1.0f};
    ImVec4 result = fizz::theme::withAlpha(base, 0.5f);
    THEME_TEST_ASSERT(nearEqual(result.x, 0.2f));
    THEME_TEST_ASSERT(nearEqual(result.y, 0.4f));
    THEME_TEST_ASSERT(nearEqual(result.z, 0.6f));
    THEME_TEST_ASSERT(nearEqual(result.w, 0.5f));

    // Clamping checks
    ImVec4 clampedMin = fizz::theme::withAlpha(base, -0.2f);
    THEME_TEST_ASSERT(nearEqual(clampedMin.w, 0.0f));

    ImVec4 clampedMax = fizz::theme::withAlpha(base, 1.5f);
    THEME_TEST_ASSERT(nearEqual(clampedMax.w, 1.0f));

    std::cout << "[PASS] test_theme_with_alpha passed.\n";
}

void test_theme_adwaita_palette_colors() {
    std::cout << "[TEST] Running test_theme_adwaita_palette_colors...\n";

    ImGuiStyle style;
    fizz::theme::applyTheme(style);

    const ImVec4* c = style.Colors;
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_WindowBg], fizz::theme::Palette::kBg));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_Border], fizz::theme::Palette::kBorder));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_Text], fizz::theme::Palette::kText));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_TextDisabled], fizz::theme::Palette::kDim));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_FrameBg], fizz::theme::Palette::kFrameBg));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_FrameBgHovered], fizz::theme::Palette::kFrameBgHover));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_FrameBgActive], fizz::theme::Palette::kFrameBgActive));

    // Selection highlight header uses Adwaita Blue accent with comfortable translucent contrast
    // Hover and active states intensify selection
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_Header], fizz::theme::withAlpha(fizz::theme::Palette::kAccent, 0.20f)));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_HeaderHovered], fizz::theme::withAlpha(fizz::theme::Palette::kAccent, 0.26f)));
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_HeaderActive], fizz::theme::withAlpha(fizz::theme::Palette::kAccent, 0.32f)));

    // Separators use subtle border highlight
    THEME_TEST_ASSERT(vec4NearEqual(c[ImGuiCol_Separator], fizz::theme::Palette::kBorder));

    // Category badge colors
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Palette::kTagTeal, {0.200f, 0.780f, 0.720f, 1.00f}));

    std::cout << "[PASS] test_theme_adwaita_palette_colors passed.\n";
}

void test_theme_indicator_geometry() {
    std::cout << "[TEST] Running test_theme_indicator_geometry...\n";

    ImVec2 itemMin = {16.0f, 100.0f};
    ImVec2 itemMax = {734.0f, 124.0f}; // 24px height

    fizz::theme::IndicatorGeometry geom = fizz::theme::calculateIndicatorGeometry(itemMin, itemMax);

    // Left margin is 4px from itemMin.x
    THEME_TEST_ASSERT(nearEqual(geom.min.x, 20.0f));
    // Top margin is 3px down from itemMin.y
    THEME_TEST_ASSERT(nearEqual(geom.min.y, 103.0f));
    // Indicator width is 4.0px
    THEME_TEST_ASSERT(nearEqual(geom.max.x, 24.0f));
    // Bottom margin is 3px up from itemMax.y
    THEME_TEST_ASSERT(nearEqual(geom.max.y, 121.0f));
    // Soft rounding is 2.0px (half of 4.0px width for a capsule)
    THEME_TEST_ASSERT(nearEqual(geom.rounding, 2.0f));

    // Edge case: extremely small row height handles clamping gracefully
    ImVec2 smallMin = {16.0f, 100.0f};
    ImVec2 smallMax = {734.0f, 102.0f}; // height 2px < 2 * margin
    fizz::theme::IndicatorGeometry smallGeom = fizz::theme::calculateIndicatorGeometry(smallMin, smallMax);
    THEME_TEST_ASSERT(smallGeom.max.y >= smallGeom.min.y);

    std::cout << "[PASS] test_theme_indicator_geometry passed.\n";
}

void test_theme_curated_accents_and_normalization() {
    std::cout << "[TEST] Running test_theme_curated_accents_and_normalization...\n";

    // 1. Verify exact RGB definitions for all 10 curated accents in fizz::theme::Accents
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kBlue,    {0.208f, 0.518f, 0.894f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kEmerald, {0.180f, 0.761f, 0.494f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kPurple,  {0.569f, 0.255f, 0.675f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kAmber,   {0.949f, 0.702f, 0.102f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kCrimson, {0.878f, 0.106f, 0.141f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kRose,    {0.957f, 0.247f, 0.369f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kIndigo,  {0.388f, 0.400f, 0.945f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kCyan,    {0.024f, 0.714f, 0.831f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kSunset,  {0.976f, 0.451f, 0.086f, 1.00f}));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::Accents::kLime,    {0.518f, 0.800f, 0.086f, 1.00f}));

    // 2. Normalization: canonical IDs
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("blue") == "blue");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("emerald") == "emerald");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("purple") == "purple");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("amber") == "amber");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("crimson") == "crimson");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("rose") == "rose");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("indigo") == "indigo");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("cyan") == "cyan");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("sunset") == "sunset");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("lime") == "lime");

    // 3. Normalization: common aliases and case-insensitivity
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("Adwaita Blue") == "blue");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("green") == "emerald");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("Emerald Green") == "emerald");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("red") == "crimson");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("coral") == "rose");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("Rose / Coral") == "rose");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("electric indigo") == "indigo");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("nord cyan") == "cyan");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("orange") == "sunset");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("Sunset Orange") == "sunset");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("neon lime") == "lime");
    THEME_TEST_ASSERT(fizz::theme::normalizeAccent("unknown_accent") == "blue");

    // 4. Color resolution via getAccentColor for all 10 canonical IDs and aliases
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("blue"), fizz::theme::Accents::kBlue));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("emerald"), fizz::theme::Accents::kEmerald));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("purple"), fizz::theme::Accents::kPurple));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("amber"), fizz::theme::Accents::kAmber));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("crimson"), fizz::theme::Accents::kCrimson));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("rose"), fizz::theme::Accents::kRose));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("coral"), fizz::theme::Accents::kRose));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("indigo"), fizz::theme::Accents::kIndigo));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("electric indigo"), fizz::theme::Accents::kIndigo));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("cyan"), fizz::theme::Accents::kCyan));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("nord cyan"), fizz::theme::Accents::kCyan));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("sunset"), fizz::theme::Accents::kSunset));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("orange"), fizz::theme::Accents::kSunset));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("sunset orange"), fizz::theme::Accents::kSunset));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("lime"), fizz::theme::Accents::kLime));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("neon lime"), fizz::theme::Accents::kLime));
    THEME_TEST_ASSERT(vec4NearEqual(fizz::theme::getAccentColor("invalid"), fizz::theme::Accents::kBlue));

    std::cout << "[PASS] test_theme_curated_accents_and_normalization passed.\n";
}

void run_theme_tests() {
    test_theme_soft_rounding_metrics();
    test_theme_with_alpha();
    test_theme_adwaita_palette_colors();
    test_theme_indicator_geometry();
    test_theme_curated_accents_and_normalization();
}
