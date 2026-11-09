#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "services/AppLauncher.h"

namespace fs = std::filesystem;

#define LAUNCHER_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_is_desktop_entry() {
    std::cout << "[TEST] Running test_is_desktop_entry...\n";

    LAUNCHER_TEST_ASSERT(isDesktopEntry("/usr/share/applications/firefox.desktop"));
    LAUNCHER_TEST_ASSERT(isDesktopEntry("~/.local/share/applications/editor.DESKTOP"));
    LAUNCHER_TEST_ASSERT(isDesktopEntry("app.desktop"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntry("/usr/bin/firefox"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntry("/home/user/notes.txt"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntry("/home/user/desktop"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntry(""));

    // Directories ending in .desktop should not be treated as Desktop Entries
    fs::path tempDir = fs::temp_directory_path() / "fizz_test_dir.desktop";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    LAUNCHER_TEST_ASSERT(!isDesktopEntry(tempDir.string()));
    fs::remove_all(tempDir, ec);

    std::cout << "[PASS] test_is_desktop_entry passed.\n";
}

void test_parse_desktop_exec() {
    std::cout << "[TEST] Running test_parse_desktop_exec...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_launcher";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    fs::path desktopEntryPath = tempDir / "sample.desktop";

    {
        std::ofstream out(desktopEntryPath);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=Sample App\n";
        out << "Exec=sample-app --flag \"hello world\" 'single quoted' my\\ app\\ tool %u %F\n";
        out << "Terminal=false\n";
    }

    std::vector<std::string> args = parseDesktopExec(desktopEntryPath.string());
    LAUNCHER_TEST_ASSERT(args.size() == 5);
    LAUNCHER_TEST_ASSERT(args[0] == "sample-app");
    LAUNCHER_TEST_ASSERT(args[1] == "--flag");
    LAUNCHER_TEST_ASSERT(args[2] == "hello world");
    LAUNCHER_TEST_ASSERT(args[3] == "single quoted");
    LAUNCHER_TEST_ASSERT(args[4] == "my app tool");

    // Missing file returns empty
    LAUNCHER_TEST_ASSERT(parseDesktopExec(tempDir / "nonexistent.desktop").empty());

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_parse_desktop_exec passed.\n";
}

void test_build_launch_command() {
    std::cout << "[TEST] Running test_build_launch_command...\n";

    // 1. Regular non-executable file -> xdg-open -- <file>
    {
        auto cmd = buildLaunchCommand("/home/user/document.pdf", ExecState::NotExecutable, false);
        LAUNCHER_TEST_ASSERT(cmd.size() == 3);
        LAUNCHER_TEST_ASSERT(cmd[0] == "xdg-open");
        LAUNCHER_TEST_ASSERT(cmd[1] == "--");
        LAUNCHER_TEST_ASSERT(cmd[2] == "/home/user/document.pdf");
    }

    // 2. Direct executable binary -> binary itself
    {
        auto cmd = buildLaunchCommand("/usr/bin/htop", ExecState::Executable, false);
        LAUNCHER_TEST_ASSERT(cmd.size() == 1);
        LAUNCHER_TEST_ASSERT(cmd[0] == "/usr/bin/htop");
    }

    // 3. showFolder flag -> xdg-open -- <parent-dir>
    {
        auto cmd = buildLaunchCommand("/home/user/documents/report.txt", ExecState::NotExecutable, true);
        LAUNCHER_TEST_ASSERT(cmd.size() == 3);
        LAUNCHER_TEST_ASSERT(cmd[0] == "xdg-open");
        LAUNCHER_TEST_ASSERT(cmd[1] == "--");
        LAUNCHER_TEST_ASSERT(cmd[2] == "/home/user/documents");
    }
    {
        auto cmd = buildLaunchCommand("/usr/share/applications/editor.desktop", ExecState::NotExecutable, true);
        LAUNCHER_TEST_ASSERT(cmd.size() == 3);
        LAUNCHER_TEST_ASSERT(cmd[0] == "xdg-open");
        LAUNCHER_TEST_ASSERT(cmd[1] == "--");
        LAUNCHER_TEST_ASSERT(cmd[2] == "/usr/share/applications");
    }

    // 4. Desktop Entry with explicit Gio backend -> gio launch -- <file>
    {
        auto cmd = buildLaunchCommand(
            "/usr/share/applications/firefox.desktop",
            ExecState::NotExecutable,
            false,
            DesktopLauncherBackend::Gio
        );
        LAUNCHER_TEST_ASSERT(cmd.size() == 4);
        LAUNCHER_TEST_ASSERT(cmd[0] == "gio");
        LAUNCHER_TEST_ASSERT(cmd[1] == "launch");
        LAUNCHER_TEST_ASSERT(cmd[2] == "--");
        LAUNCHER_TEST_ASSERT(cmd[3] == "/usr/share/applications/firefox.desktop");
    }

    // 5. Desktop Entry with explicit GtkLaunch backend -> gtk-launch -- <desktop-name>
    {
        auto cmd = buildLaunchCommand(
            "/usr/share/applications/firefox.desktop",
            ExecState::NotExecutable,
            false,
            DesktopLauncherBackend::GtkLaunch
        );
        LAUNCHER_TEST_ASSERT(cmd.size() == 3);
        LAUNCHER_TEST_ASSERT(cmd[0] == "gtk-launch");
        LAUNCHER_TEST_ASSERT(cmd[1] == "--");
        LAUNCHER_TEST_ASSERT(cmd[2] == "firefox.desktop");
    }

    // 6. Desktop Entry with ExecLine fallback backend -> parsed Exec= tokens
    {
        fs::path tempDir = fs::temp_directory_path() / "fizz_test_cmd_exec";
        std::error_code ec;
        fs::create_directories(tempDir, ec);
        fs::path deskPath = tempDir / "custom.desktop";
        {
            std::ofstream out(deskPath);
            out << "[Desktop Entry]\nName=Custom\nExec=/opt/custom/bin --launch %u\n";
        }

        auto cmd = buildLaunchCommand(
            deskPath.string(),
            ExecState::NotExecutable,
            false,
            DesktopLauncherBackend::ExecLine
        );
        LAUNCHER_TEST_ASSERT(cmd.size() == 2);
        LAUNCHER_TEST_ASSERT(cmd[0] == "/opt/custom/bin");
        LAUNCHER_TEST_ASSERT(cmd[1] == "--launch");

        fs::remove_all(tempDir, ec);
    }

    // 7. Desktop Entry with Auto backend -> launches via gio / gtk-launch / exec line (not xdg-open)
    {
        fs::path tempDir = fs::temp_directory_path() / "fizz_test_cmd_auto";
        std::error_code ec;
        fs::create_directories(tempDir, ec);
        fs::path deskPath = tempDir / "auto_test.desktop";
        {
            std::ofstream out(deskPath);
            out << "[Desktop Entry]\nName=AutoApp\nExec=auto-app-bin --start\n";
        }

        auto cmd = buildLaunchCommand(
            deskPath.string(),
            ExecState::NotExecutable,
            false,
            DesktopLauncherBackend::Auto
        );
        LAUNCHER_TEST_ASSERT(!cmd.empty());
        // Desktop entry must NOT open via raw xdg-open
        LAUNCHER_TEST_ASSERT(cmd[0] != "xdg-open");
        // On this system gio is installed, so cmd[0] is gio and cmd[1] is launch
        LAUNCHER_TEST_ASSERT(cmd[0] == "gio" || cmd[0] == "gtk-launch" || cmd[0] == "auto-app-bin");

        // Executable desktop entry should still use desktop launching semantics, not direct binary exec
        auto cmdExec = buildLaunchCommand(
            deskPath.string(),
            ExecState::Executable,
            false,
            DesktopLauncherBackend::Auto
        );
        LAUNCHER_TEST_ASSERT(!cmdExec.empty());
        LAUNCHER_TEST_ASSERT(cmdExec[0] != deskPath.string());
        LAUNCHER_TEST_ASSERT(cmdExec[0] != "xdg-open");

        fs::remove_all(tempDir, ec);
    }

    std::cout << "[PASS] test_build_launch_command passed.\n";
}

void test_is_desktop_entry_trusted() {
    std::cout << "[TEST] Running test_is_desktop_entry_trusted...\n";

    const auto& trustedDirs = getTrustedApplicationDirectories();
    LAUNCHER_TEST_ASSERT(!trustedDirs.empty());
    bool hasUsrShare = false;
    for (const auto& dir : trustedDirs) {
        if (dir == "/usr/share/applications/") {
            hasUsrShare = true;
            break;
        }
    }
    LAUNCHER_TEST_ASSERT(hasUsrShare);

    // Standard FreeDesktop application directories -> trusted
    LAUNCHER_TEST_ASSERT(isDesktopEntryTrusted("/usr/share/applications/firefox.desktop"));
    LAUNCHER_TEST_ASSERT(isDesktopEntryTrusted("/usr/local/share/applications/myapp.desktop"));
    LAUNCHER_TEST_ASSERT(isDesktopEntryTrusted("/var/lib/flatpak/exports/share/applications/com.app.desktop"));

    // Paths under $HOME/.local/share/applications -> trusted
    const char* home = std::getenv("HOME");
    if (home) {
        std::string trustedPath = std::string(home) + "/.local/share/applications/editor.desktop";
        LAUNCHER_TEST_ASSERT(isDesktopEntryTrusted(trustedPath));

        std::string flatpakPath = std::string(home) + "/.local/share/flatpak/exports/share/applications/com.app.desktop";
        LAUNCHER_TEST_ASSERT(isDesktopEntryTrusted(flatpakPath));
    }

    // Non-standard paths -> untrusted
    LAUNCHER_TEST_ASSERT(!isDesktopEntryTrusted("/tmp/evil.desktop"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntryTrusted("/home/user/Desktop/myapp.desktop"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntryTrusted("/opt/vendor/apps/app.desktop"));
    LAUNCHER_TEST_ASSERT(!isDesktopEntryTrusted("/usr/share/applications.desktop"));  // not inside the dir

    std::cout << "[PASS] test_is_desktop_entry_trusted passed.\n";
}

void run_app_launcher_tests() {
    test_is_desktop_entry();
    test_parse_desktop_exec();
    test_build_launch_command();
    test_is_desktop_entry_trusted();
}
