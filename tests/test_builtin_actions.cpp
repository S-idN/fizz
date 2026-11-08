#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include "services/BuiltinActions.h"
#include "services/AppLauncher.h"
#include "services/SearchUi.h"
#include "services/Theme.h"
#include "services/ViewMode.h"

#define ACTION_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_builtin_action_query_matching() {
    std::cout << "[TEST] Running test_builtin_action_query_matching...\n";

    // 1. Settings reserved keywords
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("settings") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("fizz settings") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("fizz:settings") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery(":settings") == fizz::BuiltinActionType::Settings);

    // Case-insensitivity and surrounding whitespace for Settings
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("Settings") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("SETTINGS") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("  settings  ") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("Fizz Settings") == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("  fizz:settings  ") == fizz::BuiltinActionType::Settings);

    // 2. About reserved keywords
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("about") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("fizz about") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("fizz:about") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery(":about") == fizz::BuiltinActionType::About);

    // Case-insensitivity and surrounding whitespace for About
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("About") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("ABOUT") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("  about  ") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("Fizz About") == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("  fizz:about  ") == fizz::BuiltinActionType::About);

    // 3. Non-matching queries
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("fizz") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("set") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("sett") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("setting") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("ab") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("calculator") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("firefox") == fizz::BuiltinActionType::None);
    ACTION_TEST_ASSERT(fizz::matchBuiltinActionQuery("fizz:unknown") == fizz::BuiltinActionType::None);

    // isBuiltinActionQuery boolean helper
    ACTION_TEST_ASSERT(fizz::isBuiltinActionQuery("settings"));
    ACTION_TEST_ASSERT(fizz::isBuiltinActionQuery("about"));
    ACTION_TEST_ASSERT(fizz::isBuiltinActionQuery("fizz settings"));
    ACTION_TEST_ASSERT(!fizz::isBuiltinActionQuery("fizz"));
    ACTION_TEST_ASSERT(!fizz::isBuiltinActionQuery("something"));

    std::cout << "[PASS] test_builtin_action_query_matching passed.\n";
}

void test_builtin_action_hit_generation() {
    std::cout << "[TEST] Running test_builtin_action_hit_generation...\n";

    // 1. Settings hit generation
    auto settingsHits = fizz::generateBuiltinActionHits("settings");
    ACTION_TEST_ASSERT(settingsHits.size() == 1);
    ACTION_TEST_ASSERT(settingsHits[0].type == fizz::BuiltinActionType::Settings);
    ACTION_TEST_ASSERT(settingsHits[0].targetMode == ViewMode::Settings);
    ACTION_TEST_ASSERT(settingsHits[0].actionPath == fizz::kActionPathSettings);
    ACTION_TEST_ASSERT(settingsHits[0].title == fizz::kActionTitleSettings);
    ACTION_TEST_ASSERT(settingsHits[0].title == "⚙️ Fizz Settings");

    // Convert to SearchHit
    SearchHit settingsSearchHit = fizz::toSearchHit(settingsHits[0]);
    ACTION_TEST_ASSERT(settingsSearchHit.path == fizz::kActionPathSettings);
    ACTION_TEST_ASSERT(fizz::isBuiltinActionPath(settingsSearchHit.path));
    ACTION_TEST_ASSERT(fizz::isBuiltinAction(settingsSearchHit));
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTitle(settingsSearchHit.path) == "⚙️ Fizz Settings");
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTargetMode(settingsSearchHit.path) == ViewMode::Settings);

    // 2. About hit generation
    auto aboutHits = fizz::generateBuiltinActionHits("about");
    ACTION_TEST_ASSERT(aboutHits.size() == 1);
    ACTION_TEST_ASSERT(aboutHits[0].type == fizz::BuiltinActionType::About);
    ACTION_TEST_ASSERT(aboutHits[0].targetMode == ViewMode::About);
    ACTION_TEST_ASSERT(aboutHits[0].actionPath == fizz::kActionPathAbout);
    ACTION_TEST_ASSERT(aboutHits[0].title == fizz::kActionTitleAbout);
    ACTION_TEST_ASSERT(aboutHits[0].title == "ℹ️ About Fizz");

    // Convert to SearchHit
    SearchHit aboutSearchHit = fizz::toSearchHit(aboutHits[0]);
    ACTION_TEST_ASSERT(aboutSearchHit.path == fizz::kActionPathAbout);
    ACTION_TEST_ASSERT(fizz::isBuiltinActionPath(aboutSearchHit.path));
    ACTION_TEST_ASSERT(fizz::isBuiltinAction(aboutSearchHit));
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTitle(aboutSearchHit.path) == "ℹ️ About Fizz");
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTargetMode(aboutSearchHit.path) == ViewMode::About);

    // Shorthand path variations
    ACTION_TEST_ASSERT(fizz::isBuiltinActionPath(":settings"));
    ACTION_TEST_ASSERT(fizz::isBuiltinActionPath(":about"));
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTitle(":settings") == "⚙️ Fizz Settings");
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTitle(":about") == "ℹ️ About Fizz");

    // Non-action paths
    ACTION_TEST_ASSERT(!fizz::isBuiltinActionPath("/usr/bin/settings"));
    ACTION_TEST_ASSERT(!fizz::isBuiltinActionPath("/home/user/about.txt"));
    ACTION_TEST_ASSERT(!fizz::isBuiltinActionPath(""));
    ACTION_TEST_ASSERT(fizz::getBuiltinActionTitle("/usr/bin/bash").empty());
    ACTION_TEST_ASSERT(!fizz::getBuiltinActionTargetMode("/usr/bin/bash").has_value());

    // 3. Non-matching query returns empty hits
    ACTION_TEST_ASSERT(fizz::generateBuiltinActionHits("firefox").empty());
    ACTION_TEST_ASSERT(fizz::generateBuiltinActionHits("").empty());

    std::cout << "[PASS] test_builtin_action_hit_generation passed.\n";
}

void test_app_first_ranking_precedence() {
    std::cout << "[TEST] Running test_app_first_ranking_precedence...\n";

    // Scenario A: An installed Desktop Entry matching "settings" exists alongside regular files
    // In accordance with App-First discovery:
    // 1. Installed Desktop Entry must appear strictly ahead of Builtin Action Hit.
    // 2. Builtin Action Hit must appear strictly ahead of regular files.
    std::vector<SearchHit> inputA = {
        SearchHit{"/home/user/docs/settings.conf", ExecState::NotExecutable, 500},
        SearchHit{"/usr/share/applications/settings.desktop", ExecState::Executable, 450},
        SearchHit{"/home/user/notes/settings.txt", ExecState::NotExecutable, 400}
    };

    auto rankedA = fizz::rankHitsWithBuiltinActions("settings", inputA, 50);
    ACTION_TEST_ASSERT(rankedA.size() == 4);
    // Index 0: Desktop Entry strictly first
    ACTION_TEST_ASSERT(rankedA[0].path == "/usr/share/applications/settings.desktop");
    // Index 1: Builtin Action Hit
    ACTION_TEST_ASSERT(rankedA[1].path == fizz::kActionPathSettings);
    // Indices 2 & 3: Regular files
    ACTION_TEST_ASSERT(rankedA[2].path == "/home/user/docs/settings.conf");
    ACTION_TEST_ASSERT(rankedA[3].path == "/home/user/notes/settings.txt");

    // Scenario B: Multiple Desktop Entries matching query name
    // All matching Desktop Entries must rank ahead of the Builtin Action Hit
    std::vector<SearchHit> inputB = {
        SearchHit{"/usr/share/applications/org.gnome.Settings.desktop", ExecState::Executable, 600},
        SearchHit{"/home/user/settings.json", ExecState::NotExecutable, 500},
        SearchHit{"/usr/share/applications/xfce4-settings-manager.desktop", ExecState::Executable, 550}
    };

    auto rankedB = fizz::rankHitsWithBuiltinActions("fizz settings", inputB, 50);
    ACTION_TEST_ASSERT(rankedB.size() == 4);
    ACTION_TEST_ASSERT(rankedB[0].path == "/usr/share/applications/org.gnome.Settings.desktop");
    ACTION_TEST_ASSERT(rankedB[1].path == "/usr/share/applications/xfce4-settings-manager.desktop");
    ACTION_TEST_ASSERT(rankedB[2].path == fizz::kActionPathSettings);
    ACTION_TEST_ASSERT(rankedB[3].path == "/home/user/settings.json");

    // Scenario C: No Desktop Entries match the query (only regular files)
    // The Builtin Action Hit is prioritized at index 0 ahead of regular files
    std::vector<SearchHit> inputC = {
        SearchHit{"/etc/about.txt", ExecState::NotExecutable, 300},
        SearchHit{"/home/user/about_notes.md", ExecState::NotExecutable, 250}
    };

    auto rankedC = fizz::rankHitsWithBuiltinActions("about", inputC, 50);
    ACTION_TEST_ASSERT(rankedC.size() == 3);
    ACTION_TEST_ASSERT(rankedC[0].path == fizz::kActionPathAbout);
    ACTION_TEST_ASSERT(rankedC[1].path == "/etc/about.txt");
    ACTION_TEST_ASSERT(rankedC[2].path == "/home/user/about_notes.md");

    // Scenario D: Search result list is completely empty
    // The Builtin Action Hit is synthesized as the sole hit at index 0
    std::vector<SearchHit> inputD = {};
    auto rankedD = fizz::rankHitsWithBuiltinActions("fizz:settings", inputD, 50);
    ACTION_TEST_ASSERT(rankedD.size() == 1);
    ACTION_TEST_ASSERT(rankedD[0].path == fizz::kActionPathSettings);

    // Scenario E: Query does not match any reserved terms
    // No Builtin Action Hit is added, results remain unchanged
    std::vector<SearchHit> inputE = {
        SearchHit{"/usr/bin/grep", ExecState::Executable, 100},
        SearchHit{"/home/user/grep.txt", ExecState::NotExecutable, 80}
    };
    auto rankedE = fizz::rankHitsWithBuiltinActions("grep", inputE, 50);
    ACTION_TEST_ASSERT(rankedE.size() == 2);
    ACTION_TEST_ASSERT(rankedE[0].path == "/usr/bin/grep");
    ACTION_TEST_ASSERT(rankedE[1].path == "/home/user/grep.txt");

    // Scenario F: Truncation with limit
    auto rankedF = fizz::rankHitsWithBuiltinActions("settings", inputA, 2);
    ACTION_TEST_ASSERT(rankedF.size() == 2);
    ACTION_TEST_ASSERT(rankedF[0].path == "/usr/share/applications/settings.desktop");
    ACTION_TEST_ASSERT(rankedF[1].path == fizz::kActionPathSettings);

    // Scenario G: Unmatched desktop entries rank after Builtin Action Hit
    std::vector<SearchHit> inputG = {
        SearchHit{"/usr/share/applications/gimp.desktop", ExecState::Executable, 100},
        SearchHit{"/usr/share/applications/settings.desktop", ExecState::Executable, 200}
    };
    auto rankedG = fizz::rankHitsWithBuiltinActions("settings", inputG, 50);
    ACTION_TEST_ASSERT(rankedG.size() == 3);
    ACTION_TEST_ASSERT(rankedG[0].path == "/usr/share/applications/settings.desktop");
    ACTION_TEST_ASSERT(rankedG[1].path == fizz::kActionPathSettings);
    ACTION_TEST_ASSERT(rankedG[2].path == "/usr/share/applications/gimp.desktop");

    std::cout << "[PASS] test_app_first_ranking_precedence passed.\n";
}

void test_view_transition_triggering() {
    std::cout << "[TEST] Running test_view_transition_triggering...\n";

    ViewModeStateMachine sm(ViewMode::Search);
    std::string query = "settings";
    std::vector<SearchHit> results = {
        SearchHit{"fizz:settings", ExecState::NotExecutable, 1000}
    };
    char buf[32] = "settings";
    sm.setSearchStatePointers(&query, &results, buf);

    // 1. Executing Settings action transitions to ViewMode::Settings
    bool executedSettings = fizz::executeBuiltinAction("fizz:settings", sm);
    ACTION_TEST_ASSERT(executedSettings);
    ACTION_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Settings);
    // Pointers must be cleared on transition according to state machine rules
    ACTION_TEST_ASSERT(query.empty());
    ACTION_TEST_ASSERT(results.empty());
    ACTION_TEST_ASSERT(buf[0] == '\0');

    // Return to Search
    ACTION_TEST_ASSERT(sm.transitionTo(ViewMode::Search));
    ACTION_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Search);

    // 2. Executing About action transitions to ViewMode::About
    query = "about";
    results = {SearchHit{"fizz:about", ExecState::NotExecutable, 1000}};
    buf[0] = 'a'; buf[1] = '\0';

    bool executedAbout = fizz::executeBuiltinAction("fizz:about", sm);
    ACTION_TEST_ASSERT(executedAbout);
    ACTION_TEST_ASSERT(sm.getCurrentMode() == ViewMode::About);
    ACTION_TEST_ASSERT(query.empty());
    ACTION_TEST_ASSERT(results.empty());

    // 3. Executing a regular file path does NOT trigger transition
    ACTION_TEST_ASSERT(sm.transitionTo(ViewMode::Search));
    bool executedFile = fizz::executeBuiltinAction("/usr/bin/bash", sm);
    ACTION_TEST_ASSERT(!executedFile);
    ACTION_TEST_ASSERT(sm.getCurrentMode() == ViewMode::Search);

    std::cout << "[PASS] test_view_transition_triggering passed.\n";
}

void test_builtin_action_badge() {
    std::cout << "[TEST] Running test_builtin_action_badge...\n";

    // Category badge for builtin actions must return "action" with Palette::kTagAction
    auto bSettings = fizz::ui::getCategoryBadge("fizz:settings", ExecState::NotExecutable);
    ACTION_TEST_ASSERT(bSettings.label == "action");

    auto bAbout = fizz::ui::getCategoryBadge("fizz:about", ExecState::NotExecutable);
    ACTION_TEST_ASSERT(bAbout.label == "action");

    // Badge color must be distinct from normal categories
    auto bCode = fizz::ui::getCategoryBadge("main.cpp", ExecState::NotExecutable);
    auto bDoc  = fizz::ui::getCategoryBadge("doc.pdf", ExecState::NotExecutable);
    auto bExec = fizz::ui::getCategoryBadge("/bin/ls", ExecState::Executable);

    auto nearEqual = [](float a, float b) { return std::abs(a - b) < 0.001f; };
    auto colorEqual = [&](const ImVec4& c1, const ImVec4& c2) {
        return nearEqual(c1.x, c2.x) && nearEqual(c1.y, c2.y) && nearEqual(c1.z, c2.z);
    };

    ACTION_TEST_ASSERT(!colorEqual(bSettings.color, bCode.color));
    ACTION_TEST_ASSERT(!colorEqual(bSettings.color, bDoc.color));
    ACTION_TEST_ASSERT(!colorEqual(bSettings.color, bExec.color));

    std::cout << "[PASS] test_builtin_action_badge passed.\n";
}

void run_builtin_actions_tests() {
    test_builtin_action_query_matching();
    test_builtin_action_hit_generation();
    test_app_first_ranking_precedence();
    test_view_transition_triggering();
    test_builtin_action_badge();
}
