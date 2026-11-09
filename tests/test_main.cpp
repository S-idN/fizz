#include <iostream>
#include <string>

void run_app_launcher_tests();
void run_focus_tracker_tests();
void run_font_discovery_tests();
void run_config_tests();
void run_binary_cache_tests();
void run_theme_tests();
void run_search_ui_tests();
void run_file_cache_builder_tests();

int main(int argc, char* argv[]) {
    std::string suite = (argc > 1) ? argv[1] : "";

    auto run_if = [&](const std::string& name, void (*fn)()) {
        if (suite.empty() || suite == name) {
            fn();
        }
    };

    run_if("app_launcher", run_app_launcher_tests);
    run_if("focus_tracker", run_focus_tracker_tests);
    run_if("font_discovery", run_font_discovery_tests);
    run_if("config", run_config_tests);
    run_if("binary_cache", run_binary_cache_tests);
    run_if("theme", run_theme_tests);
    run_if("search_ui", run_search_ui_tests);
    run_if("file_cache_builder", run_file_cache_builder_tests);
    run_if("index_daemon", run_file_cache_builder_tests);

    std::cout << "All tests passed successfully.\n";
    return 0;
}

