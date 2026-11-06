#include <iostream>

void run_app_launcher_tests();
void run_focus_tracker_tests();
void run_font_discovery_tests();
void run_config_tests();
void run_binary_cache_tests();
void run_theme_tests();

int main() {
    run_app_launcher_tests();
    run_focus_tracker_tests();
    run_font_discovery_tests();
    run_config_tests();
    run_binary_cache_tests();
    run_theme_tests();

    std::cout << "All tests passed successfully.\n";
    return 0;
}
