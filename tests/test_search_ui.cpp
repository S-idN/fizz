#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include "services/SearchUi.h"
#include "services/Theme.h"
#include "services/FileSearch.h"

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

static bool vec4NearEqual(const ImVec4& a, const ImVec4& b, float eps = 0.001f) {
    return nearEqual(a.x, b.x, eps) &&
           nearEqual(a.y, b.y, eps) &&
           nearEqual(a.z, b.z, eps) &&
           nearEqual(a.w, b.w, eps);
}

void test_category_badges() {
    std::cout << "[TEST] Running test_category_badges...\n";

    // Executable badge
    auto bExec = fizz::ui::getCategoryBadge("/usr/bin/bash", ExecState::Executable);
    UI_TEST_ASSERT(bExec.label == "exec");
    UI_TEST_ASSERT(vec4NearEqual(bExec.color, fizz::theme::Palette::kTagOrange));

    // Code badge
    auto bCode = fizz::ui::getCategoryBadge("/home/user/project/main.cpp", ExecState::NotExecutable);
    UI_TEST_ASSERT(bCode.label == "code");
    UI_TEST_ASSERT(vec4NearEqual(bCode.color, fizz::theme::Palette::kTagPurple));

    // Document badge (distinct teal color)
    auto bDoc = fizz::ui::getCategoryBadge("/home/user/notes/report.pdf", ExecState::NotExecutable);
    UI_TEST_ASSERT(bDoc.label == "doc");
    UI_TEST_ASSERT(vec4NearEqual(bDoc.color, fizz::theme::Palette::kTagTeal));

    // Media badge
    auto bMedia = fizz::ui::getCategoryBadge("/home/user/music/song.mp3", ExecState::NotExecutable);
    UI_TEST_ASSERT(bMedia.label == "media");
    UI_TEST_ASSERT(vec4NearEqual(bMedia.color, fizz::theme::Palette::kTagGreen));

    // Image badge
    auto bImage = fizz::ui::getCategoryBadge("/home/user/pictures/photo.png", ExecState::NotExecutable);
    UI_TEST_ASSERT(bImage.label == "image");
    UI_TEST_ASSERT(vec4NearEqual(bImage.color, fizz::theme::Palette::kTagPink));

    // Archive badge
    auto bArchive = fizz::ui::getCategoryBadge("/home/user/backups/data.tar.gz", ExecState::NotExecutable);
    UI_TEST_ASSERT(bArchive.label == "archive");
    UI_TEST_ASSERT(vec4NearEqual(bArchive.color, fizz::theme::Palette::kTagGrey));

    // Text badge
    auto bText = fizz::ui::getCategoryBadge("/home/user/notes.txt", ExecState::NotExecutable);
    UI_TEST_ASSERT(bText.label == "text");
    UI_TEST_ASSERT(vec4NearEqual(bText.color, fizz::theme::Palette::kTagBlue));

    // All category badge colors must be mutually distinct
    const ImVec4 colors[] = {
        bExec.color, bCode.color, bDoc.color, bMedia.color,
        bImage.color, bArchive.color, bText.color
    };
    constexpr size_t numColors = sizeof(colors) / sizeof(colors[0]);
    for (size_t i = 0; i < numColors; ++i) {
        for (size_t j = i + 1; j < numColors; ++j) {
            UI_TEST_ASSERT(!vec4NearEqual(colors[i], colors[j]));
        }
    }

    // Unclassified file returns empty label
    auto bNone = fizz::ui::getCategoryBadge("/home/user/binary_data", ExecState::NotExecutable);
    UI_TEST_ASSERT(bNone.label.empty());

    std::cout << "[PASS] test_category_badges passed.\n";
}

void test_badge_geometry() {
    std::cout << "[TEST] Running test_badge_geometry...\n";

    // Standard row bounds: min (0, 50), max (750, 74) -> height 24px
    ImVec2 rowMin = {0.0f, 50.0f};
    ImVec2 rowMax = {750.0f, 74.0f};
    float textWidth = 32.0f; // e.g. "code" text width
    float fontSize = 16.0f;

    auto geom = fizz::ui::calculateBadgeGeometry(rowMin, rowMax, textWidth, fontSize);

    // Badge height is row height (24px) - 2 * marginY (3px) = 18px
    UI_TEST_ASSERT(nearEqual(geom.height, 18.0f));
    UI_TEST_ASSERT(nearEqual(geom.min.y, 53.0f));
    UI_TEST_ASSERT(nearEqual(geom.max.y, 71.0f));

    // Badge width is text width + 2 * paddingX (7px) = 46px
    float expectedWidth = textWidth + 2.0f * fizz::ui::Metrics::kBadgePaddingX;
    UI_TEST_ASSERT(nearEqual(geom.width, expectedWidth));

    // Badge is positioned right-aligned with margin right (10px)
    UI_TEST_ASSERT(nearEqual(geom.max.x, 750.0f - fizz::ui::Metrics::kBadgeMarginRight));
    UI_TEST_ASSERT(nearEqual(geom.min.x, geom.max.x - expectedWidth));

    // Rounding is half the badge height (9.0px) for a perfect pill shape
    UI_TEST_ASSERT(nearEqual(geom.rounding, 9.0f));

    // Text position is centered horizontally and vertically
    float expectedTextX = geom.min.x + fizz::ui::Metrics::kBadgePaddingX;
    float expectedTextY = geom.min.y + (geom.height - fontSize) * 0.5f;
    UI_TEST_ASSERT(nearEqual(geom.textPos.x, expectedTextX));
    UI_TEST_ASSERT(nearEqual(geom.textPos.y, expectedTextY));

    std::cout << "[PASS] test_badge_geometry passed.\n";
}

void test_path_truncation_widths() {
    std::cout << "[TEST] Running test_path_truncation_widths...\n";

    auto measureText = [](std::string_view s) -> float {
        return static_cast<float>(s.size()) * 10.0f;
    };

    std::string_view deepPath = "/home/user/workspace/fizz/services"; // 34 chars = 340px

    // 1. Ample container width: returns exact path unmodified without ellipsis
    std::string full = fizz::ui::truncatePath(deepPath, 400.0f, measureText);
    UI_TEST_ASSERT(full == deepPath);
    UI_TEST_ASSERT(measureText(full) <= 400.0f);

    // 2. Exact match at path width
    std::string exact = fizz::ui::truncatePath(deepPath, 340.0f, measureText);
    UI_TEST_ASSERT(exact == deepPath);

    // 3. Container width 250px (25 chars): intelligent middle truncation preserves root & tail
    std::string mid250 = fizz::ui::truncatePath(deepPath, 250.0f, measureText);
    UI_TEST_ASSERT(measureText(mid250) <= 250.0f);
    UI_TEST_ASSERT(mid250.find("...") != std::string::npos);
    UI_TEST_ASSERT(mid250.find("services") != std::string::npos); // Tail directory context preserved

    // 4. Container width 180px (18 chars): preserves /home and services
    std::string mid180 = fizz::ui::truncatePath(deepPath, 180.0f, measureText);
    UI_TEST_ASSERT(measureText(mid180) <= 180.0f);
    UI_TEST_ASSERT(mid180 == "/home/.../services");

    // 5. Container width 130px (13 chars): preserves leading root slash /.../services
    std::string mid130 = fizz::ui::truncatePath(deepPath, 130.0f, measureText);
    UI_TEST_ASSERT(measureText(mid130) <= 130.0f);
    UI_TEST_ASSERT(mid130 == "/.../services");

    // 6. Tight container width 60px (6 chars): character-level middle truncation
    std::string tight = fizz::ui::truncatePath(deepPath, 60.0f, measureText);
    UI_TEST_ASSERT(measureText(tight) <= 60.0f);
    UI_TEST_ASSERT(tight.find("...") != std::string::npos);

    // 7. Bare minimum container width 30px: only ellipsis fits
    std::string ellipsisOnly = fizz::ui::truncatePath(deepPath, 30.0f, measureText);
    UI_TEST_ASSERT(measureText(ellipsisOnly) <= 30.0f);
    UI_TEST_ASSERT(ellipsisOnly == "...");

    // 8. Width less than ellipsis: must return empty string to prevent overflow/overlap
    std::string subEllipsis = fizz::ui::truncatePath(deepPath, 25.0f, measureText);
    UI_TEST_ASSERT(subEllipsis.empty());

    // 9. Zero and negative container widths
    UI_TEST_ASSERT(fizz::ui::truncatePath(deepPath, 0.0f, measureText).empty());
    UI_TEST_ASSERT(fizz::ui::truncatePath(deepPath, -50.0f, measureText).empty());

    // 10. Empty path returns empty string
    UI_TEST_ASSERT(fizz::ui::truncatePath("", 300.0f, measureText).empty());

    // 11. Root path "/" preserves "/" without truncation when space permits
    UI_TEST_ASSERT(fizz::ui::truncatePath("/", 50.0f, measureText) == "/");

    // 12. Path with no slashes
    std::string_view singleName = "supercalifragilistic"; // 20 chars = 200px
    std::string noSlash = fizz::ui::truncatePath(singleName, 100.0f, measureText);
    UI_TEST_ASSERT(measureText(noSlash) <= 100.0f);
    UI_TEST_ASSERT(noSlash.find("...") != std::string::npos);

    // 13. Dynamic width sweep across container widths 0px..500px: invariant check
    for (float w = 0.0f; w <= 500.0f; w += 15.0f) {
        std::string resMid = fizz::ui::truncatePath(deepPath, w, measureText);
        UI_TEST_ASSERT(measureText(resMid) <= w);
    }

    std::cout << "[PASS] test_path_truncation_widths passed.\n";
}

void test_row_layout_allocation_and_non_overlap() {
    std::cout << "[TEST] Running test_row_layout_allocation_and_non_overlap...\n";

    auto measureText = [](std::string_view s) -> float {
        return static_cast<float>(s.size()) * 10.0f;
    };

    std::string_view fileName = "SearchUi.cpp";                     // 12 chars = 120px
    std::string_view dirPath  = "/home/user/workspace/fizz/services"; // 34 chars = 340px
    fizz::ui::CategoryBadge badge{"code", fizz::theme::Palette::kTagPurple}; // 4 chars = 40px

    // 1. Standard launcher width: 750px
    ImVec2 rowMin = {0.0f, 100.0f};
    ImVec2 rowMax750 = {750.0f, 124.0f};

    auto layout750 = fizz::ui::calculateRowLayout(rowMin, rowMax750, fileName, dirPath, badge, measureText);

    UI_TEST_ASSERT(layout750.hasBadge);
    UI_TEST_ASSERT(nearEqual(layout750.namePos.x, 16.0f));
    UI_TEST_ASSERT(nearEqual(layout750.nameWidth, 120.0f));

    // Filename and directory do not overlap: dir starts strictly after name + gap
    float expectedDirStartX = 16.0f + 120.0f + fizz::ui::Metrics::kFileNameDirSpacing;
    UI_TEST_ASSERT(nearEqual(layout750.dirPos.x, expectedDirStartX));
    UI_TEST_ASSERT(layout750.dirPos.x >= layout750.namePos.x + layout750.nameWidth);

    // Directory path and badge do not overlap
    UI_TEST_ASSERT(layout750.dirPos.x + layout750.dirWidth <= layout750.textRightLimit);
    UI_TEST_ASSERT(layout750.textRightLimit <= layout750.badgeGeom.min.x - fizz::ui::Metrics::kBadgeSpacing);
    UI_TEST_ASSERT(layout750.dirPos.x + layout750.dirWidth < layout750.badgeGeom.min.x);

    // Badge pill dimensions are fully preserved
    UI_TEST_ASSERT(nearEqual(layout750.badgeGeom.height, 18.0f));
    UI_TEST_ASSERT(nearEqual(layout750.badgeGeom.max.x, 750.0f - fizz::ui::Metrics::kBadgeMarginRight));

    // 2. Narrow launcher width: 350px (forcing path truncation with ellipsis)
    ImVec2 rowMax350 = {350.0f, 124.0f};
    auto layout350 = fizz::ui::calculateRowLayout(rowMin, rowMax350, fileName, dirPath, badge, measureText);

    UI_TEST_ASSERT(layout350.hasBadge);
    UI_TEST_ASSERT(!layout350.truncatedDir.empty());
    UI_TEST_ASSERT(layout350.truncatedDir.find("...") != std::string::npos);

    // Strict non-overlap invariants in narrow layout
    UI_TEST_ASSERT(layout350.dirPos.x >= layout350.namePos.x + layout350.nameWidth);
    UI_TEST_ASSERT(layout350.dirPos.x + layout350.dirWidth <= layout350.textRightLimit);
    UI_TEST_ASSERT(layout350.dirPos.x + layout350.dirWidth < layout350.badgeGeom.min.x);
    UI_TEST_ASSERT(nearEqual(layout350.badgeGeom.max.x, 350.0f - fizz::ui::Metrics::kBadgeMarginRight));

    // 3. Extremely tight launcher width: 200px (filename + badge occupy all room)
    ImVec2 rowMax200 = {200.0f, 124.0f};
    auto layout200 = fizz::ui::calculateRowLayout(rowMin, rowMax200, fileName, dirPath, badge, measureText);

    // Directory path is omitted to prevent any overlapping with filename or badge
    UI_TEST_ASSERT(layout200.truncatedDir.empty());
    UI_TEST_ASSERT(nearEqual(layout200.dirWidth, 0.0f));
    UI_TEST_ASSERT(layout200.hasBadge);
    UI_TEST_ASSERT(nearEqual(layout200.badgeGeom.max.x, 200.0f - fizz::ui::Metrics::kBadgeMarginRight));

    // 4. Launcher row without badge
    fizz::ui::CategoryBadge noBadge{"", ImVec4(0, 0, 0, 0)};
    auto layoutNoBadge = fizz::ui::calculateRowLayout(rowMin, rowMax750, fileName, dirPath, noBadge, measureText);

    UI_TEST_ASSERT(!layoutNoBadge.hasBadge);
    // Right limit extends to right margin since no badge is present
    UI_TEST_ASSERT(nearEqual(layoutNoBadge.textRightLimit, 750.0f - fizz::ui::Metrics::kBadgeMarginRight));
    UI_TEST_ASSERT(layoutNoBadge.dirPos.x + layoutNoBadge.dirWidth <= layoutNoBadge.textRightLimit);

    std::cout << "[PASS] test_row_layout_allocation_and_non_overlap passed.\n";
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
    float footerHeight = 28.0f;
    float separatorSpacing = 4.0f;

    auto layout = fizz::ui::calculateFooterLayout(footerHeight, separatorSpacing);
    UI_TEST_ASSERT(nearEqual(layout.footerHeight, 28.0f));
    UI_TEST_ASSERT(nearEqual(layout.reservedBottomSpace, footerHeight + separatorSpacing));
    UI_TEST_ASSERT(nearEqual(layout.reservedChildHeight, -(footerHeight + separatorSpacing)));

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

    // Input position starts after prompt + spacing and origin.y for full-height frame padding centering
    float expectedInputX = geom.promptPos.x + 12.0f + 8.0f;
    UI_TEST_ASSERT(nearEqual(geom.inputPos.x, expectedInputX));
    UI_TEST_ASSERT(nearEqual(geom.inputPos.y, origin.y));
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

    // Render search hit row with category badge pill and truncated directory path
    ImVec2 rMin = {0.0f, 50.0f};
    ImVec2 rMax = {750.0f, 74.0f};
    auto bCode = fizz::ui::getCategoryBadge("/home/user/workspace/fizz/services/SearchUi.cpp", ExecState::NotExecutable);
    auto rowLayout = fizz::ui::calculateRowLayout(rMin, rMax, "SearchUi.cpp", "/home/user/workspace/fizz/services", bCode);
    UI_TEST_ASSERT(rowLayout.hasBadge);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    fizz::ui::renderRowContent(dl, rowLayout, "SearchUi.cpp", bCode);

    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    UI_TEST_ASSERT(drawData != nullptr);
    UI_TEST_ASSERT(drawData->CmdListsCount > 0);

    ImGui::DestroyContext(ctx);

    std::cout << "[PASS] test_headless_ui_rendering passed.\n";
}

void run_search_ui_tests() {
    test_category_badges();
    test_badge_geometry();
    test_path_truncation_widths();
    test_row_layout_allocation_and_non_overlap();
    test_status_footer_formatting_and_shortcuts();
    test_search_pill_geometry_and_metrics();
    test_ui_glyph_ranges();
    test_headless_ui_rendering();
}
