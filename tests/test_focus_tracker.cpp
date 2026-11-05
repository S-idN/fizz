#include <iostream>
#include <cstdlib>
#include "services/FocusTracker.h"

#define FOCUS_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_focus_tracker_initial_state() {
    std::cout << "[TEST] Running test_focus_tracker_initial_state...\n";

    FocusTracker tracker;
    FOCUS_TEST_ASSERT(!tracker.isArmed());
    FOCUS_TEST_ASSERT(tracker.getAutoCloseOnUnfocus() == true);

    FocusTracker trackerDisabled(false);
    FOCUS_TEST_ASSERT(!trackerDisabled.isArmed());
    FOCUS_TEST_ASSERT(trackerDisabled.getAutoCloseOnUnfocus() == false);

    std::cout << "[PASS] test_focus_tracker_initial_state passed.\n";
}

void test_focus_tracker_wayland_premature_dismissal_prevention() {
    std::cout << "[TEST] Running test_focus_tracker_wayland_premature_dismissal_prevention...\n";

    FocusTracker tracker;
    // Simulating Wayland compositor initial mapping events before focus is acquired
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(false) == false);
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(0) == false);
    FOCUS_TEST_ASSERT(!tracker.isArmed());

    std::cout << "[PASS] test_focus_tracker_wayland_premature_dismissal_prevention passed.\n";
}

void test_focus_tracker_armed_dismissal() {
    std::cout << "[TEST] Running test_focus_tracker_armed_dismissal...\n";

    FocusTracker tracker;
    // Window receives initial focus
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(true) == false);
    FOCUS_TEST_ASSERT(tracker.isArmed());

    // Window receives redundant focus event while already focused
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(1) == false);
    FOCUS_TEST_ASSERT(tracker.isArmed());

    // Window loses focus while armed -> triggers dismissal
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(false) == true);
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(0) == true);

    std::cout << "[PASS] test_focus_tracker_armed_dismissal passed.\n";
}

void test_focus_tracker_disabled_toggle() {
    std::cout << "[TEST] Running test_focus_tracker_disabled_toggle...\n";

    FocusTracker tracker(false);
    // Unfocused initially does not dismiss
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(false) == false);

    // Initial focus arms tracker
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(true) == false);
    FOCUS_TEST_ASSERT(tracker.isArmed());

    // Focus loss while auto_close_on_unfocus is disabled does NOT dismiss
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(false) == false);
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(0) == false);

    // Enabling at runtime then losing focus triggers dismissal
    tracker.setAutoCloseOnUnfocus(true);
    FOCUS_TEST_ASSERT(tracker.getAutoCloseOnUnfocus() == true);
    FOCUS_TEST_ASSERT(tracker.handleFocusChange(false) == true);

    std::cout << "[PASS] test_focus_tracker_disabled_toggle passed.\n";
}

void run_focus_tracker_tests() {
    test_focus_tracker_initial_state();
    test_focus_tracker_wayland_premature_dismissal_prevention();
    test_focus_tracker_armed_dismissal();
    test_focus_tracker_disabled_toggle();
}

#ifdef FOCUS_TRACKER_TEST_MAIN
int main() {
    run_focus_tracker_tests();
    std::cout << "All focus tracker tests passed successfully.\n";
    return 0;
}
#endif
