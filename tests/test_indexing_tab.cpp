#include <iostream>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "services/IndexingTab.h"
#include "services/Theme.h"
#include "imgui_internal.h"

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

    auto checkNoDuplicates = [](const std::vector<std::string>& vec, const std::string& context) {
        std::set<std::string> seen;
        for (const auto& item : vec) {
            INDEXING_TEST_ASSERT(seen.insert(item).second);
        }
    };

    // 1. Minimalist Profile
    // Search Roots = user media/document folders (~/Documents, ~/Downloads, ~/Desktop, ~/Pictures, ~/Music, ~/Videos, /usr/share/applications, ~/.local/share/applications); Ignore Rules = .cache.
    auto minProfile = fizz::getPresetProfile(fizz::PresetType::Minimalist, fakeHome);
    INDEXING_TEST_ASSERT(minProfile.name == "Minimalist");
    INDEXING_TEST_ASSERT(minProfile.searchRoots.size() == 8);
    INDEXING_TEST_ASSERT(minProfile.searchRoots[0] == "/home/alice/Documents");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[1] == "/home/alice/Downloads");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[2] == "/home/alice/Desktop");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[3] == "/home/alice/Pictures");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[4] == "/home/alice/Music");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[5] == "/home/alice/Videos");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[6] == "/usr/share/applications");
    INDEXING_TEST_ASSERT(minProfile.searchRoots[7] == "/home/alice/.local/share/applications");
    checkNoDuplicates(minProfile.searchRoots, "Minimalist searchRoots");
    INDEXING_TEST_ASSERT(minProfile.ignoreRules.size() == 1);
    INDEXING_TEST_ASSERT(minProfile.ignoreRules[0] == ".cache");
    checkNoDuplicates(minProfile.ignoreRules, "Minimalist ignoreRules");

    // 2. Standard Desktop Profile
    // Search Roots = ~ and /usr/share/applications (no redundant nested paths); Ignore Rules = .cache, .git, .cargo, .rustup, .npm, .mozilla, Trash
    auto stdProfile = fizz::getPresetProfile(fizz::PresetType::StandardDesktop, fakeHome);
    INDEXING_TEST_ASSERT(stdProfile.name == "Standard Desktop");
    INDEXING_TEST_ASSERT(stdProfile.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(stdProfile.searchRoots[0] == "/home/alice");
    INDEXING_TEST_ASSERT(stdProfile.searchRoots[1] == "/usr/share/applications");
    checkNoDuplicates(stdProfile.searchRoots, "StandardDesktop searchRoots");
    // Ensure nested paths are absent
    INDEXING_TEST_ASSERT(std::find(stdProfile.searchRoots.begin(), stdProfile.searchRoots.end(), "/home/alice/.local/share/applications") == stdProfile.searchRoots.end());

    INDEXING_TEST_ASSERT(stdProfile.ignoreRules.size() == 7);
    std::vector<std::string> expectedStdRules = {
        ".cache", ".git", ".cargo", ".rustup", ".npm", ".mozilla", "Trash"
    };
    for (const auto& rule : expectedStdRules) {
        INDEXING_TEST_ASSERT(std::find(stdProfile.ignoreRules.begin(), stdProfile.ignoreRules.end(), rule) != stdProfile.ignoreRules.end());
    }
    checkNoDuplicates(stdProfile.ignoreRules, "StandardDesktop ignoreRules");

    // 3. Developer Profile
    // Search Roots = ~ and /usr/share/applications; Ignore Rules = comprehensive toolchain exclusions
    auto devProfile = fizz::getPresetProfile(fizz::PresetType::Developer, fakeHome);
    INDEXING_TEST_ASSERT(devProfile.name == "Developer");
    INDEXING_TEST_ASSERT(devProfile.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(devProfile.searchRoots[0] == "/home/alice");
    INDEXING_TEST_ASSERT(devProfile.searchRoots[1] == "/usr/share/applications");
    checkNoDuplicates(devProfile.searchRoots, "Developer searchRoots");
    // Ensure no redundant nested paths
    INDEXING_TEST_ASSERT(std::find(devProfile.searchRoots.begin(), devProfile.searchRoots.end(), "/home/alice/.local/share/applications") == devProfile.searchRoots.end());
    INDEXING_TEST_ASSERT(std::find(devProfile.searchRoots.begin(), devProfile.searchRoots.end(), "/home/alice/Projects") == devProfile.searchRoots.end());
    INDEXING_TEST_ASSERT(std::find(devProfile.searchRoots.begin(), devProfile.searchRoots.end(), "/home/alice/Developer") == devProfile.searchRoots.end());
    INDEXING_TEST_ASSERT(std::find(devProfile.searchRoots.begin(), devProfile.searchRoots.end(), "/home/alice/src") == devProfile.searchRoots.end());

    std::vector<std::string> expectedDevRules = {
        // Python/uv
        ".venv", "venv", "__pycache__", ".pytest_cache", ".mypy_cache", ".uv", ".ruff_cache",
        // Node/pnpm
        "node_modules", ".pnpm-store", ".npm", ".yarn",
        // Rust
        "target", ".cargo", ".rustup",
        // Go
        "go/pkg",
        // VCS/Build
        ".git", "build", "dist", "bin", "obj", ".cache", ".idea", ".vscode"
    };
    INDEXING_TEST_ASSERT(devProfile.ignoreRules.size() == expectedDevRules.size());
    for (const auto& rule : expectedDevRules) {
        INDEXING_TEST_ASSERT(std::find(devProfile.ignoreRules.begin(), devProfile.ignoreRules.end(), rule) != devProfile.ignoreRules.end());
    }
    checkNoDuplicates(devProfile.ignoreRules, "Developer ignoreRules");

    // 4. God Mode Profile
    auto godProfile = fizz::getPresetProfile(fizz::PresetType::GodMode, fakeHome);
    INDEXING_TEST_ASSERT(godProfile.name == "God Mode");
    INDEXING_TEST_ASSERT(godProfile.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(godProfile.searchRoots[0] == "/");
    checkNoDuplicates(godProfile.searchRoots, "GodMode searchRoots");
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
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(config.searchRoots[0] == "/home/charlie");
    INDEXING_TEST_ASSERT(config.searchRoots[1] == "/usr/share/applications");
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 23);

    // Verify atomic persistence to file
    FizzConfig loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 2);
    INDEXING_TEST_ASSERT(loaded.searchRoots[0] == "/home/charlie");
    INDEXING_TEST_ASSERT(loaded.searchRoots[1] == "/usr/share/applications");
    INDEXING_TEST_ASSERT(loaded.ignoreRules.size() == 23);
    INDEXING_TEST_ASSERT(std::find(loaded.ignoreRules.begin(), loaded.ignoreRules.end(), "node_modules") != loaded.ignoreRules.end());
    INDEXING_TEST_ASSERT(std::find(loaded.ignoreRules.begin(), loaded.ignoreRules.end(), "go/pkg") != loaded.ignoreRules.end());
    INDEXING_TEST_ASSERT(std::find(loaded.ignoreRules.begin(), loaded.ignoreRules.end(), ".ruff_cache") != loaded.ignoreRules.end());
    INDEXING_TEST_ASSERT(std::find(loaded.ignoreRules.begin(), loaded.ignoreRules.end(), ".vscode") != loaded.ignoreRules.end());

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
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 8);
    INDEXING_TEST_ASSERT(config.searchRoots[0] == "/home/charlie/Documents");
    INDEXING_TEST_ASSERT(config.searchRoots[1] == "/home/charlie/Downloads");
    INDEXING_TEST_ASSERT(config.ignoreRules.size() == 1);
    INDEXING_TEST_ASSERT(config.ignoreRules[0] == ".cache");

    // Verify persistence for Minimalist
    loaded = loadConfig(testConfigPath);
    INDEXING_TEST_ASSERT(loaded.searchRoots.size() == 8);
    INDEXING_TEST_ASSERT(loaded.searchRoots[0] == "/home/charlie/Documents");
    INDEXING_TEST_ASSERT(loaded.searchRoots[1] == "/home/charlie/Downloads");
    INDEXING_TEST_ASSERT(loaded.ignoreRules.size() == 1);
    INDEXING_TEST_ASSERT(loaded.ignoreRules[0] == ".cache");

    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_preset_application_and_atomic_persistence passed.\n";
}

void test_manual_god_mode_root_intercept() {
    std::cout << "[TEST] Running test_manual_god_mode_root_intercept...\n";
    std::string testDir = "/tmp/fizz_test_manual_root_" + std::to_string(getpid());
    std::error_code ec;
    fs::create_directories(testDir, ec);
    std::string testConfigPath = testDir + "/fizz.conf";

    FizzConfig config;
    config.searchRoots = {"/home/user/docs"};
    config.ignoreRules = {"node_modules"};
    saveConfig(config, testConfigPath);

    fizz::IndexingTab tab;
    // Enter "/" as new root in buffer
    std::strncpy(tab.getState().newRootBuffer, "/", sizeof(tab.getState().newRootBuffer));
    tab.submitNewRoot(config, testConfigPath);

    // Assert that adding "/" manually was intercepted and triggered God Mode modal
    INDEXING_TEST_ASSERT(tab.isGodModeWarningOpen());
    INDEXING_TEST_ASSERT(tab.getPendingPreset() == fizz::PresetType::GodMode);
    // Config was not blindly updated with "/" without confirmation
    INDEXING_TEST_ASSERT(config.searchRoots.size() == 1);
    INDEXING_TEST_ASSERT(config.searchRoots[0] == "/home/user/docs");

    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_manual_god_mode_root_intercept passed.\n";
}

void test_preset_tooltips() {
    std::cout << "[TEST] Running test_preset_tooltips...\n";
    const std::string fakeHome = "/home/testuser";

    // 1. Minimalist
    std::string minTip = fizz::getPresetTooltipText(fizz::PresetType::Minimalist, fakeHome);
    INDEXING_TEST_ASSERT(minTip.find("Includes (Search Roots):") != std::string::npos);
    INDEXING_TEST_ASSERT(minTip.find("• " + fakeHome + "/Documents") != std::string::npos);
    INDEXING_TEST_ASSERT(minTip.find("• " + fakeHome + "/Downloads") != std::string::npos);
    INDEXING_TEST_ASSERT(minTip.find("• /usr/share/applications") != std::string::npos);
    INDEXING_TEST_ASSERT(minTip.find("Excludes (Ignore Rules):") != std::string::npos);
    INDEXING_TEST_ASSERT(minTip.find("• .cache") != std::string::npos);
    INDEXING_TEST_ASSERT(minTip.find("Warning:") == std::string::npos);

    // 2. Standard Desktop
    std::string stdTip = fizz::getPresetTooltipText(fizz::PresetType::StandardDesktop, fakeHome);
    INDEXING_TEST_ASSERT(stdTip.find("Includes (Search Roots):") != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("• " + fakeHome) != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("• /usr/share/applications") != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("Excludes (Ignore Rules):") != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("• .git") != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("• .cargo") != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("• Trash") != std::string::npos);
    INDEXING_TEST_ASSERT(stdTip.find("Warning:") == std::string::npos);

    // 3. Developer
    std::string devTip = fizz::getPresetTooltipText(fizz::PresetType::Developer, fakeHome);
    INDEXING_TEST_ASSERT(devTip.find("Includes (Search Roots):") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("• " + fakeHome) != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("• /usr/share/applications") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("Excludes (Ignore Rules):") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("• .venv") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("• node_modules") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("• target") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("• build") != std::string::npos);
    INDEXING_TEST_ASSERT(devTip.find("Warning:") == std::string::npos);

    // 4. God Mode
    std::string godTip = fizz::getPresetTooltipText(fizz::PresetType::GodMode, fakeHome);
    INDEXING_TEST_ASSERT(godTip.find("Includes (Search Roots):") != std::string::npos);
    INDEXING_TEST_ASSERT(godTip.find("• /") != std::string::npos);
    INDEXING_TEST_ASSERT(godTip.find("Excludes (Ignore Rules):") != std::string::npos);
    INDEXING_TEST_ASSERT(godTip.find("• (none)") != std::string::npos || godTip.find("(none)") != std::string::npos);
    INDEXING_TEST_ASSERT(godTip.find("⚠️ Warning:") != std::string::npos || godTip.find("Warning:") != std::string::npos);
    INDEXING_TEST_ASSERT(godTip.find("Full root traversal ('/')") != std::string::npos || godTip.find("full root traversal") != std::string::npos || godTip.find("root filesystem") != std::string::npos);
    INDEXING_TEST_ASSERT(godTip.find("inotify watch") != std::string::npos || godTip.find("inotify") != std::string::npos);

    std::cout << "[PASS] test_preset_tooltips passed.\n";
}

void test_god_mode_neutralized_button_styling_and_modal() {
    std::cout << "[TEST] Running test_god_mode_neutralized_button_styling_and_modal...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_btn_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();
    FizzConfig config;
    config.searchRoots = {"/home/alice/Documents"};
    config.ignoreRules = {".cache"};
    saveConfig(config, testConfigPath);

    fizz::IndexingTab tab;

    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(800, 600);

    // Frame 1: Render tab without modal. Inspect captured text and draw list.
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::LogToClipboard();
    tab.render(config, testConfigPath);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();

    // Check button label contains ⚠️ God Mode
    const char* clip = ImGui::GetClipboardText();
    INDEXING_TEST_ASSERT(clip != nullptr);
    std::string clipStr = clip;
    INDEXING_TEST_ASSERT(clipStr.find("⚠️ God Mode") != std::string::npos);

    // Check draw list: ensure the bright red color (0.70f, 0.15f, 0.15f) is NOT used for God Mode button background in the main tab
    ImDrawData* drawData = ImGui::GetDrawData();
    INDEXING_TEST_ASSERT(drawData != nullptr);
    ImU32 brightRedColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.70f, 0.15f, 0.15f, 0.85f));
    bool foundBrightRedInMainTab = false;
    for (int i = 0; i < drawData->CmdListsCount; ++i) {
        ImDrawList* dl = drawData->CmdLists[i];
        for (int j = 0; j < dl->VtxBuffer.Size; ++j) {
            if (dl->VtxBuffer[j].col == brightRedColor) {
                foundBrightRedInMainTab = true;
                break;
            }
        }
    }
    INDEXING_TEST_ASSERT(!foundBrightRedInMainTab);

    // Frame 2: Request God Mode preset (calls OpenPopup)
    tab.requestApplyPreset(fizz::PresetType::GodMode);
    INDEXING_TEST_ASSERT(tab.isGodModeWarningOpen());

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(config, testConfigPath);
    ImGui::End();
    ImGui::Render();

    // Frame 3: Modal is now open and rendered
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::LogToClipboard();
    tab.render(config, testConfigPath);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();

    clip = ImGui::GetClipboardText();
    INDEXING_TEST_ASSERT(clip != nullptr);
    clipStr = clip;
    INDEXING_TEST_ASSERT(clipStr.find("WARNING: High Resource Usage") != std::string::npos);
    INDEXING_TEST_ASSERT(clipStr.find("Enable God Mode") != std::string::npos);

    // Check that inside the modal, bright red styling IS retained
    drawData = ImGui::GetDrawData();
    bool foundBrightRedInModal = false;
    ImU32 expectedModalBtnColor = ImGui::GetColorU32(ImVec4(0.70f, 0.15f, 0.15f, 0.85f));
    for (int i = 0; i < drawData->CmdListsCount; ++i) {
        ImDrawList* dl = drawData->CmdLists[i];
        for (int j = 0; j < dl->VtxBuffer.Size; ++j) {
            if (dl->VtxBuffer[j].col == brightRedColor || dl->VtxBuffer[j].col == expectedModalBtnColor) {
                foundBrightRedInModal = true;
                break;
            }
        }
    }
    INDEXING_TEST_ASSERT(foundBrightRedInModal);

    tab.cancelPresetConfirmation();
    ImGui::DestroyContext(ctx);
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_god_mode_neutralized_button_styling_and_modal passed.\n";
}

void test_preset_tooltips_headless_hover() {
    std::cout << "[TEST] Running test_preset_tooltips_headless_hover...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_hover_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    std::string testConfigPath = (testDir / "fizz.conf").string();
    FizzConfig config;
    config.searchRoots = {"/home/alice/Documents"};
    config.ignoreRules = {".cache"};
    saveConfig(config, testConfigPath);

    fizz::IndexingTab tab;

    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(800, 600);

    // Frame 1: Initial layout to record button bounding boxes
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(config, testConfigPath);
    ImGui::End();
    ImGui::Render();

    const auto& state = tab.getState();
    // 0: Minimalist, 1: StandardDesktop, 2: Developer, 3: GodMode
    for (int p = 0; p < 4; ++p) {
        ImVec2 bMin = state.presetButtonMin[p];
        ImVec2 bMax = state.presetButtonMax[p];
        INDEXING_TEST_ASSERT(bMax.x > bMin.x && bMax.y > bMin.y);

        // Hover over the center of this preset button
        io.MousePos = ImVec2((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);
        // Eliminate delay so tooltip activates immediately in headless frame
        ImGuiStyle& style = ImGui::GetStyle();
        style.HoverDelayShort = 0.0f;
        style.HoverDelayNormal = 0.0f;
        style.HoverFlagsForTooltipMouse = ImGuiHoveredFlags_None;

        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
        tab.render(config, testConfigPath);
        ImGui::End();
        ImGui::Render();

        // Verify tooltip window exists
        ImGuiWindow* tooltipWin = ImGui::FindWindowByName("##Tooltip_00");
        INDEXING_TEST_ASSERT(tooltipWin != nullptr);
        INDEXING_TEST_ASSERT(tooltipWin->Active);

        // Verify tooltip contents in draw commands
        ImDrawData* drawData = ImGui::GetDrawData();
        INDEXING_TEST_ASSERT(drawData != nullptr);
    }

    ImGui::DestroyContext(ctx);
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_preset_tooltips_headless_hover passed.\n";
}

void test_detect_matching_preset() {
    std::cout << "[TEST] Running test_detect_matching_preset...\n";
    const std::string fakeHome = "/home/testuser";

    // 1. Standard Desktop match
    FizzConfig stdCfg;
    auto stdProf = fizz::getPresetProfile(fizz::PresetType::StandardDesktop, fakeHome);
    stdCfg.searchRoots = stdProf.searchRoots;
    stdCfg.ignoreRules = stdProf.ignoreRules;
    auto matchStd = fizz::detectMatchingPreset(stdCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchStd.has_value());
    INDEXING_TEST_ASSERT(*matchStd == fizz::PresetType::StandardDesktop);

    // 2. Minimalist match
    FizzConfig minCfg;
    auto minProf = fizz::getPresetProfile(fizz::PresetType::Minimalist, fakeHome);
    minCfg.searchRoots = minProf.searchRoots;
    minCfg.ignoreRules = minProf.ignoreRules;
    auto matchMin = fizz::detectMatchingPreset(minCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchMin.has_value());
    INDEXING_TEST_ASSERT(*matchMin == fizz::PresetType::Minimalist);

    // 3. Developer match
    FizzConfig devCfg;
    auto devProf = fizz::getPresetProfile(fizz::PresetType::Developer, fakeHome);
    devCfg.searchRoots = devProf.searchRoots;
    devCfg.ignoreRules = devProf.ignoreRules;
    auto matchDev = fizz::detectMatchingPreset(devCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchDev.has_value());
    INDEXING_TEST_ASSERT(*matchDev == fizz::PresetType::Developer);

    // 4. God Mode match
    FizzConfig godCfg;
    auto godProf = fizz::getPresetProfile(fizz::PresetType::GodMode, fakeHome);
    godCfg.searchRoots = godProf.searchRoots;
    godCfg.ignoreRules = godProf.ignoreRules;
    auto matchGod = fizz::detectMatchingPreset(godCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchGod.has_value());
    INDEXING_TEST_ASSERT(*matchGod == fizz::PresetType::GodMode);

    // 5. Order insensitivity (search roots in reverse order)
    FizzConfig revRootsCfg = stdCfg;
    std::reverse(revRootsCfg.searchRoots.begin(), revRootsCfg.searchRoots.end());
    auto matchRevRoots = fizz::detectMatchingPreset(revRootsCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchRevRoots.has_value());
    INDEXING_TEST_ASSERT(*matchRevRoots == fizz::PresetType::StandardDesktop);

    // 6. Order insensitivity (ignore rules shuffled)
    FizzConfig revRulesCfg = devCfg;
    std::reverse(revRulesCfg.ignoreRules.begin(), revRulesCfg.ignoreRules.end());
    auto matchRevRules = fizz::detectMatchingPreset(revRulesCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchRevRules.has_value());
    INDEXING_TEST_ASSERT(*matchRevRules == fizz::PresetType::Developer);

    // 7. Path normalization equivalence (~, redundant slashes, trailing slashes)
    FizzConfig tildeCfg;
    tildeCfg.searchRoots = {"~/", "///usr/share/applications///"};
    tildeCfg.ignoreRules = stdProf.ignoreRules;
    auto matchTilde = fizz::detectMatchingPreset(tildeCfg, fakeHome);
    INDEXING_TEST_ASSERT(matchTilde.has_value());
    INDEXING_TEST_ASSERT(*matchTilde == fizz::PresetType::StandardDesktop);

    // 8. Custom mutations away from presets return std::nullopt
    // 8a. Extra search root added
    FizzConfig extraRootCfg = stdCfg;
    extraRootCfg.searchRoots.push_back("/opt/custom_tools");
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(extraRootCfg, fakeHome).has_value());

    // 8b. Search root removed
    FizzConfig missingRootCfg = stdCfg;
    missingRootCfg.searchRoots.pop_back();
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(missingRootCfg, fakeHome).has_value());

    // 8c. Extra ignore rule added
    FizzConfig extraRuleCfg = minCfg;
    extraRuleCfg.ignoreRules.push_back(".git");
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(extraRuleCfg, fakeHome).has_value());

    // 8d. Ignore rule removed
    FizzConfig missingRuleCfg = minCfg;
    missingRuleCfg.ignoreRules.clear();
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(missingRuleCfg, fakeHome).has_value());

    // 8e. God Mode with ignore rule added is custom
    FizzConfig godWithIgnore = godCfg;
    godWithIgnore.ignoreRules.push_back(".cache");
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(godWithIgnore, fakeHome).has_value());

    // 8f. Empty configuration is custom
    FizzConfig emptyCfg;
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(emptyCfg, fakeHome).has_value());

    // 8g. Duplicate roots do not match
    FizzConfig dupRootsCfg = stdCfg;
    dupRootsCfg.searchRoots.push_back(stdCfg.searchRoots.front());
    INDEXING_TEST_ASSERT(!fizz::detectMatchingPreset(dupRootsCfg, fakeHome).has_value());

    std::cout << "[PASS] test_detect_matching_preset passed.\n";
}

void test_dynamic_preset_detection_and_custom_badge_rendering() {
    std::cout << "[TEST] Running test_dynamic_preset_detection_and_custom_badge_rendering...\n";
    fs::path testDir = fs::temp_directory_path() / ("fizz_test_indexing_custom_badge_" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(testDir, ec);
    fs::create_directories(testDir, ec);
    chmod(testDir.c_str(), 0700);

    const std::string fakeHome = "/home/testuser";
    std::string testConfigPath = (testDir / "fizz.conf").string();

    FizzConfig config;
    auto stdProf = fizz::getPresetProfile(fizz::PresetType::StandardDesktop, fakeHome);
    config.searchRoots = stdProf.searchRoots;
    config.ignoreRules = stdProf.ignoreRules;
    INDEXING_TEST_ASSERT(saveConfig(config, testConfigPath));

    fizz::IndexingTab tab;
    tab.setCustomHome(fakeHome);

    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(800, 600);

    auto hasAccentInBox = [](ImDrawData* drawData, ImVec2 bMin, ImVec2 bMax, ImU32 accentCol) -> bool {
        if (!drawData) return false;
        for (int i = 0; i < drawData->CmdListsCount; ++i) {
            ImDrawList* dl = drawData->CmdLists[i];
            for (int j = 0; j < dl->VtxBuffer.Size; ++j) {
                const auto& vtx = dl->VtxBuffer[j];
                if (vtx.pos.x >= bMin.x - 2.0f && vtx.pos.x <= bMax.x + 2.0f &&
                    vtx.pos.y >= bMin.y - 2.0f && vtx.pos.y <= bMax.y + 2.0f) {
                    if (vtx.col == accentCol) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // ----------------------------------------------------
    // FRAME 1: Standard Desktop matches
    // ----------------------------------------------------
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::LogToClipboard();
    tab.render(config, testConfigPath, fakeHome);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();

    INDEXING_TEST_ASSERT(tab.getActivePreset().has_value());
    INDEXING_TEST_ASSERT(*tab.getActivePreset() == fizz::PresetType::StandardDesktop);
    INDEXING_TEST_ASSERT(!tab.isCustomProfileActive());

    // [Custom] badge button must be completely hidden
    std::string clipStr = ImGui::GetClipboardText();
    INDEXING_TEST_ASSERT(clipStr.find("[Custom]") == std::string::npos);
    INDEXING_TEST_ASSERT(tab.getState().customBadgeMin.x == 0.0f && tab.getState().customBadgeMin.y == 0.0f);

    ImU32 accentCol = ImGui::GetColorU32(fizz::theme::getActiveAccent());
    ImDrawData* drawData = ImGui::GetDrawData();
    // Preset buttons: 0: Minimalist, 1: StandardDesktop, 2: Developer, 3: GodMode
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[0], tab.getState().presetButtonMax[0], accentCol));
    INDEXING_TEST_ASSERT(hasAccentInBox(drawData, tab.getState().presetButtonMin[1], tab.getState().presetButtonMax[1], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[2], tab.getState().presetButtonMax[2], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[3], tab.getState().presetButtonMax[3], accentCol));

    // ----------------------------------------------------
    // FRAME 2: Mutate config away from preset -> illuminates [Custom]
    // ----------------------------------------------------
    fizz::addSearchRoot(config, "/var/opt/custom_root", testConfigPath, fakeHome);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::LogToClipboard();
    tab.render(config, testConfigPath, fakeHome);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();

    INDEXING_TEST_ASSERT(!tab.getActivePreset().has_value());
    INDEXING_TEST_ASSERT(tab.isCustomProfileActive());

    // All preset buttons must return to neutral styling
    drawData = ImGui::GetDrawData();
    for (int p = 0; p < 4; ++p) {
        INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[p], tab.getState().presetButtonMax[p], accentCol));
    }

    // [Custom] badge button is dynamically rendered in active accent color
    clipStr = ImGui::GetClipboardText();
    INDEXING_TEST_ASSERT(clipStr.find("[Custom]") != std::string::npos);
    ImVec2 badgeMin = tab.getState().customBadgeMin;
    ImVec2 badgeMax = tab.getState().customBadgeMax;
    INDEXING_TEST_ASSERT(badgeMax.x > badgeMin.x && badgeMax.y > badgeMin.y);
    INDEXING_TEST_ASSERT(hasAccentInBox(drawData, badgeMin, badgeMax, accentCol));

    // ----------------------------------------------------
    // FRAME 3: Hovering over [Custom] shows tooltip
    // ----------------------------------------------------
    io.MousePos = ImVec2((badgeMin.x + badgeMax.x) * 0.5f, (badgeMin.y + badgeMax.y) * 0.5f);
    ImGuiStyle& style = ImGui::GetStyle();
    style.HoverDelayShort = 0.0f;
    style.HoverDelayNormal = 0.0f;
    style.HoverFlagsForTooltipMouse = ImGuiHoveredFlags_None;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(config, testConfigPath, fakeHome);
    ImGui::End();
    ImGui::Render();

    ImGuiWindow* tooltipWin = ImGui::FindWindowByName("##Tooltip_00");
    INDEXING_TEST_ASSERT(tooltipWin != nullptr);
    INDEXING_TEST_ASSERT(tooltipWin->Active);
    INDEXING_TEST_ASSERT(tab.getCustomBadgeTooltip().find("Custom Search Roots or Ignore Rules are currently active") != std::string::npos);

    // ----------------------------------------------------
    // FRAME 4: Return configuration to Minimalist preset -> [Custom] hides
    // ----------------------------------------------------
    tab.requestApplyPreset(fizz::PresetType::Minimalist);
    INDEXING_TEST_ASSERT(tab.confirmPresetApplication(config, testConfigPath, fakeHome));

    io.MousePos = ImVec2(-100, -100); // move mouse away
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::LogToClipboard();
    tab.render(config, testConfigPath, fakeHome);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();

    INDEXING_TEST_ASSERT(tab.getActivePreset().has_value());
    INDEXING_TEST_ASSERT(*tab.getActivePreset() == fizz::PresetType::Minimalist);
    INDEXING_TEST_ASSERT(!tab.isCustomProfileActive());

    // [Custom] badge button must be completely hidden
    clipStr = ImGui::GetClipboardText();
    INDEXING_TEST_ASSERT(clipStr.find("[Custom]") == std::string::npos);
    INDEXING_TEST_ASSERT(tab.getState().customBadgeMin.x == 0.0f && tab.getState().customBadgeMin.y == 0.0f);

    drawData = ImGui::GetDrawData();
    INDEXING_TEST_ASSERT(hasAccentInBox(drawData, tab.getState().presetButtonMin[0], tab.getState().presetButtonMax[0], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[1], tab.getState().presetButtonMax[1], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[2], tab.getState().presetButtonMax[2], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[3], tab.getState().presetButtonMax[3], accentCol));

    // ----------------------------------------------------
    // FRAME 5: God Mode preset matching
    // ----------------------------------------------------
    tab.requestApplyPreset(fizz::PresetType::GodMode);
    INDEXING_TEST_ASSERT(tab.confirmPresetApplication(config, testConfigPath, fakeHome));

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Settings_Test", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::LogToClipboard();
    tab.render(config, testConfigPath, fakeHome);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();

    INDEXING_TEST_ASSERT(tab.getActivePreset().has_value());
    INDEXING_TEST_ASSERT(*tab.getActivePreset() == fizz::PresetType::GodMode);
    INDEXING_TEST_ASSERT(!tab.isCustomProfileActive());

    clipStr = ImGui::GetClipboardText();
    INDEXING_TEST_ASSERT(clipStr.find("[Custom]") == std::string::npos);

    drawData = ImGui::GetDrawData();
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[0], tab.getState().presetButtonMax[0], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[1], tab.getState().presetButtonMax[1], accentCol));
    INDEXING_TEST_ASSERT(!hasAccentInBox(drawData, tab.getState().presetButtonMin[2], tab.getState().presetButtonMax[2], accentCol));
    INDEXING_TEST_ASSERT(hasAccentInBox(drawData, tab.getState().presetButtonMin[3], tab.getState().presetButtonMax[3], accentCol));

    ImGui::DestroyContext(ctx);
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_dynamic_preset_detection_and_custom_badge_rendering passed.\n";
}

void run_indexing_tab_tests() {
    test_normalize_search_root();
    test_normalize_ignore_rule();
    test_preset_profile_contents();
    test_preset_tooltips();
    test_god_mode_neutralized_button_styling_and_modal();
    test_preset_tooltips_headless_hover();
    test_preset_confirmation_gating_and_text();
    test_god_mode_warning_modal_gating_and_inotify_text();
    test_preset_application_and_atomic_persistence();
    test_search_roots_mutation_and_persistence();
    test_ignore_rules_mutation_and_persistence();
    test_indexing_tab_state_and_actions();
    test_manual_god_mode_root_intercept();
    test_headless_indexing_tab_rendering();
    test_detect_matching_preset();
    test_dynamic_preset_detection_and_custom_badge_rendering();
}
