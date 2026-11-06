#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include "services/SearchUi.h"

#define UI_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

static bool nearEqual(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) <= eps;
}

void test_status_footer_formatting_and_shortcuts() {
    std::cout << "[TEST] Running test_status_footer_formatting_and_shortcuts...\n";

    // Hit count formatting
    UI_TEST_ASSERT(fizz::ui::formatHitCount(0) == "0 hits");
    UI_TEST_ASSERT(fizz::ui::formatHitCount(1) == "1 hit");
    UI_TEST_ASSERT(fizz::ui::formatHitCount(2) == "2 hits");
    UI_TEST_ASSERT(fizz::ui::formatHitCount(50) == "50 hits");
    UI_TEST_ASSERT(fizz::ui::formatHitCount(-5) == "0 hits");

    // Keyboard navigation shortcuts
    auto shortcuts = fizz::ui::getNavigationShortcuts();
    UI_TEST_ASSERT(shortcuts.size() == 3);

    UI_TEST_ASSERT(shortcuts[0].key == "↵");
    UI_TEST_ASSERT(shortcuts[0].action == "Open");

    UI_TEST_ASSERT(shortcuts[1].key == "⇧↵");
    UI_TEST_ASSERT(shortcuts[1].action == "Folder");

    UI_TEST_ASSERT(shortcuts[2].key == "Esc");
    UI_TEST_ASSERT(shortcuts[2].action == "Quit");

    // Footer layout calculation
    float contentHeight = 452.0f;
    float footerHeight = 28.0f;
    float separatorSpacing = 4.0f;

    auto layout = fizz::ui::calculateFooterLayout(contentHeight, footerHeight, separatorSpacing);
    UI_TEST_ASSERT(nearEqual(layout.footerHeight, 28.0f));
    UI_TEST_ASSERT(nearEqual(layout.reservedBottomSpace, footerHeight + separatorSpacing));
    UI_TEST_ASSERT(nearEqual(layout.resultsChildHeight, -(footerHeight + separatorSpacing)));

    std::cout << "[PASS] test_status_footer_formatting_and_shortcuts passed.\n";
}

void test_search_pill_geometry_and_metrics() {
    std::cout << "[TEST] Running test_search_pill_geometry_and_metrics...\n";

    // Placeholder hint text specification verification
    UI_TEST_ASSERT(fizz::ui::kDefaultPlaceholderText == "Search files and apps...");

    // Default metrics verification
    UI_TEST_ASSERT(nearEqual(fizz::ui::Metrics::kSearchPillHeight, 38.0f));
    UI_TEST_ASSERT(nearEqual(fizz::ui::Metrics::kSearchPillRounding, 10.0f));
    UI_TEST_ASSERT(nearEqual(fizz::ui::Metrics::kSearchPillPaddingX, 12.0f));
    UI_TEST_ASSERT(nearEqual(fizz::ui::Metrics::kSearchPillPaddingY, 6.0f));
    UI_TEST_ASSERT(nearEqual(fizz::ui::Metrics::kPromptGlyphWidth, 12.0f));
    UI_TEST_ASSERT(nearEqual(fizz::ui::Metrics::kPromptSpacing, 8.0f));

    // Calculate pill geometry given origin (16, 14) and width 718, height 38
    ImVec2 origin = {16.0f, 14.0f};
    float width = 718.0f;
    float height = 38.0f;

    auto geom = fizz::ui::calculateSearchPillGeometry(origin, width, height);

    UI_TEST_ASSERT(nearEqual(geom.pillMin.x, 16.0f));
    UI_TEST_ASSERT(nearEqual(geom.pillMin.y, 14.0f));
    UI_TEST_ASSERT(nearEqual(geom.pillMax.x, 16.0f + 718.0f));
    UI_TEST_ASSERT(nearEqual(geom.pillMax.y, 14.0f + 38.0f));
    UI_TEST_ASSERT(nearEqual(geom.rounding, 10.0f));

    // Prompt position starts at origin.x + kSearchPillPaddingX
    UI_TEST_ASSERT(nearEqual(geom.promptPos.x, 16.0f + 12.0f));
    // Prompt vertically centered
    UI_TEST_ASSERT(nearEqual(geom.promptPos.y, 14.0f + (38.0f - 16.0f) * 0.5f));

    // Input position starts after prompt + spacing
    float expectedInputX = geom.promptPos.x + 12.0f + 8.0f;
    UI_TEST_ASSERT(nearEqual(geom.inputPos.x, expectedInputX));
    // Input width occupies remaining width up to pillMax.x - kSearchPillPaddingX
    float expectedInputW = geom.pillMax.x - 12.0f - expectedInputX;
    UI_TEST_ASSERT(nearEqual(geom.inputWidth, expectedInputW));

    std::cout << "[PASS] test_search_pill_geometry_and_metrics passed.\n";
}

static bool isGlyphInRanges(const ImWchar* ranges, ImWchar c) {
    if (!ranges) return false;
    for (int i = 0; ranges[i] != 0; i += 2) {
        if (c >= ranges[i] && c <= ranges[i + 1]) {
            return true;
        }
    }
    return false;
}

void test_ui_glyph_ranges() {
    std::cout << "[TEST] Running test_ui_glyph_ranges...\n";

    const ImWchar* ranges = fizz::ui::getUiGlyphRanges();
    UI_TEST_ASSERT(ranges != nullptr);

    // Basic Latin
    UI_TEST_ASSERT(isGlyphInRanges(ranges, ' '));
    UI_TEST_ASSERT(isGlyphInRanges(ranges, 'A'));
    UI_TEST_ASSERT(isGlyphInRanges(ranges, 'z'));
    UI_TEST_ASSERT(isGlyphInRanges(ranges, '>'));

    // Navigation arrow glyphs
    UI_TEST_ASSERT(isGlyphInRanges(ranges, 0x21B5)); // '↵'
    UI_TEST_ASSERT(isGlyphInRanges(ranges, 0x21E7)); // '⇧'

    // Warning emoji
    UI_TEST_ASSERT(isGlyphInRanges(ranges, 0x26A0)); // '⚠️'

    // Uncovered control character
    UI_TEST_ASSERT(!isGlyphInRanges(ranges, 0x0005));

    std::cout << "[PASS] test_ui_glyph_ranges passed.\n";
}

void test_headless_ui_rendering() {
    std::cout << "[TEST] Running test_headless_ui_rendering...\n";

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_window", nullptr, ImGuiWindowFlags_NoDecoration);

    char searchBuf[256] = "";
    bool changed = fizz::ui::renderSearchPill(searchBuf, sizeof(searchBuf), fizz::ui::kDefaultPlaceholderText, true);
    UI_TEST_ASSERT(!changed);

    // Render with 0 hits
    fizz::ui::renderStatusFooter(0, "");

    // Render with warning message
    fizz::ui::renderStatusFooter(5, "⚠️ Untrusted Desktop Entry");

    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    UI_TEST_ASSERT(drawData != nullptr);
    UI_TEST_ASSERT(drawData->CmdListsCount > 0);

    ImGui::DestroyContext(ctx);

    std::cout << "[PASS] test_headless_ui_rendering passed.\n";
}

void run_search_ui_tests() {
    test_status_footer_formatting_and_shortcuts();
    test_search_pill_geometry_and_metrics();
    test_ui_glyph_ranges();
    test_headless_ui_rendering();
}
