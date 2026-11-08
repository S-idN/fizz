#include <iostream>
#include <cstdlib>
#include <cassert>
#include <string>
#include "services/IndexingTab.h"

#define INDEXING_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_normalize_search_root() {
    std::cout << "[TEST] Running test_normalize_search_root...\n";
    const std::string fakeHome = "/home/alice";

    // Tilde expansion
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("~", fakeHome) == "/home/alice");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("~/", fakeHome) == "/home/alice");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("~/Projects", fakeHome) == "/home/alice/Projects");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("~/Projects/", fakeHome) == "/home/alice/Projects");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("~/Projects///", fakeHome) == "/home/alice/Projects");

    // Trailing slashes stripping
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("/var/log/", fakeHome) == "/var/log");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("/var/log///", fakeHome) == "/var/log");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("/var/log", fakeHome) == "/var/log");

    // Root slash preservation
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("/", fakeHome) == "/");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("///", fakeHome) == "/");

    // Whitespace trimming
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("  /etc/nginx/  ", fakeHome) == "/etc/nginx");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("  ~/src  ", fakeHome) == "/home/alice/src");

    // Empty or whitespace-only inputs
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("", fakeHome) == "");
    INDEXING_TEST_ASSERT(fizz::normalizeSearchRoot("   \t  ", fakeHome) == "");

    std::cout << "[PASS] test_normalize_search_root passed.\n";
}

void test_normalize_ignore_rule() {
    std::cout << "[TEST] Running test_normalize_ignore_rule...\n";

    // Basic patterns
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("node_modules") == "node_modules");
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("*.tmp") == "*.tmp");

    // Whitespace trimming
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("   build   ") == "build");

    // Quote stripping
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("\"target\"") == "target");
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("'dist'") == "dist");
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("  \"*.cache\"  ") == "*.cache");

    // Empty or whitespace-only inputs
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("") == "");
    INDEXING_TEST_ASSERT(fizz::normalizeIgnoreRule("   ") == "");

    std::cout << "[PASS] test_normalize_ignore_rule passed.\n";
}

#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

void test_search_roots_mutation_and_persistence() {
    std::cout << "[TEST] Running test_search_roots_mutation_and_persistence...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();

    FizzConfig config;
    config.searchRoots = {"/usr/share", "/opt"};
    config.ignoreRules = {"node_modules"};
    INDEXING_TEST_ASSERT(saveConfig(config, testConfigPath));

    const std::string fakeHome = "/home/bob";

    // 1. Add valid new search root with tilde and trailing slashes
    auto res = fizz::addSearchRoot(config, "  ~/Documents/Projects///  ", testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 3);
    INDEXING_TEST_ASSERT(config.searchRoots[2] == "/home/bob/Documents/Projects");

    // Verify atomic file persistence
    FizzConfig loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 3);
    INDEXING_TEST_ASSERT(loaded.searchRoots[2] == "/home/bob/Documents/Projects");

    // 2. Duplicate prevention (adding identical or normalized identical path)
    res = fizz::addSearchRoot(config, "~/Documents/Projects", testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Duplicate);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 3);

    res = fizz::addSearchRoot(config, "/usr/share/", testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Duplicate);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 3);

    // 3. Empty / whitespace-only input
    res = fizz::addSearchRoot(config, "    ", testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::EmptyInput);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 3);

    // 4. Remove existing search root
    res = fizz::removeSearchRoot(config, 1, testConfigPath); // removes "/opt"
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(config.searchRoots[0] == "/usr/share");
    INDEXING_TEST_ASSERT(config.searchRoots[1] == "/home/bob/Documents/Projects");

    // Verify persistence after removal
    loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(loaded.searchRoots[1] == "/home/bob/Documents/Projects");

    // 5. Remove out-of-bounds index
    res = fizz::removeSearchRoot(config, 99, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::IndexOutOfBounds);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);

    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_search_roots_mutation_and_persistence passed.\n";
}

void test_ignore_rules_mutation_and_persistence() {
    std::cout << "[TEST] Running test_ignore_rules_mutation_and_persistence...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_rules_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();

    FizzConfig config;
    config.searchRoots = {"/usr/share"};
    config.ignoreRules = {".git", "build"};
    INDEXING_TEST_ASSERT(saveConfig(config, testConfigPath));

    // 1. Add valid ignore rule with quotes and whitespace
    auto res = fizz::addIgnoreRule(config, "  \"node_modules\"  ", testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 3);
    INDEXING_TEST_ASSERT(config.ignoreRules[2] == "node_modules");

    // Verify persistence to file
    FizzConfig loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.ignoreRules.size() == 3);
    INDEXING_TEST_ASSERT(loaded.ignoreRules[2] == "node_modules");

    // 2. Duplicate prevention
    res = fizz::addIgnoreRule(config, "node_modules", testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Duplicate);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 3);

    res = fizz::addIgnoreRule(config, "  '.git'  ", testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Duplicate);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 3);

    // 3. Empty input
    res = fizz::addIgnoreRule(config, "   \t ", testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::EmptyInput);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 3);

    // 4. Remove rule
    res = fizz::removeIgnoreRule(config, 1, testConfigPath); // removes "build"
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 2);
    INDEXING_TEST_ASSERT(config.ignoreRules[0] == ".git");
    INDEXING_TEST_ASSERT(config.ignoreRules[1] == "node_modules");

    loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.ignoreRules.size() == 2);
    INDEXING_TEST_ASSERT(loaded.ignoreRules[1] == "node_modules");

    // 5. Remove out of bounds
    res = fizz::removeIgnoreRule(config, 50, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::IndexOutOfBounds);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 2);

    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_ignore_rules_mutation_and_persistence passed.\n";
}

void test_indexing_tab_state_and_actions() {
    std::cout << "[TEST] Running test_indexing_tab_state_and_actions...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_state_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();

    FizzConfig config;
    config.searchRoots = {"/usr/bin"};
    config.ignoreRules = {".cache"};
    INDEXING_TEST_ASSERT(saveConfig(config, testConfigPath));

    fizz::IndexingTab tab;

    // 1. Submit new root via buffer
    std::snprintf(tab.getState().newRootBuffer, sizeof(tab.getState().newRootBuffer), "/var/tmp");
    auto res = tab.submitNewRoot(config, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(tab.getState().newRootBuffer[0] == '\0');
    INDEXING_TEST_ASSERT(tab.getState().selectedRootIndex == 1);
    INDEXING_TEST_ASSERT(!tab.getState().isErrorMessage);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(config.searchRoots[1] == "/var/tmp");

    // 2. Duplicate root submission sets error state
    std::snprintf(tab.getState().newRootBuffer, sizeof(tab.getState().newRootBuffer), "/var/tmp");
    res = tab.submitNewRoot(config, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Duplicate);
    INDEXING_TEST_ASSERT(tab.getState().isErrorMessage);

    // 3. Remove selected root
    res = tab.removeSelectedRoot(config, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(tab.getState().selectedRootIndex == 0);

    // 4. Submit new ignore rule via buffer
    std::snprintf(tab.getState().newRuleBuffer, sizeof(tab.getState().newRuleBuffer), "target_dir");
    res = tab.submitNewRule(config, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(tab.getState().newRuleBuffer[0] == '\0');
    INDEXING_TEST_ASSERT(tab.getState().selectedRuleIndex == 1);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 2);
    INDEXING_TEST_ASSERT(config.ignoreRules[1] == "target_dir");

    // 5. Remove selected rule
    res = tab.removeSelectedRule(config, testConfigPath);
    INDEXING_TEST_ASSERT(res == fizz::MutationResult::Success);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 1);
    INDEXING_TEST_ASSERT(tab.getState().selectedRuleIndex == 0);

    // Verify persisted config on disk
    FizzConfig loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(loaded.searchRoots[0] == "/usr/bin");
    INDEXING_TEST_ASSERT(loaded.ignoreRules.size() == 1);
    INDEXING_TEST_ASSERT(loaded.ignoreRules[0] == ".cache");

    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_indexing_tab_state_and_actions passed.\n";
}

void test_headless_indexing_tab_rendering() {
    std::cout << "[TEST] Running test_headless_indexing_tab_rendering...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_ui_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();

    FizzConfig config;
    config.searchRoots = {"/home/alice/Documents", "/usr/local/bin"};
    config.ignoreRules = {".git", "node_modules", "build"};
    INDEXING_TEST_ASSERT(saveConfig(config, testConfigPath));

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
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);

    fizz::IndexingTab tab;

    // Frame 1: Normal tab rendering with preset buttons
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(config, testConfigPath);
    ImGui::End();
    ImGui::Render();

    // Frame 2: Preset confirmation modal open
    tab.requestApplyPreset(fizz::PresetType::Minimalist);
    INDEXING_TEST_ASSERT(tab.isPresetConfirmationOpen());
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(config, testConfigPath);
    ImGui::End();
    ImGui::Render();
    tab.cancelPresetConfirmation();

    // Frame 3: God Mode warning modal open
    tab.requestApplyPreset(fizz::PresetType::GodMode);
    INDEXING_TEST_ASSERT(tab.isGodModeWarningOpen());
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(config, testConfigPath);
    ImGui::End();
    ImGui::Render();
    tab.cancelPresetConfirmation();

    ImGui::DestroyContext(ctx);
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_headless_indexing_tab_rendering passed.\n";
}

void test_preset_profile_contents() {
    std::cout << "[TEST] Running test_preset_profile_contents...\n";
    const std::string fakeHome = "/home/alice";

    // 1. Minimalist Profile
    auto minProfile = fizz::getPresetProfile(fizz::PresetType::Minimalist, fakeHome);
    INDEXING_TEST_ASSERT(minProfile.name == "Minimalist");
    INDEXING_TEST_ASSERT(minProfile.searchRoots.size() == 7);
    INDEXING_TEST_ASSERT(minProfile.searchRoots[0] == "/home/alice/Documents");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[1] == "/home/alice/Desktop");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[2] == "/home/alice/Notes");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[3] == "/home/alice/Music");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[4] == "/home/alice/Pictures");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[5] == "/usr/share/applications");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[6] == "/home/alice/.local/share/applications");
    INDEXING_TEST_ASSERT(minProfile.ignoreRules.empty());

    // 2. Standard Desktop Profile
    auto stdProfile = fizz::getPresetProfile(fizz::PresetType::StandardDesktop, fakeHome);
    INDEXING_TEST_ASSERT(stdProfile.name == "Standard Desktop");
    INDEXING_TEST_ASSERT(stdProfile.searchRoots.size() == 3);
    INDEXING_TEST_ASSERT(stdProfile.searchRoots[0] == "/home/alice");
    INDEXING_TEST_ASSERT(stdProfile.searchRoots[1] == "/usr/share/applications");
    INDEXING_TEST_ASSERT(stdProfile.searchRoots[2] == "/home/alice/.local/share/applications");
    INDEXING_TEST_ASSERT(stdProfile.ignoreRules.size() == 1);
    INDEXING_TEST_ASSERT(stdProfile.ignoreRules[0] == ".cache");

    // 3. Developer Profile
    auto devProfile = fizz::getPresetProfile(fizz::PresetType::Developer, fakeHome);
    INDEXING_TEST_ASSERT(devProfile.name == "Developer");
    INDEXING_TEST_ASSERT(devProfile.searchRoots.size() == 6);
    INDEXING_TEST_ASSERT(devProfile.searchRoots[0] == "/home/alice");
    INDEXING_TEST_ASSERT(devProfile.searchRoots[1] == "/usr/share/applications");
    INDEXING_TEST_ASSERT(devProfile.searchRoots[2] == "/home/alice/.local/share/applications");
    INDEXING_TEST_ASSERT(devProfile.searchRoots[3] == "/home/alice/Projects");
    INDEXING_TEST_ASSERT(devProfile.searchRoots[4] == "/home/alice/Developer");
    INDEXING_TEST_ASSERT(devProfile.searchRoots[5] == "/home/alice/src");

    // Ignored: node_modules, target, build, .venv, .git, .pnpm-store, dist, bin, obj
    INDEXING_TEST_ASSERT(devProfile.ignoreRules.size() == 9);
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[0] == "node_modules");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[1] == "target");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[2] == "build");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[3] == ".venv");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[4] == ".git");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[5] == ".pnpm-store");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[6] == "dist");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[7] == "bin");
    INDEXING_TEST_ASSERT(devProfile.ignoreRules[8] == "obj");

    // 4. God Mode Profile
    auto godProfile = fizz::getPresetProfile(fizz::PresetType::GodMode, fakeHome);
    INDEXING_TEST_ASSERT(godProfile.name == "God Mode");
    INDEXING_TEST_ASSERT(godProfile.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(godProfile.searchRoots[0] == "/");
    INDEXING_TEST_ASSERT(godProfile.ignoreRules.empty());

    std::cout << "[PASS] test_preset_profile_contents passed.\n";
}

void test_preset_confirmation_gating_and_text() {
    std::cout << "[TEST] Running test_preset_confirmation_gating_and_text...\n";
    FizzConfig config;
    config.searchRoots = {"/home/alice/Documents", "/home/alice/Downloads"};
    config.ignoreRules = {"node_modules", ".git"};

    fizz::IndexingTab tab;

    // 1. Initial state: no modal open
    INDEXING_TEST_ASSERT(!tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(!tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(!tab.getPendingPreset().has_value());

    // 2. Unguarded confirmation attempt must be rejected
    bool res = tab.confirmPresetApplication(config);
    INDEXING_TEST_ASSERT(!res);
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 2);

    // 3. Request Minimalist preset
    tab.requestApplyPreset(fizz::PresetType::Minimalist);
    INDEXING_TEST_ASSERT(tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(!tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(tab.getPendingPreset().has_value());
    INDEXING_TEST_ASSERT(tab.getPendingPreset().value() == fizz::PresetType::Minimalist);

    // Verify confirmation message contains item counts and overwrite warning
    std::string msg = tab.getPresetConfirmationMessage(config);
    INDEXING_TEST_ASSERT(msg.find("2 Search Roots") != std::string::npos || msg.find("2 search roots") != std::string::npos);
    INDEXING_TEST_ASSERT(msg.find("2 Ignore Rules") != std::string::npos || msg.find("2 ignore rules") != std::string::npos);
    INDEXING_TEST_ASSERT(msg.find("Minimalist") != std::string::npos);
    INDEXING_TEST_ASSERT(msg.find("overwrite") != std::string::npos);

    // 4. Cancel confirmation
    tab.cancelPresetConfirmation();
    INDEXING_TEST_ASSERT(!tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(!tab.getPendingPreset().has_value());
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 2);

    // 5. Request Developer preset
    tab.requestApplyPreset(fizz::PresetType::Developer);
    INDEXING_TEST_ASSERT(tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(!tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(tab.getPendingPreset().value() == fizz::PresetType::Developer);
    std::string devMsg = tab.getPresetConfirmationMessage(config);
    INDEXING_TEST_ASSERT(devMsg.find("Developer") != std::string::npos);
    INDEXING_TEST_ASSERT(devMsg.find("2") != std::string::npos);

    std::cout << "[PASS] test_preset_confirmation_gating_and_text passed.\n";
}

void test_god_mode_warning_modal_gating_and_inotify_text() {
    std::cout << "[TEST] Running test_god_mode_warning_modal_gating_and_inotify_text...\n";
    FizzConfig config;
    config.searchRoots = {"/home/alice/Documents", "/usr/share"};
    config.ignoreRules = {"node_modules", "target", "build"};

    fizz::IndexingTab tab;

    // Initial state: not open
    INDEXING_TEST_ASSERT(!tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(!tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(!tab.getPendingPreset().has_value());

    // Request God Mode preset
    tab.requestApplyPreset(fizz::PresetType::GodMode);
    INDEXING_TEST_ASSERT(tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(!tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(tab.getPendingPreset().has_value());
    INDEXING_TEST_ASSERT(tab.getPendingPreset().value() == fizz::PresetType::GodMode);

    // Verify warning text content
    std::string warnMsg = tab.getGodModeWarningMessage(config);
    INDEXING_TEST_ASSERT(warnMsg.find("CPU") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("RAM") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("inotify") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("kernel inotify watch limits") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("exhaustion") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("fs.inotify.max_user_watches") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("2 Search Roots") != std::string::npos);
    INDEXING_TEST_ASSERT(warnMsg.find("3 Ignore Rules") != std::string::npos);

    // Cancel God Mode warning
    tab.cancelPresetConfirmation();
    INDEXING_TEST_ASSERT(!tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(!tab.getPendingPreset().has_value());
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 3);

    std::cout << "[PASS] test_god_mode_warning_modal_gating_and_inotify_text passed.\n";
}

void test_preset_application_and_atomic_persistence() {
    std::cout << "[TEST] Running test_preset_application_and_atomic_persistence...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_preset_persist_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();
    const std::string fakeHome = "/home/charlie";

    FizzConfig config;
    config.searchRoots = {"/custom/path1", "/custom/path2"};
    config.ignoreRules = {"my_custom_ignore"};
    INDEXING_TEST_ASSERT(saveConfig(config, testConfigPath));

    fizz::IndexingTab tab;

    // 1. Apply Developer preset
    tab.requestApplyPreset(fizz::PresetType::Developer);
    INDEXING_TEST_ASSERT(tab.isPresetConfirmationOpen());
    bool ok = tab.confirmPresetApplication(config, testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(ok);
    INDEXING_TEST_ASSERT(!tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 6);
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 9);

    // Verify atomic persistence to file
    FizzConfig loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 6);
    INDEXING_TEST_ASSERT(loaded.searchRoots[0] == "/home/charlie");
    INDEXING_TEST_ASSERT(loaded.searchRoots[3] == "/home/charlie/Projects");
    INDEXING_TEST_ASSERT(loaded.ignoreRules.size() == 9);
    INDEXING_TEST_ASSERT(loaded.ignoreRules[0] == "node_modules");
    INDEXING_TEST_ASSERT(loaded.ignoreRules[8] == "obj");

    // 2. Apply God Mode preset
    tab.requestApplyPreset(fizz::PresetType::GodMode);
    INDEXING_TEST_ASSERT(tab.isGodModeWarningOpen());
    ok = tab.confirmPresetApplication(config, testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(ok);
    INDEXING_TEST_ASSERT(!tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(config.searchRoots[0] == "/");
    INDEXING_TEST_ASSERT(config.ignoreRules.empty());

    // Verify atomic persistence for God Mode
    loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(loaded.searchRoots[0] == "/");
    INDEXING_TEST_ASSERT(loaded.ignoreRules.empty());

    // 3. Apply Minimalist preset
    tab.requestApplyPreset(fizz::PresetType::Minimalist);
    INDEXING_TEST_ASSERT(tab.isPresetConfirmationOpen());
    ok = tab.confirmPresetApplication(config, testConfigPath, fakeHome);
    INDEXING_TEST_ASSERT(ok);
    INDEXING_TEST_ASSERT(!tab.isPresetConfirmationOpen());
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 7);
    INDEXING_TEST_ASSERT(config.ignoreRules.empty());

    // Verify persistence for Minimalist
    loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 7);
    INDEXING_TEST_ASSERT(loaded.searchRoots[0] == "/home/charlie/Documents");
    INDEXING_TEST_ASSERT(loaded.ignoreRules.empty());

    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_preset_application_and_atomic_persistence passed.\n";
}

void run_indexing_tab_tests() {
    test_normalize_search_root();
    test_normalize_ignore_rule();
    test_preset_profile_contents();
    test_preset_confirmation_gating_and_text();
    test_god_mode_warning_modal_gating_and_inotify_text();
    test_preset_application_and_atomic_persistence();
    test_search_roots_mutation_and_persistence();
    test_ignore_rules_mutation_and_persistence();
    test_indexing_tab_state_and_actions();
    test_headless_indexing_tab_rendering();
}
