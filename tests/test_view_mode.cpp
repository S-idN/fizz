#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>
#include "services/ViewMode.h"
#include "services/FileSearch.h"
#include "services/QueryOps.h"
#include "services/SearchUi.h"
#include "services/Theme.h"

#define VIEW_MODE_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_view_mode_string_conversions() {
    std::cout << "[TEST] Running test_view_mode_string_conversions...\n";

    // viewModeToString conversions
    VIEW_MODE_TEST_ASSERT(std::string_view(viewModeToString(ViewMode::Search)) == "Search");
    VIEW_MODE_TEST_ASSERT(std::string_view(viewModeToString(ViewMode::Settings)) == "Settings");
    VIEW_MODE_TEST_ASSERT(std::string_view(viewModeToString(ViewMode::About)) == "About");

    // stringToViewMode exact conversions
    VIEW_MODE_TEST_ASSERT(stringToViewMode("Search") == ViewMode::Search);
    VIEW_MODE_TEST_ASSERT(stringToViewMode("Settings") == ViewMode::Settings);
    VIEW_MODE_TEST_ASSERT(stringToViewMode("About") == ViewMode::About);

    // Case-insensitive conversions
    VIEW_MODE_TEST_ASSERT(stringToViewMode("search") == ViewMode::Search);
    VIEW_MODE_TEST_ASSERT(stringToViewMode("settings") == ViewMode::Settings);
    VIEW_MODE_TEST_ASSERT(stringToViewMode("about") == ViewMode::About);

    // Unknown string fallback to Search
    VIEW_MODE_TEST_ASSERT(stringToViewMode("") == ViewMode::Search);
    VIEW_MODE_TEST_ASSERT(stringToViewMode("unknown") == ViewMode::Search);
    VIEW_MODE_TEST_ASSERT(stringToViewMode("foo_bar") == ViewMode::Search);

    std::cout << "[PASS] test_view_mode_string_conversions passed.\n";
}

void test_view_mode_state_transitions() {
    std::cout << "[TEST] Running test_view_mode_state_transitions...\n";

    ViewModeStateMachine sm;
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Search);

    // Search -> Settings
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Settings) == true);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Settings);

    // Redundant transition Settings -> Settings is a no-op returning false
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Settings) == false);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Settings);

    // Settings -> Search
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Search) == true);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Search);

    // Search -> About
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::About) == true);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::About);

    // About -> Search
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Search) == true);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Search);

    // Transition between non-Search modes: Settings <-> About
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Settings) == true);
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::About) == true);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::About);
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Settings) == true);
    VIEW_MODE_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Settings);

    // Explicit initial mode constructor
    ViewModeStateMachine smSettings(ViewMode::Settings);
    VIEW_MODE_TEST_ASSERT(smSettings.getCurrentMode() == ViewMode::Settings);

    std::cout << "[PASS] test_view_mode_state_transitions passed.\n";
}

void test_escape_key_handling() {
    std::cout << "[TEST] Running test_escape_key_handling...\n";

    // 1. In Settings mode: Escape transitions back to Search without exiting
    ViewModeStateMachine smSettings(ViewMode::Settings);
    bool shouldExitFromSettings = smSettings.handleEscape();
    VIEW_MODE_TEST_ASSERT(!shouldExitFromSettings);
    VIEW_MODE_TEST_ASSERT(smSettings.getCurrentMode() == ViewMode::Search);

    // 2. In About mode: Escape transitions back to Search without exiting
    ViewModeStateMachine smAbout(ViewMode::About);
    bool shouldExitFromAbout = smAbout.handleEscape();
    VIEW_MODE_TEST_ASSERT(!shouldExitFromAbout);
    VIEW_MODE_TEST_ASSERT(smAbout.getCurrentMode() == ViewMode::Search);

    // 3. In Search mode: Escape signals application exit without changing mode
    ViewModeStateMachine smSearch(ViewMode::Search);
    bool shouldExitFromSearch = smSearch.handleEscape();
    VIEW_MODE_TEST_ASSERT(shouldExitFromSearch);
    VIEW_MODE_TEST_ASSERT(smSearch.getCurrentMode() == ViewMode::Search);

    std::cout << "[PASS] test_escape_key_handling passed.\n";
}

void test_worker_cancellation_and_result_clearing() {
    std::cout << "[TEST] Running test_worker_cancellation_and_result_clearing...\n";

    setSearchInterrupted(false);
    VIEW_MODE_TEST_ASSERT(!isSearchInterrupted());

    ViewModeStateMachine sm(ViewMode::Search);
    std::string query = "fizz";
    std::vector<SearchHit> results;
    results.push_back(SearchHit{"/usr/bin/fizz", ExecState::Executable, 100});
    char buf[64] = "fizz";
    bool cancelCallbackInvoked = false;

    sm.setOnCancelSearch([&]() {
        cancelCallbackInvoked = true;
    });
    sm.setSearchStatePointers(&query, &results, buf);

    // Transitioning Search -> Settings must interrupt workers and clear results & query state
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Settings) == true);
    VIEW_MODE_TEST_ASSERT(isSearchInterrupted() == true);
    VIEW_MODE_TEST_ASSERT(cancelCallbackInvoked == true);
    VIEW_MODE_TEST_ASSERT(query.empty());
    VIEW_MODE_TEST_ASSERT(results.empty());
    VIEW_MODE_TEST_ASSERT(buf[0] == '\0');

    // Transitioning back to Search un-interrupts search workers for subsequent queries
    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::Search) == true);
    VIEW_MODE_TEST_ASSERT(isSearchInterrupted() == false);

    // Transitioning Search -> About must also interrupt workers and clear state
    query = "app";
    results.push_back(SearchHit{"/usr/bin/app", ExecState::Executable, 50});
    buf[0] = 'a'; buf[1] = '\0';
    cancelCallbackInvoked = false;

    VIEW_MODE_TEST_ASSERT(sm.transitionTo(ViewMode::About) == true);
    VIEW_MODE_TEST_ASSERT(isSearchInterrupted() == true);
    VIEW_MODE_TEST_ASSERT(cancelCallbackInvoked == true);
    VIEW_MODE_TEST_ASSERT(query.empty());
    VIEW_MODE_TEST_ASSERT(results.empty());
    VIEW_MODE_TEST_ASSERT(buf[0] == '\0');

    std::cout << "[PASS] test_worker_cancellation_and_result_clearing passed.\n";
}

static bool nearEqual(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) <= eps;
}

void test_header_layout_geometry() {
    std::cout << "[TEST] Running test_header_layout_geometry...\n";

    auto measureText = [](std::string_view s) -> float {
        return static_cast<float>(s.size()) * 10.0f;
    };

    ImVec2 origin = {0.0f, 0.0f};
    float width = 750.0f;
    std::string_view title = "Settings";
    float headerHeight = 38.0f;
    float fontSize = 16.0f;

    auto geom = fizz::ui::calculateViewHeaderLayout(origin, width, title, measureText, headerHeight, fontSize);

    // Header container bounds
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.headerMin.x, 0.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.headerMin.y, 0.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.headerMax.x, 750.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.headerMax.y, 38.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.headerHeight, 38.0f));

    // "← Back to Search" button geometry
    float backTextW = measureText(fizz::ui::kBackToSearchLabel);
    float expectedBtnW = backTextW + 2.0f * fizz::ui::Metrics::kBackBtnPaddingX;
    float expectedBtnH = fontSize + 2.0f * fizz::ui::Metrics::kBackBtnPaddingY;

    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonWidth, expectedBtnW));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonHeight, expectedBtnH));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonMin.x, fizz::ui::Metrics::kHeaderPaddingX));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonMax.x, geom.backButtonMin.x + expectedBtnW));

    // Back button is vertically centered within header
    float expectedBtnY = (headerHeight - expectedBtnH) * 0.5f;
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonMin.y, expectedBtnY));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonMax.y, expectedBtnY + expectedBtnH));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonMin.y - geom.headerMin.y, geom.headerMax.y - geom.backButtonMax.y));

    // Back button text position is inset by padding
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonTextPos.x, geom.backButtonMin.x + fizz::ui::Metrics::kBackBtnPaddingX));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.backButtonTextPos.y, geom.backButtonMin.y + fizz::ui::Metrics::kBackBtnPaddingY));

    // Title position is strictly to the right of back button (no overlap)
    VIEW_MODE_TEST_ASSERT(geom.titlePos.x >= geom.backButtonMax.x + fizz::ui::Metrics::kHeaderTitleSpacing);
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.titleWidth, measureText(title)));
    float expectedTitleY = (headerHeight - fontSize) * 0.5f;
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.titlePos.y, expectedTitleY));

    // Separator line is at header bottom
    VIEW_MODE_TEST_ASSERT(nearEqual(geom.separatorY, geom.headerMax.y));

    // Test with non-zero origin and custom dimensions
    ImVec2 origin2 = {50.0f, 100.0f};
    float width2 = 600.0f;
    std::string_view title2 = "About Fizz";
    auto geom2 = fizz::ui::calculateViewHeaderLayout(origin2, width2, title2, measureText, 44.0f, 18.0f);

    VIEW_MODE_TEST_ASSERT(nearEqual(geom2.headerMin.x, 50.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom2.headerMin.y, 100.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom2.headerMax.x, 650.0f));
    VIEW_MODE_TEST_ASSERT(nearEqual(geom2.headerMax.y, 144.0f));
    VIEW_MODE_TEST_ASSERT(geom2.backButtonMax.x < geom2.titlePos.x);
    VIEW_MODE_TEST_ASSERT(nearEqual(geom2.separatorY, 144.0f));

    std::cout << "[PASS] test_header_layout_geometry passed.\n";
}

void run_view_mode_tests() {
    test_view_mode_string_conversions();
    test_view_mode_state_transitions();
    test_escape_key_handling();
    test_worker_cancellation_and_result_clearing();
    test_header_layout_geometry();
}
