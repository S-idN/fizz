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

void run_theme_tests() {
    test_theme_soft_rounding_metrics();
    test_theme_with_alpha();
    test_theme_adwaita_palette_colors();
    test_theme_indicator_geometry();
}
