#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "services/AboutView.h"
#include "services/Config.h"
#include "services/FileCacheBuilder.h"
#include "services/FileSearch.h"
#include "services/Theme.h"
#include "imgui.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

#define ABOUT_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_about_view_metadata() {
    std::cout << "[TEST] Running test_about_view_metadata...\n";

    // Acceptance criteria: Displays version v1.1.0, license, config path, repository URL
    ABOUT_TEST_ASSERT(fizz::ui::getAppVersion() == "v1.1.0");
    ABOUT_TEST_ASSERT(fizz::ui::kAppVersion == "v1.1.0");

    ABOUT_TEST_ASSERT(fizz::ui::getAppLicense() == "MIT License");
    ABOUT_TEST_ASSERT(fizz::ui::kAppLicense == "MIT License");

    ABOUT_TEST_ASSERT(fizz::ui::getRepoUrl() == "https://github.com/sahil-shefeek/fizz");
    ABOUT_TEST_ASSERT(fizz::ui::kRepoUrl == "https://github.com/sahil-shefeek/fizz");

    // Config path verification
    std::string cfgPath = getConfigPath();
    ABOUT_TEST_ASSERT(!cfgPath.empty());
    ABOUT_TEST_ASSERT(cfgPath.find("fizz.conf") != std::string::npos);

    std::cout << "[PASS] test_about_view_metadata passed.\n";
}

void test_shortcut_cheat_sheet_content() {
    std::cout << "[TEST] Running test_shortcut_cheat_sheet_content...\n";

    const auto& shortcuts = fizz::ui::getDefaultShortcutCheatSheet();
    ABOUT_TEST_ASSERT(!shortcuts.empty());

    bool hasFileLaunch = false;
    bool hasTerminalExec = false;
    bool hasUntrustedDesktop = false;
    bool hasTabNav = false;
    bool hasEscape = false;

    for (const auto& item : shortcuts) {
        std::string actLower = item.action;
        for (char& c : actLower) c = std::tolower(static_cast<unsigned char>(c));

        std::string keyLower = item.key;
        for (char& c : keyLower) c = std::tolower(static_cast<unsigned char>(c));

        // Launching files
        if (keyLower.find("enter") != std::string::npos && (actLower.find("open") != std::string::npos || actLower.find("launch") != std::string::npos)) {
            hasFileLaunch = true;
        }

        // Terminal execution
        if (actLower.find("terminal") != std::string::npos || keyLower.find("alt+enter") != std::string::npos) {
            hasTerminalExec = true;
        }

        // Untrusted desktop execution
        if (keyLower.find("ctrl+enter") != std::string::npos && (actLower.find("untrusted") != std::string::npos || actLower.find("security") != std::string::npos)) {
            hasUntrustedDesktop = true;
        }

        // Tab navigation
        if (keyLower.find("tab") != std::string::npos) {
            hasTabNav = true;
        }

        // Escape
        if (keyLower.find("esc") != std::string::npos) {
            hasEscape = true;
        }
    }

    ABOUT_TEST_ASSERT(hasFileLaunch);
    ABOUT_TEST_ASSERT(hasTerminalExec);
    ABOUT_TEST_ASSERT(hasUntrustedDesktop);
    ABOUT_TEST_ASSERT(hasTabNav);
    ABOUT_TEST_ASSERT(hasEscape);

    std::cout << "[PASS] test_shortcut_cheat_sheet_content passed.\n";
}

void test_shortcut_cheat_sheet_table_formatting() {
    std::cout << "[TEST] Running test_shortcut_cheat_sheet_table_formatting...\n";

    const auto& shortcuts = fizz::ui::getDefaultShortcutCheatSheet();
    std::string formattedTable = fizz::ui::formatShortcutTable(shortcuts);

    ABOUT_TEST_ASSERT(!formattedTable.empty());

    // Verify table headers
    ABOUT_TEST_ASSERT(formattedTable.find("Shortcut") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("Action") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("Category") != std::string::npos);

    // Verify markdown / ascii separator row
    ABOUT_TEST_ASSERT(formattedTable.find("|-") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("-|-") != std::string::npos);

    // Verify key documented shortcuts appear in the formatted table
    ABOUT_TEST_ASSERT(formattedTable.find("Enter") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("terminal") != std::string::npos || formattedTable.find("Terminal") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("untrusted") != std::string::npos || formattedTable.find("Untrusted") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("Tab") != std::string::npos);
    ABOUT_TEST_ASSERT(formattedTable.find("Esc") != std::string::npos);

    // Verify column alignment: every row should have identical count of pipe delimiters '|'
    std::istringstream stream(formattedTable);
    std::string line;
    int expectedPipes = -1;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        int pipes = 0;
        for (char c : line) {
            if (c == '|') pipes++;
        }
        if (expectedPipes < 0) {
            expectedPipes = pipes;
            ABOUT_TEST_ASSERT(expectedPipes == 4); // | col1 | col2 | col3 |
        } else {
            ABOUT_TEST_ASSERT(pipes == expectedPipes);
        }
    }

    std::cout << "[PASS] test_shortcut_cheat_sheet_table_formatting passed.\n";
}

void test_diagnostics_polling_rate_limiting() {
    std::cout << "[TEST] Running test_diagnostics_polling_rate_limiting...\n";

    fizz::ui::AboutViewDiagnostics poller;
    ABOUT_TEST_ASSERT(poller.getPollCount() == 0);
    ABOUT_TEST_ASSERT(!poller.hasPolled());

    auto t0 = std::chrono::steady_clock::time_point(1000s);

    // 1. Initial poll at t0 must succeed and increment poll count
    bool polled1 = poller.refresh(t0);
    ABOUT_TEST_ASSERT(polled1 == true);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 1);
    ABOUT_TEST_ASSERT(poller.hasPolled() == true);
    ABOUT_TEST_ASSERT(poller.getLastPollTime() == t0);

    // 2. Poll at t0 + 100ms must be throttled (< 1 second)
    bool polled2 = poller.refresh(t0 + 100ms);
    ABOUT_TEST_ASSERT(polled2 == false);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 1);

    // 3. Poll at t0 + 500ms must be throttled
    bool polled3 = poller.refresh(t0 + 500ms);
    ABOUT_TEST_ASSERT(polled3 == false);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 1);

    // 4. Poll at t0 + 999ms must be throttled
    bool polled4 = poller.refresh(t0 + 999ms);
    ABOUT_TEST_ASSERT(polled4 == false);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 1);

    // 5. Poll at t0 + 1000ms (exactly 1 second) must succeed
    bool polled5 = poller.refresh(t0 + 1000ms);
    ABOUT_TEST_ASSERT(polled5 == true);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 2);
    ABOUT_TEST_ASSERT(poller.getLastPollTime() == t0 + 1000ms);

    // 6. Poll at t0 + 1500ms must be throttled
    bool polled6 = poller.refresh(t0 + 1500ms);
    ABOUT_TEST_ASSERT(polled6 == false);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 2);

    // 7. Poll at t0 + 2001ms must succeed
    bool polled7 = poller.refresh(t0 + 2001ms);
    ABOUT_TEST_ASSERT(polled7 == true);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 3);

    // 8. Force refresh bypasses interval throttling
    bool polledForced = poller.refresh(t0 + 2010ms, /*force=*/true);
    ABOUT_TEST_ASSERT(polledForced == true);
    ABOUT_TEST_ASSERT(poller.getPollCount() == 4);

    // 9. Reset clears polling state
    poller.reset();
    ABOUT_TEST_ASSERT(poller.getPollCount() == 0);
    ABOUT_TEST_ASSERT(!poller.hasPolled());

    std::cout << "[PASS] test_diagnostics_polling_rate_limiting passed.\n";
}

void test_cache_diagnostics_collection() {
    std::cout << "[TEST] Running test_cache_diagnostics_collection...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_about_cache";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    fs::path cachePath = sandboxDir / "mock_cache.bin";

    // 1. Missing cache file diagnostics
    auto diagMissing = fizz::ui::collectCacheDiagnostics(cachePath.string());
    ABOUT_TEST_ASSERT(!diagMissing.exists);
    ABOUT_TEST_ASSERT(diagMissing.totalEntries == 0);
    ABOUT_TEST_ASSERT(diagMissing.fileSizeBytes == 0);
    ABOUT_TEST_ASSERT(diagMissing.formattedFileSize == "0 B");
    ABOUT_TEST_ASSERT(diagMissing.lastRebuildTime == "Not found");

    // 2. Create mock binary cache file: structCount=123, charCount=456 + data
    {
        std::ofstream out(cachePath, std::ios::binary);
        size_t structCount = 123;
        size_t charCount = 456;
        out.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
        out.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
        std::vector<char> dummyStructs(structCount * sizeof(fileEntity), 0);
        out.write(dummyStructs.data(), dummyStructs.size());
        std::string dummyChars(charCount, 'a');
        out.write(dummyChars.data(), dummyChars.size());
    }

    auto diagPresent = fizz::ui::collectCacheDiagnostics(cachePath.string());
    ABOUT_TEST_ASSERT(diagPresent.exists);
    ABOUT_TEST_ASSERT(diagPresent.totalEntries == 123);
    ABOUT_TEST_ASSERT(diagPresent.fileSizeBytes > sizeof(size_t) * 2);
    ABOUT_TEST_ASSERT(!diagPresent.formattedFileSize.empty());
    ABOUT_TEST_ASSERT(diagPresent.formattedFileSize != "0 B");
    ABOUT_TEST_ASSERT(diagPresent.lastRebuildTime != "Not found");
    ABOUT_TEST_ASSERT(diagPresent.lastRebuildTime.find("-") != std::string::npos);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_cache_diagnostics_collection passed.\n";
}

void test_daemon_diagnostics_states() {
    std::cout << "[TEST] Running test_daemon_diagnostics_states...\n";

    // 1. String formatting checks for exact indicator format
    // - ● fizzd: running (PID)
    // - ● fizzd: stopped
    // - ⚠️ fizzd: running (watch limit reached)
    ABOUT_TEST_ASSERT(fizz::ui::formatDaemonStatus(false, 0, false) == "● fizzd: stopped");
    ABOUT_TEST_ASSERT(fizz::ui::formatDaemonStatus(true, 1234, false) == "● fizzd: running (1234)");
    ABOUT_TEST_ASSERT(fizz::ui::formatDaemonStatus(true, 4242, false) == "● fizzd: running (4242)");
    ABOUT_TEST_ASSERT(fizz::ui::formatDaemonStatus(true, 1234, true) == "⚠️ fizzd: running (watch limit reached)");
    ABOUT_TEST_ASSERT(fizz::ui::formatDaemonStatus(true, 0, true) == "⚠️ fizzd: running (watch limit reached)");

    // 2. Live daemon status in sandbox runtime directory
    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_about_daemon";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    // Initial state: stopped
    auto diag1 = fizz::ui::collectDaemonDiagnostics(sandboxDir.string());
    ABOUT_TEST_ASSERT(!diag1.isRunning);
    ABOUT_TEST_ASSERT(diag1.statusIndicator == "● fizzd: stopped");

    // Acquire lock (daemon running)
    int lockFd = acquireDaemonLock(sandboxDir.string());
    ABOUT_TEST_ASSERT(lockFd >= 0);

    auto diag2 = fizz::ui::collectDaemonDiagnostics(sandboxDir.string());
    ABOUT_TEST_ASSERT(diag2.isRunning);
    ABOUT_TEST_ASSERT(diag2.pid == getpid());
    ABOUT_TEST_ASSERT(diag2.statusIndicator == "● fizzd: running (" + std::to_string(getpid()) + ")");

    // Create inotify watch limit marker
    fs::path watchLimitMarker = sandboxDir / "fizzd.watch_limit";
    {
        std::ofstream out(watchLimitMarker);
        out << "limit reached\n";
    }

    auto diag3 = fizz::ui::collectDaemonDiagnostics(sandboxDir.string());
    ABOUT_TEST_ASSERT(diag3.isRunning);
    ABOUT_TEST_ASSERT(diag3.watchLimitReached);
    ABOUT_TEST_ASSERT(diag3.statusIndicator == "⚠️ fizzd: running (watch limit reached)");

    // Remove marker and release lock (daemon stopped)
    fs::remove(watchLimitMarker, ec);
    releaseDaemonLock(lockFd);

    auto diag4 = fizz::ui::collectDaemonDiagnostics(sandboxDir.string());
    ABOUT_TEST_ASSERT(!diag4.isRunning);
    ABOUT_TEST_ASSERT(diag4.statusIndicator == "● fizzd: stopped");

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_daemon_diagnostics_states passed.\n";
}

void test_headless_about_view_rendering() {
    std::cout << "[TEST] Running test_headless_about_view_rendering...\n";

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(750.0f, 480.0f);
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    FizzConfig config = FizzConfig::defaults();
    ViewModeStateMachine stateMachine(ViewMode::About);

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("fizz_test_window", nullptr);

    fizz::ui::renderAboutView(config, stateMachine);

    ImGui::End();
    ImGui::Render();

    ImGui::DestroyContext(ctx);

    std::cout << "[PASS] test_headless_about_view_rendering passed.\n";
}

void run_about_view_tests() {
    test_about_view_metadata();
    test_shortcut_cheat_sheet_content();
    test_shortcut_cheat_sheet_table_formatting();
    test_diagnostics_polling_rate_limiting();
    test_cache_diagnostics_collection();
    test_daemon_diagnostics_states();
    test_headless_about_view_rendering();
}
