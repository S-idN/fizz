#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "services/GeneralTab.h"
#include "services/Config.h"
#include "imgui.h"

namespace fs = std::filesystem;

#define GENERAL_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

static std::string createTempTestDir(const std::string& prefix) {
    std::string pattern = "/tmp/fizz_test_" + prefix + "_XXXXXX";
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", pattern.c_str());
    char* dir = mkdtemp(buf);
    GENERAL_TEST_ASSERT(dir != nullptr);
    return std::string(dir);
}

void test_general_tab_auto_close_mutation() {
    std::cout << "[TEST] Running test_general_tab_auto_close_mutation...\n";

    std::string testDir = createTempTestDir("autoclose");
    std::string confPath = testDir + "/fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    GENERAL_TEST_ASSERT(cfg.autoCloseOnUnfocus == true);
    GENERAL_TEST_ASSERT(saveConfig(cfg, confPath));

    GeneralTab tab(confPath);
    GENERAL_TEST_ASSERT(tab.getConfigPath() == confPath);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    bool callbackInvoked = false;
    bool lastCallbackVal = false;
    tab.setOnConfigChanged([&](const FizzConfig& c) {
        callbackInvoked = true;
        lastCallbackVal = c.autoCloseOnUnfocus;
    });

    // Mutate to false
    bool ok = tab.setAutoClose(cfg, false);
    GENERAL_TEST_ASSERT(ok);
    GENERAL_TEST_ASSERT(cfg.autoCloseOnUnfocus == false);
    GENERAL_TEST_ASSERT(callbackInvoked);
    GENERAL_TEST_ASSERT(lastCallbackVal == false);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 1);

    // Verify persisted file on disk
    FizzConfig loaded = loadConfig(confPath);
    GENERAL_TEST_ASSERT(loaded.autoCloseOnUnfocus == false);

    // Mutate back to true
    callbackInvoked = false;
    ok = tab.setAutoClose(cfg, true);
    GENERAL_TEST_ASSERT(ok);
    GENERAL_TEST_ASSERT(cfg.autoCloseOnUnfocus == true);
    GENERAL_TEST_ASSERT(callbackInvoked);
    GENERAL_TEST_ASSERT(lastCallbackVal == true);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 2);

    loaded = loadConfig(confPath);
    GENERAL_TEST_ASSERT(loaded.autoCloseOnUnfocus == true);

    std::error_code ec;
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_general_tab_auto_close_mutation passed.\n";
}

void test_general_tab_grace_period_clamping_and_live_mutation() {
    std::cout << "[TEST] Running test_general_tab_grace_period_clamping_and_live_mutation...\n";

    std::string testDir = createTempTestDir("grace_clamp");
    std::string confPath = testDir + "/fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    saveConfig(cfg, confPath);

    GeneralTab tab(confPath);
    int callbackVal = -1;
    tab.setOnConfigChanged([&](const FizzConfig& c) {
        callbackVal = c.view_state_grace_seconds;
    });

    // Valid range mutations
    tab.setGracePeriod(cfg, 10);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 10);
    GENERAL_TEST_ASSERT(callbackVal == 10);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0); // debounced: no disk write yet

    tab.setGracePeriod(cfg, 1);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 1);
    GENERAL_TEST_ASSERT(callbackVal == 1);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    tab.setGracePeriod(cfg, 15);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 15);
    GENERAL_TEST_ASSERT(callbackVal == 15);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    // Below minimum clamping (< 1 -> 1)
    tab.setGracePeriod(cfg, 0);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 1);
    GENERAL_TEST_ASSERT(callbackVal == 1);

    tab.setGracePeriod(cfg, -42);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 1);
    GENERAL_TEST_ASSERT(callbackVal == 1);

    // Above maximum clamping (> 15 -> 15)
    tab.setGracePeriod(cfg, 16);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 15);
    GENERAL_TEST_ASSERT(callbackVal == 15);

    tab.setGracePeriod(cfg, 100);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 15);
    GENERAL_TEST_ASSERT(callbackVal == 15);

    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    std::error_code ec;
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_general_tab_grace_period_clamping_and_live_mutation passed.\n";
}

void test_general_tab_grace_period_debounce() {
    std::cout << "[TEST] Running test_general_tab_grace_period_debounce...\n";

    using namespace std::chrono_literals;

    std::string testDir = createTempTestDir("grace_debounce");
    std::string confPath = testDir + "/fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    saveConfig(cfg, confPath);

    GeneralTab tab(confPath, 500ms);
    GENERAL_TEST_ASSERT(!tab.hasPendingSave());
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    auto t0 = std::chrono::steady_clock::time_point(1000s);

    // Initial slider drag at t0
    tab.setGracePeriod(cfg, 6, t0);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 6);
    GENERAL_TEST_ASSERT(tab.hasPendingSave());
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0); // Not written yet

    // Continuous slider adjustments within debounce window
    tab.setGracePeriod(cfg, 7, t0 + 100ms);
    tab.setGracePeriod(cfg, 8, t0 + 200ms);
    tab.setGracePeriod(cfg, 9, t0 + 300ms);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 9);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    // 100ms after last change (t0 + 400ms) -> should NOT persist
    bool didSave = tab.update(cfg, t0 + 400ms);
    GENERAL_TEST_ASSERT(!didSave);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    // 499ms after last change (t0 + 799ms) -> should NOT persist (< 500ms inactivity)
    didSave = tab.update(cfg, t0 + 799ms);
    GENERAL_TEST_ASSERT(!didSave);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    // On-disk config still has default (5)
    FizzConfig loaded = loadConfig(confPath);
    GENERAL_TEST_ASSERT(loaded.view_state_grace_seconds == 5);

    // 501ms after last change (t0 + 801ms) -> 500ms inactivity reached -> persists!
    didSave = tab.update(cfg, t0 + 801ms);
    GENERAL_TEST_ASSERT(didSave);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 1);
    GENERAL_TEST_ASSERT(!tab.hasPendingSave());

    // File on disk now reflects grace period 9
    loaded = loadConfig(confPath);
    GENERAL_TEST_ASSERT(loaded.view_state_grace_seconds == 9);

    // Subsequent update with no changes -> no writes
    didSave = tab.update(cfg, t0 + 1500ms);
    GENERAL_TEST_ASSERT(!didSave);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 1);

    // Another slider drag at t0 + 2000ms
    tab.setGracePeriod(cfg, 12, t0 + 2000ms);
    GENERAL_TEST_ASSERT(tab.hasPendingSave());
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 1);

    // Immediate flush (e.g. app exit or tab switch)
    bool flushed = tab.flushPendingSave(cfg);
    GENERAL_TEST_ASSERT(flushed);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 2);
    GENERAL_TEST_ASSERT(!tab.hasPendingSave());

    loaded = loadConfig(confPath);
    GENERAL_TEST_ASSERT(loaded.view_state_grace_seconds == 12);

    // Second flush is no-op
    GENERAL_TEST_ASSERT(!tab.flushPendingSave(cfg));
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 2);

    std::error_code ec;
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_general_tab_grace_period_debounce passed.\n";
}

void test_general_tab_reset_confirmation_gating_and_wording() {
    std::cout << "[TEST] Running test_general_tab_reset_confirmation_gating_and_wording...\n";

    std::string testDir = createTempTestDir("reset_gating");
    std::string confPath = testDir + "/fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    cfg.autoCloseOnUnfocus = false;
    cfg.view_state_grace_seconds = 11;
    saveConfig(cfg, confPath);

    GeneralTab tab(confPath);

    // Exact wording verification
    const std::string expectedWording =
        "Resets all configuration options to defaults. Does not affect launch history, search index, or cached data.";
    GENERAL_TEST_ASSERT(GeneralTab::kResetConfirmationText == expectedWording);
    GENERAL_TEST_ASSERT(tab.getResetConfirmationMessage() == expectedWording);

    // Initial state: reset modal is closed
    GENERAL_TEST_ASSERT(!tab.isResetConfirmationOpen());

    // Unguarded reset attempt must fail without modifying config or saving to disk
    bool resetAttempt = tab.confirmReset(cfg);
    GENERAL_TEST_ASSERT(!resetAttempt);
    GENERAL_TEST_ASSERT(!cfg.autoCloseOnUnfocus);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 11);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    // Request reset -> opens confirmation
    tab.requestReset();
    GENERAL_TEST_ASSERT(tab.isResetConfirmationOpen());
    // Config still untouched before confirmation
    GENERAL_TEST_ASSERT(!cfg.autoCloseOnUnfocus);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    // Cancel reset -> closes confirmation without changing config
    tab.cancelReset();
    GENERAL_TEST_ASSERT(!tab.isResetConfirmationOpen());
    GENERAL_TEST_ASSERT(!cfg.autoCloseOnUnfocus);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == 11);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 0);

    std::error_code ec;
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_general_tab_reset_confirmation_gating_and_wording passed.\n";
}

void test_general_tab_factory_defaults_restoration() {
    std::cout << "[TEST] Running test_general_tab_factory_defaults_restoration...\n";

    std::string testDir = createTempTestDir("reset_restore");
    std::string confPath = testDir + "/fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    cfg.autoCloseOnUnfocus = false;
    cfg.view_state_grace_seconds = 14;
    cfg.theme = "light";
    cfg.accent = "crimson";
    cfg.window_width = 1200;
    cfg.window_height = 800;
    cfg.searchRoots = {"/tmp/custom_root_1", "/tmp/custom_root_2"};
    cfg.ignoreRules = {"*.tmp", "*.bak"};
    saveConfig(cfg, confPath);

    GeneralTab tab(confPath);

    bool onResetCalled = false;
    FizzConfig resetCfgCallback;
    tab.setOnReset([&](const FizzConfig& c) {
        onResetCalled = true;
        resetCfgCallback = c;
    });

    // Request and confirm reset
    tab.requestReset();
    GENERAL_TEST_ASSERT(tab.isResetConfirmationOpen());

    bool ok = tab.confirmReset(cfg);
    GENERAL_TEST_ASSERT(ok);
    GENERAL_TEST_ASSERT(!tab.isResetConfirmationOpen());
    GENERAL_TEST_ASSERT(onResetCalled);
    GENERAL_TEST_ASSERT(tab.getSaveCount() == 1);

    // Verify active configuration reverted to clean defaults
    FizzConfig defaults = FizzConfig::defaults();
    GENERAL_TEST_ASSERT(cfg.autoCloseOnUnfocus == defaults.autoCloseOnUnfocus);
    GENERAL_TEST_ASSERT(cfg.view_state_grace_seconds == defaults.view_state_grace_seconds);
    GENERAL_TEST_ASSERT(cfg.theme == defaults.theme);
    GENERAL_TEST_ASSERT(cfg.accent == defaults.accent);
    GENERAL_TEST_ASSERT(cfg.window_width == defaults.window_width);
    GENERAL_TEST_ASSERT(cfg.window_height == defaults.window_height);
    GENERAL_TEST_ASSERT(cfg.searchRoots == defaults.searchRoots);
    GENERAL_TEST_ASSERT(cfg.ignoreRules == defaults.ignoreRules);

    // Explicit Standard Desktop configuration verification
    std::string expectedHome = getHomeDir().empty() ? "~" : getHomeDir();
    GENERAL_TEST_ASSERT(cfg.searchRoots.size() == 2);
    GENERAL_TEST_ASSERT(cfg.searchRoots[0] == expectedHome);
    GENERAL_TEST_ASSERT(cfg.searchRoots[1] == "/usr/share/applications");

    std::vector<std::string> expectedIgnoreRules = {
        ".cache", ".git", ".cargo", ".rustup", ".npm", ".mozilla", "Trash"
    };
    GENERAL_TEST_ASSERT(cfg.ignoreRules == expectedIgnoreRules);

    GENERAL_TEST_ASSERT(resetCfgCallback.autoCloseOnUnfocus == defaults.autoCloseOnUnfocus);
    GENERAL_TEST_ASSERT(resetCfgCallback.view_state_grace_seconds == defaults.view_state_grace_seconds);
    GENERAL_TEST_ASSERT(resetCfgCallback.searchRoots == defaults.searchRoots);
    GENERAL_TEST_ASSERT(resetCfgCallback.ignoreRules == defaults.ignoreRules);

    // Verify on-disk file was saved atomically with clean defaults
    FizzConfig loaded = loadConfig(confPath);
    GENERAL_TEST_ASSERT(loaded.autoCloseOnUnfocus == defaults.autoCloseOnUnfocus);
    GENERAL_TEST_ASSERT(loaded.view_state_grace_seconds == defaults.view_state_grace_seconds);
    GENERAL_TEST_ASSERT(loaded.theme == defaults.theme);
    GENERAL_TEST_ASSERT(loaded.accent == defaults.accent);
    GENERAL_TEST_ASSERT(loaded.window_width == defaults.window_width);
    GENERAL_TEST_ASSERT(loaded.window_height == defaults.window_height);
    GENERAL_TEST_ASSERT(loaded.searchRoots == defaults.searchRoots);
    GENERAL_TEST_ASSERT(loaded.ignoreRules == defaults.ignoreRules);

    std::error_code ec;
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_general_tab_factory_defaults_restoration passed.\n";
}

void test_general_tab_headless_ui_rendering() {
    std::cout << "[TEST] Running test_general_tab_headless_ui_rendering...\n";

    std::string testDir = createTempTestDir("ui_render");
    std::string confPath = testDir + "/fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    saveConfig(cfg, confPath);

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(750.0f, 480.0f);

    GeneralTab tab(confPath);

    // Frame 1: Default rendering
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_settings", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(cfg);
    ImGui::End();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    GENERAL_TEST_ASSERT(drawData != nullptr);
    GENERAL_TEST_ASSERT(drawData->CmdListsCount > 0);

    // Frame 2: Trigger reset confirmation modal and render
    tab.requestReset();
    GENERAL_TEST_ASSERT(tab.isResetConfirmationOpen());

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_settings", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(cfg);
    ImGui::End();
    ImGui::Render();

    drawData = ImGui::GetDrawData();
    GENERAL_TEST_ASSERT(drawData != nullptr);
    GENERAL_TEST_ASSERT(drawData->CmdListsCount > 0);

    // Frame 3: Confirm reset
    bool ok = tab.confirmReset(cfg);
    GENERAL_TEST_ASSERT(ok);
    GENERAL_TEST_ASSERT(!tab.isResetConfirmationOpen());

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("test_settings", nullptr, ImGuiWindowFlags_NoDecoration);
    tab.render(cfg);
    ImGui::End();
    ImGui::Render();

    ImGui::DestroyContext(ctx);

    std::error_code ec;
    fs::remove_all(testDir, ec);
    std::cout << "[PASS] test_general_tab_headless_ui_rendering passed.\n";
}

void run_general_tab_tests() {
    test_general_tab_auto_close_mutation();
    test_general_tab_grace_period_clamping_and_live_mutation();
    test_general_tab_grace_period_debounce();
    test_general_tab_reset_confirmation_gating_and_wording();
    test_general_tab_factory_defaults_restoration();
    test_general_tab_headless_ui_rendering();
}
