#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "services/Config.h"
#include "services/FileSearch.h"
#include "services/LoadFile.h"

namespace fs = std::filesystem;

#define CONFIG_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_config_parser_defaults_and_missing_file() {
    std::cout << "[TEST] Running test_config_parser_defaults_and_missing_file...\n";

    const char* home = std::getenv("HOME");
    std::string expectedHome = home ? home : "";
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    std::string expectedConfig = (xdgConfigHome && xdgConfigHome[0] != '\0')
        ? std::string(xdgConfigHome) + "/fizz/fizz.conf"
        : expectedHome + "/.config/fizz/fizz.conf";

    // 1. By default, getConfigPath() resolves to expected ~/.config/fizz/fizz.conf
    CONFIG_TEST_ASSERT(getConfigPath() == expectedConfig);

    // 2. setConfigPath override
    std::string overridePath = "/tmp/nonexistent_fizz.conf";
    setConfigPath(overridePath);
    CONFIG_TEST_ASSERT(getConfigPath() == overridePath);

    // 3. Loading missing config falls back cleanly to defaults without error
    FizzConfig config = loadConfig(overridePath);
    auto defaultRoots = getDefaultSearchRoots(true);
    CONFIG_TEST_ASSERT(config.searchRoots == defaultRoots);
    CONFIG_TEST_ASSERT(config.ignoreRules.empty());
    CONFIG_TEST_ASSERT(config.autoCloseOnUnfocus == true);
    CONFIG_TEST_ASSERT(config.config_version == 1);
    CONFIG_TEST_ASSERT(config.theme == "dark");
    CONFIG_TEST_ASSERT(config.accent == "blue");
    CONFIG_TEST_ASSERT(config.window_width == 750);
    CONFIG_TEST_ASSERT(config.window_height == 480);
    CONFIG_TEST_ASSERT(config.font_path == "");
    CONFIG_TEST_ASSERT(config.font_size == 16.0f);
    CONFIG_TEST_ASSERT(config.binary_cache_path == "");
    CONFIG_TEST_ASSERT(config.view_state_grace_seconds == 5);
    CONFIG_TEST_ASSERT(config.unrecognizedKeys.empty());

    // 4. Resetting config path override restores default
    setConfigPath("");
    CONFIG_TEST_ASSERT(getConfigPath() == expectedConfig);

    std::cout << "[PASS] test_config_parser_defaults_and_missing_file passed.\n";
}

void test_config_parser_custom_values() {
    std::cout << "[TEST] Running test_config_parser_custom_values...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_custom";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    fs::path confFile = tempDir / "fizz.conf";
    {
        std::ofstream out(confFile);
        out << "# Fizz configuration test\n";
        out << "\n";
        out << "; Another comment line\n";
        out << "search_roots = ~/projects, /mnt/storage, /opt/tools/\n";
        out << "ignore_rules = .git, node_modules, build, /data/tmp\n";
    }

    FizzConfig config = loadConfig(confFile.string());

    const char* home = std::getenv("HOME");
    std::string expectedHome = home ? home : "";
    std::string expectedExpandedHome = expectedHome + "/projects";

    CONFIG_TEST_ASSERT(config.searchRoots.size() == 3);
    CONFIG_TEST_ASSERT(config.searchRoots[0] == expectedExpandedHome);
    CONFIG_TEST_ASSERT(config.searchRoots[1] == "/mnt/storage");
    CONFIG_TEST_ASSERT(config.searchRoots[2] == "/opt/tools"); // normalized without trailing slash

    CONFIG_TEST_ASSERT(config.ignoreRules.size() == 4);
    CONFIG_TEST_ASSERT(config.ignoreRules[0] == ".git");
    CONFIG_TEST_ASSERT(config.ignoreRules[1] == "node_modules");
    CONFIG_TEST_ASSERT(config.ignoreRules[2] == "build");
    CONFIG_TEST_ASSERT(config.ignoreRules[3] == "/data/tmp");
    CONFIG_TEST_ASSERT(config.autoCloseOnUnfocus == true);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_parser_custom_values passed.\n";
}

void test_config_parser_corrupt_and_unreadable() {
    std::cout << "[TEST] Running test_config_parser_corrupt_and_unreadable...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_corrupt";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    // 1. File with gibberish and missing values
    fs::path confFile = tempDir / "malformed.conf";
    {
        std::ofstream out(confFile);
        out << "this is not key value\n";
        out << "invalid syntax ??? !!!\n";
        out << "search_roots = \n"; // empty
        out << "ignore_rules = \n"; // empty
    }

    FizzConfig config = loadConfig(confFile.string());
    auto defaultRoots = getDefaultSearchRoots(true);
    CONFIG_TEST_ASSERT(config.searchRoots == defaultRoots);
    CONFIG_TEST_ASSERT(config.ignoreRules.empty());

    // 1b. File with an unrecognized option does NOT trigger fallback (permissive parsing)
    fs::path permissiveOptFile = tempDir / "permissive_opt.conf";
    {
        std::ofstream out(permissiveOptFile);
        out << "search_roots = /tmp/valid\n";
        out << "unknown_unreadable_setting = true\n";
    }
    FizzConfig optConfig = loadConfig(permissiveOptFile.string());
    CONFIG_TEST_ASSERT(optConfig.searchRoots.size() == 1 && optConfig.searchRoots[0] == "/tmp/valid");
    CONFIG_TEST_ASSERT(optConfig.unrecognizedKeys.size() == 1);
    CONFIG_TEST_ASSERT(optConfig.unrecognizedKeys[0].first == "unknown_unreadable_setting");
    CONFIG_TEST_ASSERT(optConfig.unrecognizedKeys[0].second == "true");

    // 2. Binary / unreadable data
    fs::path binConf = tempDir / "binary.conf";
    {
        std::ofstream out(binConf, std::ios::binary);
        char binData[] = { '\0', '\1', '\x80', '\xFF', '\x05' };
        out.write(binData, sizeof(binData));
    }

    FizzConfig binConfig = loadConfig(binConf.string());
    CONFIG_TEST_ASSERT(binConfig.searchRoots == defaultRoots);
    CONFIG_TEST_ASSERT(binConfig.ignoreRules.empty());

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_parser_corrupt_and_unreadable passed.\n";
}

void test_config_parser_auto_close_on_unfocus() {
    std::cout << "[TEST] Running test_config_parser_auto_close_on_unfocus...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_focus";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    // 1. Missing file defaults to autoCloseOnUnfocus = true
    FizzConfig missingConfig = loadConfig((tempDir / "nonexistent.conf").string());
    CONFIG_TEST_ASSERT(missingConfig.autoCloseOnUnfocus == true);

    // 2. Config without auto_close_on_unfocus defaults to true
    fs::path defaultConf = tempDir / "default.conf";
    {
        std::ofstream out(defaultConf);
        out << "search_roots = /tmp\n";
    }
    FizzConfig loadedDefault = loadConfig(defaultConf.string());
    CONFIG_TEST_ASSERT(loadedDefault.autoCloseOnUnfocus == true);

    // 3. Explicitly setting auto_close_on_unfocus = false (and "0")
    fs::path disabledConf = tempDir / "disabled.conf";
    {
        std::ofstream out(disabledConf);
        out << "auto_close_on_unfocus = false\n";
    }
    FizzConfig loadedDisabled = loadConfig(disabledConf.string());
    CONFIG_TEST_ASSERT(loadedDisabled.autoCloseOnUnfocus == false);

    fs::path disabledConfZero = tempDir / "disabled_zero.conf";
    {
        std::ofstream out(disabledConfZero);
        out << "auto_close_on_unfocus = 0\n";
    }
    FizzConfig loadedDisabledZero = loadConfig(disabledConfZero.string());
    CONFIG_TEST_ASSERT(loadedDisabledZero.autoCloseOnUnfocus == false);

    // 4. Explicitly setting auto_close_on_unfocus = true (and "1")
    fs::path enabledConf = tempDir / "enabled.conf";
    {
        std::ofstream out(enabledConf);
        out << "auto_close_on_unfocus = true\n";
    }
    FizzConfig loadedEnabled = loadConfig(enabledConf.string());
    CONFIG_TEST_ASSERT(loadedEnabled.autoCloseOnUnfocus == true);

    fs::path enabledConfOne = tempDir / "enabled_one.conf";
    {
        std::ofstream out(enabledConfOne);
        out << "auto_close_on_unfocus = 1\n";
    }
    FizzConfig loadedEnabledOne = loadConfig(enabledConfOne.string());
    CONFIG_TEST_ASSERT(loadedEnabledOne.autoCloseOnUnfocus == true);

    // 5. Corrupt / invalid value falls back to default (true)
    fs::path invalidConf = tempDir / "invalid.conf";
    {
        std::ofstream out(invalidConf);
        out << "auto_close_on_unfocus = not_a_boolean\n";
    }
    FizzConfig loadedInvalid = loadConfig(invalidConf.string());
    CONFIG_TEST_ASSERT(loadedInvalid.autoCloseOnUnfocus == true);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_parser_auto_close_on_unfocus passed.\n";
}

void test_config_custom_roots_and_ignores_traversal() {
    std::cout << "[TEST] Running test_config_custom_roots_and_ignores_traversal...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_custom_roots_ignores";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);

    fs::path customRoot1 = sandboxDir / "custom_root1";
    fs::path customRoot2 = sandboxDir / "custom_root2";
    fs::create_directories(customRoot1 / "normal_dir", ec);
    fs::create_directories(customRoot1 / "ignored_by_name", ec);
    fs::create_directories(customRoot1 / "build_prefix" / "build", ec);
    fs::create_directories(customRoot1 / ".git", ec);
    fs::create_directories(customRoot2 / "prefix_ignored", ec);

    // Files in customRoot1
    {
        std::ofstream out(customRoot1 / "file_a.txt");
        out << "custom root 1 file\n";
    }
    {
        std::ofstream out(customRoot1 / "normal_dir" / "file_b.txt");
        out << "nested normal file\n";
    }
    {
        std::ofstream out(customRoot1 / "ignored_by_name" / "secret.txt");
        out << "should be ignored by name\n";
    }
    {
        std::ofstream out(customRoot1 / "build_prefix" / "build" / "nested_build_artifact.o");
        out << "object file\n";
    }
    {
        std::ofstream out(customRoot1 / ".git" / "HEAD");
        out << "ref: refs/heads/main\n";
    }

    // Files in customRoot2
    {
        std::ofstream out(customRoot2 / "app.desktop");
        out << "[Desktop Entry]\nName=CustomApp\nType=Application\n";
    }
    {
        std::ofstream out(customRoot2 / "prefix_ignored" / "should_not_index.txt");
        out << "should be ignored by path prefix\n";
    }

    fs::path confFile = sandboxDir / "test_fizz.conf";
    {
        std::ofstream out(confFile);
        out << "search_roots = " << customRoot1.string() << ", " << customRoot2.string() << "\n";
        out << "ignore_rules = ignored_by_name, build, " << (customRoot2 / "prefix_ignored").string() << "\n";
    }

    fs::path cachePath = sandboxDir / "custom_traversal_cache.bin";
    setBinaryCachePath(cachePath.string());
    setConfigPath(confFile.string());

    // Load config and apply
    FizzConfig config = loadConfig();
    CONFIG_TEST_ASSERT(config.searchRoots.size() == 2);
    CONFIG_TEST_ASSERT(config.ignoreRules.size() == 3);

    setCustomIgnoreRules(config.ignoreRules);
    systemFileSearch(config.searchRoots);

    CONFIG_TEST_ASSERT(fs::exists(cachePath));
    CONFIG_TEST_ASSERT(fs::file_size(cachePath) > 0);

    bool loaded = loadIndexToMemory();
    CONFIG_TEST_ASSERT(loaded);

    bool foundFileA = false;
    bool foundFileB = false;
    bool foundAppDesktop = false;
    bool foundSecret = false;
    bool foundGitHead = false;
    bool foundPrefixIgnored = false;
    bool foundBuildArtifact = false;

    for (const auto& ent : fileStructTable) {
        CONFIG_TEST_ASSERT(ent.f_offset < g_InMemoryIndexBuffer.size());
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string pathStr(namePtr);

        if (pathStr.find("file_a.txt") != std::string::npos) foundFileA = true;
        if (pathStr.find("file_b.txt") != std::string::npos) foundFileB = true;
        if (pathStr.find("app.desktop") != std::string::npos) {
            foundAppDesktop = true;
            CONFIG_TEST_ASSERT(classifyPath(pathStr, ent.f_execState) == "app");
        }
        if (pathStr.find("secret.txt") != std::string::npos) foundSecret = true;
        if (pathStr.find("HEAD") != std::string::npos) foundGitHead = true;
        if (pathStr.find("should_not_index.txt") != std::string::npos) foundPrefixIgnored = true;
        if (pathStr.find("nested_build_artifact.o") != std::string::npos) foundBuildArtifact = true;
    }

    CONFIG_TEST_ASSERT(foundFileA);
    CONFIG_TEST_ASSERT(foundFileB);
    CONFIG_TEST_ASSERT(foundAppDesktop);
    CONFIG_TEST_ASSERT(!foundSecret);        // Custom Ignore Rule (directory name)
    CONFIG_TEST_ASSERT(!foundPrefixIgnored); // Custom Ignore Rule (path prefix)
    CONFIG_TEST_ASSERT(!foundGitHead);       // Default Ignore Rule (.git)
    CONFIG_TEST_ASSERT(!foundBuildArtifact); // Custom Ignore Rule

    // Reset settings and cleanup
    setBinaryCachePath("");
    setConfigPath("");
    setCustomIgnoreRules({});
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_config_custom_roots_and_ignores_traversal passed.\n";
}

void test_custom_ignore_rules_matching() {
    std::cout << "[TEST] Running test_custom_ignore_rules_matching...\n";

    setCustomIgnoreRules({"/var/log/custom", "target", "build"});

    // 1. Path prefix rules
    CONFIG_TEST_ASSERT(isPathIgnored("/var/log/custom", "custom"));
    CONFIG_TEST_ASSERT(isPathIgnored("/var/log/custom/app.log", "app.log"));
    CONFIG_TEST_ASSERT(isPathIgnored("/var/log/custom/nested/file.txt", "file.txt"));
    // Must NOT match prefix that is merely a substring / shared prefix of directory name
    CONFIG_TEST_ASSERT(!isPathIgnored("/var/log/custom_other/file.txt", "file.txt"));

    // 2. Component / directory name rules
    CONFIG_TEST_ASSERT(isPathIgnored("/home/user/rust/target/debug/bin", "bin"));
    CONFIG_TEST_ASSERT(isPathIgnored("/home/user/rust/target", "target"));
    CONFIG_TEST_ASSERT(isPathIgnored("", "target"));
    CONFIG_TEST_ASSERT(isPathIgnored("/home/user/project/build/output.o", "output.o"));
    // Must NOT match partial component name
    CONFIG_TEST_ASSERT(!isPathIgnored("/home/user/rust/target_backup/bin", "bin"));
    CONFIG_TEST_ASSERT(!isPathIgnored("/home/user/build_dir/output.o", "output.o"));

    // 3. Default ignore rules must remain active
    CONFIG_TEST_ASSERT(isPathIgnored("/home/user/.git/config", "config"));
    CONFIG_TEST_ASSERT(isPathIgnored("", ".git"));
    CONFIG_TEST_ASSERT(isPathIgnored("/home/user/.cache/app", "app"));
    CONFIG_TEST_ASSERT(isPathIgnored("/home/user/node_modules/pkg", "pkg"));

    // Reset custom ignore rules
    setCustomIgnoreRules({});
    CONFIG_TEST_ASSERT(!isPathIgnored("/var/log/custom/app.log", "app.log"));
    CONFIG_TEST_ASSERT(!isPathIgnored("/home/user/rust/target/debug/bin", "bin"));

    std::cout << "[PASS] test_custom_ignore_rules_matching passed.\n";
}

void test_user_runtime_dir_and_fallback_isolation() {
    std::cout << "[TEST] Running test_user_runtime_dir_and_fallback_isolation...\n";

    // 1. Verify custom runtime directory override
    fs::path tempDir = fs::temp_directory_path() / "fizz_test_runtime_custom";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    setCustomRuntimeDir(tempDir.string());
    CONFIG_TEST_ASSERT(getUserRuntimeDir() == tempDir.string());
    setCustomRuntimeDir("");

    // 2. Verify runtime dir resolution when XDG_RUNTIME_DIR is set vs unset
    std::string runtimeDir = getUserRuntimeDir();
    CONFIG_TEST_ASSERT(!runtimeDir.empty());

    // 3. Fallback directory format (/tmp/fizz-<uid>) mode 0700 verification
    uid_t uid = getuid();
    std::string expectedFallback = "/tmp/fizz-" + std::to_string(uid);

    // If XDG_RUNTIME_DIR is not present or we resolve fallback, check permissions
    struct stat st;
    int res = lstat(runtimeDir.c_str(), &st);
    CONFIG_TEST_ASSERT(res == 0);
    CONFIG_TEST_ASSERT(S_ISDIR(st.st_mode));
    CONFIG_TEST_ASSERT(!S_ISLNK(st.st_mode));
    CONFIG_TEST_ASSERT(st.st_uid == uid);

    // If the resolved directory is the fallback /tmp/fizz-<uid>, verify strict 0700 permissions
    if (runtimeDir == expectedFallback) {
        CONFIG_TEST_ASSERT((st.st_mode & 0777) == 0700);
    }

    // 4. Test symlink rejection for runtime directory
    fs::path evilSymlink = fs::temp_directory_path() / "fizz_test_evil_runtime_symlink";
    fs::remove(evilSymlink, ec);
    fs::create_directory_symlink(tempDir, evilSymlink, ec);
    // Overriding with a symlink should be rejected by secure validation or handled safely
    struct stat symSt;
    CONFIG_TEST_ASSERT(lstat(evilSymlink.c_str(), &symSt) == 0 && S_ISLNK(symSt.st_mode));

    // Cleanup
    fs::remove(evilSymlink, ec);
    fs::remove_all(tempDir, ec);

    std::cout << "[PASS] test_user_runtime_dir_and_fallback_isolation passed.\n";
}

void test_config_schema_version_and_defaults() {
    std::cout << "[TEST] Running test_config_schema_version_and_defaults...\n";

    // 1. Direct defaults() call has config_version = 1 and correct default field values
    FizzConfig def = FizzConfig::defaults();
    CONFIG_TEST_ASSERT(def.config_version == 1);
    CONFIG_TEST_ASSERT(def.theme == "dark");
    CONFIG_TEST_ASSERT(def.accent == "blue");
    CONFIG_TEST_ASSERT(def.window_width == 750);
    CONFIG_TEST_ASSERT(def.window_height == 480);
    CONFIG_TEST_ASSERT(def.font_path.empty());
    CONFIG_TEST_ASSERT(def.font_size == 16.0f);
    CONFIG_TEST_ASSERT(def.binary_cache_path.empty());
    CONFIG_TEST_ASSERT(def.view_state_grace_seconds == 5);
    CONFIG_TEST_ASSERT(def.unrecognizedKeys.empty());

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_version";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    // 2. Loading existing file lacking config_version defaults config_version to 1
    fs::path unversionedFile = tempDir / "unversioned.conf";
    {
        std::ofstream out(unversionedFile);
        out << "search_roots = /usr/bin\n";
        out << "theme = light\n";
    }
    FizzConfig unversionedCfg = loadConfig(unversionedFile.string());
    CONFIG_TEST_ASSERT(unversionedCfg.config_version == 1);
    CONFIG_TEST_ASSERT(unversionedCfg.theme == "light");

    // 3. Loading file with explicit config_version preserves version
    fs::path versionedFile = tempDir / "versioned.conf";
    {
        std::ofstream out(versionedFile);
        out << "config_version = 2\n";
        out << "theme = dark\n";
    }
    FizzConfig versionedCfg = loadConfig(versionedFile.string());
    CONFIG_TEST_ASSERT(versionedCfg.config_version == 2);

    // 4. Loading file with invalid/negative config_version falls back to 1
    fs::path invalidVersionFile = tempDir / "invalid_version.conf";
    {
        std::ofstream out(invalidVersionFile);
        out << "config_version = invalid_v1\n";
    }
    FizzConfig invalidVersionCfg = loadConfig(invalidVersionFile.string());
    CONFIG_TEST_ASSERT(invalidVersionCfg.config_version == 1);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_schema_version_and_defaults passed.\n";
}

void test_config_bounded_fields_and_enum_fallbacks() {
    std::cout << "[TEST] Running test_config_bounded_fields_and_enum_fallbacks...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_bounded";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    const char* home = std::getenv("HOME");
    std::string expectedHome = home ? home : "";

    // 1. Valid custom values
    fs::path validFile = tempDir / "valid.conf";
    {
        std::ofstream out(validFile);
        out << "theme = light\n";
        out << "accent = emerald\n";
        out << "window_width = 1000\n";
        out << "window_height = 600\n";
        out << "font_path = ~/fonts/custom.ttf\n";
        out << "font_size = 18.5\n";
        out << "binary_cache_path = ~/custom_cache.bin\n";
        out << "view_state_grace_seconds = 10\n";
    }
    FizzConfig validCfg = loadConfig(validFile.string());
    CONFIG_TEST_ASSERT(validCfg.theme == "light");
    CONFIG_TEST_ASSERT(validCfg.accent == "emerald");
    CONFIG_TEST_ASSERT(validCfg.window_width == 1000);
    CONFIG_TEST_ASSERT(validCfg.window_height == 600);
    CONFIG_TEST_ASSERT(validCfg.font_path == expectedHome + "/fonts/custom.ttf");
    CONFIG_TEST_ASSERT(validCfg.font_size == 18.5f);
    CONFIG_TEST_ASSERT(validCfg.binary_cache_path == expectedHome + "/custom_cache.bin");
    CONFIG_TEST_ASSERT(validCfg.view_state_grace_seconds == 10);

    // 2. Unrecognized theme enum falls back to "dark" without marking config unreadable
    fs::path invalidThemeFile = tempDir / "invalid_theme.conf";
    {
        std::ofstream out(invalidThemeFile);
        out << "theme = solarized_neon\n";
        out << "window_width = 800\n";
    }
    FizzConfig invalidThemeCfg = loadConfig(invalidThemeFile.string());
    CONFIG_TEST_ASSERT(invalidThemeCfg.theme == "dark");
    CONFIG_TEST_ASSERT(invalidThemeCfg.window_width == 800);

    // 3. Unrecognized accent enum falls back to "blue" without marking config unreadable
    fs::path invalidAccentFile = tempDir / "invalid_accent.conf";
    {
        std::ofstream out(invalidAccentFile);
        out << "accent = invalid_rainbow_color\n";
        out << "window_height = 500\n";
    }
    FizzConfig invalidAccentCfg = loadConfig(invalidAccentFile.string());
    CONFIG_TEST_ASSERT(invalidAccentCfg.accent == "blue");
    CONFIG_TEST_ASSERT(invalidAccentCfg.window_height == 500);

    // 4. Window width clamping (bounds: 500 to 1600)
    fs::path widthClampFile = tempDir / "width_clamp.conf";
    {
        std::ofstream out(widthClampFile);
        out << "window_width = 100\n"; // below min
    }
    FizzConfig widthClampCfg = loadConfig(widthClampFile.string());
    CONFIG_TEST_ASSERT(widthClampCfg.window_width == 500);

    {
        std::ofstream out(widthClampFile);
        out << "window_width = 5000\n"; // above max
    }
    widthClampCfg = loadConfig(widthClampFile.string());
    CONFIG_TEST_ASSERT(widthClampCfg.window_width == 1600);

    {
        std::ofstream out(widthClampFile);
        out << "window_width = not_a_number\n"; // invalid
    }
    widthClampCfg = loadConfig(widthClampFile.string());
    CONFIG_TEST_ASSERT(widthClampCfg.window_width == 750);

    // 5. Window height clamping (bounds: 320 to 1200)
    fs::path heightClampFile = tempDir / "height_clamp.conf";
    {
        std::ofstream out(heightClampFile);
        out << "window_height = 50\n"; // below min
    }
    FizzConfig heightClampCfg = loadConfig(heightClampFile.string());
    CONFIG_TEST_ASSERT(heightClampCfg.window_height == 320);

    {
        std::ofstream out(heightClampFile);
        out << "window_height = 9999\n"; // above max
    }
    heightClampCfg = loadConfig(heightClampFile.string());
    CONFIG_TEST_ASSERT(heightClampCfg.window_height == 1200);

    {
        std::ofstream out(heightClampFile);
        out << "window_height = not_a_number\n"; // invalid
    }
    heightClampCfg = loadConfig(heightClampFile.string());
    CONFIG_TEST_ASSERT(heightClampCfg.window_height == 480);

    // 6. Font size fallback on non-positive / invalid
    fs::path fontInvalidFile = tempDir / "font_invalid.conf";
    {
        std::ofstream out(fontInvalidFile);
        out << "font_size = -5.0\n";
    }
    FizzConfig fontInvalidCfg = loadConfig(fontInvalidFile.string());
    CONFIG_TEST_ASSERT(fontInvalidCfg.font_size == 16.0f);

    {
        std::ofstream out(fontInvalidFile);
        out << "font_size = huge_size\n";
    }
    fontInvalidCfg = loadConfig(fontInvalidFile.string());
    CONFIG_TEST_ASSERT(fontInvalidCfg.font_size == 16.0f);

    // 7. View state grace seconds clamping (bounds: 1 to 15, default 5)
    fs::path graceClampFile = tempDir / "grace_clamp.conf";
    {
        std::ofstream out(graceClampFile);
        out << "view_state_grace_seconds = 0\n"; // below min 1
    }
    FizzConfig graceClampCfg = loadConfig(graceClampFile.string());
    CONFIG_TEST_ASSERT(graceClampCfg.view_state_grace_seconds == 1);

    {
        std::ofstream out(graceClampFile);
        out << "view_state_grace_seconds = 100\n"; // above max 15
    }
    graceClampCfg = loadConfig(graceClampFile.string());
    CONFIG_TEST_ASSERT(graceClampCfg.view_state_grace_seconds == 15);

    {
        std::ofstream out(graceClampFile);
        out << "view_state_grace_seconds = invalid_number\n";
    }
    graceClampCfg = loadConfig(graceClampFile.string());
    CONFIG_TEST_ASSERT(graceClampCfg.view_state_grace_seconds == 5);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_bounded_fields_and_enum_fallbacks passed.\n";
}

void test_config_permissive_parsing_unrecognized_keys() {
    std::cout << "[TEST] Running test_config_permissive_parsing_unrecognized_keys...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_permissive";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    fs::path confFile = tempDir / "future_config.conf";
    {
        std::ofstream out(confFile);
        out << "# Config with recognized and forward-compatible unrecognized keys\n";
        out << "theme = light\n";
        out << "window_width = 850\n";
        out << "experimental_ai_ranking = true\n";
        out << "plugin_path = /usr/lib/fizz/plugins\n";
        out << "CustomOption = With Custom Value\n";
        out << "auto_close_on_unfocus = false\n";
    }

    FizzConfig cfg = loadConfig(confFile.string());

    // Recognized keys correctly parsed
    CONFIG_TEST_ASSERT(cfg.theme == "light");
    CONFIG_TEST_ASSERT(cfg.window_width == 850);
    CONFIG_TEST_ASSERT(cfg.autoCloseOnUnfocus == false);
    CONFIG_TEST_ASSERT(cfg.config_version == 1);

    // Unrecognized keys preserved for round-trip serialization
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys.size() == 3);
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys[0].first == "experimental_ai_ranking");
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys[0].second == "true");
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys[1].first == "plugin_path");
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys[1].second == "/usr/lib/fizz/plugins");
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys[2].first == "CustomOption");
    CONFIG_TEST_ASSERT(cfg.unrecognizedKeys[2].second == "With Custom Value");

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_permissive_parsing_unrecognized_keys passed.\n";
}

void test_config_serializer_round_trip() {
    std::cout << "[TEST] Running test_config_serializer_round_trip...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_save_roundtrip";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path confFile = tempDir / "fizz.conf";

    FizzConfig cfg;
    cfg.config_version = 1;
    cfg.searchRoots = {"/home/user/code", "/mnt/storage/data", "/opt/tools"};
    cfg.ignoreRules = {".git", "node_modules", "build", "/tmp/scratch"};
    cfg.autoCloseOnUnfocus = false;
    cfg.theme = "light";
    cfg.accent = "emerald";
    cfg.window_width = 950;
    cfg.window_height = 620;
    cfg.font_path = "/usr/share/fonts/dejavu/DejaVuSansMono.ttf";
    cfg.font_size = 18.5f;
    cfg.binary_cache_path = "/var/cache/fizz/fizz_cache.bin";
    cfg.view_state_grace_seconds = 10;

    bool saved = saveConfig(cfg, confFile.string());
    CONFIG_TEST_ASSERT(saved);
    CONFIG_TEST_ASSERT(fs::exists(confFile));

    // Verify raw text contents
    std::ifstream in(confFile);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    CONFIG_TEST_ASSERT(content.find("config_version = 1") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("search_roots = /home/user/code, /mnt/storage/data, /opt/tools") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("ignore_rules = .git, node_modules, build, /tmp/scratch") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("auto_close_on_unfocus = false") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("theme = light") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("accent = emerald") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("window_width = 950") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("window_height = 620") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("font_path = /usr/share/fonts/dejavu/DejaVuSansMono.ttf") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("font_size = 18.5") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("binary_cache_path = /var/cache/fizz/fizz_cache.bin") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("view_state_grace_seconds = 10") != std::string::npos);

    // Verify round-trip load
    FizzConfig loaded = loadConfig(confFile.string());
    CONFIG_TEST_ASSERT(loaded.config_version == 1);
    CONFIG_TEST_ASSERT(loaded.searchRoots == cfg.searchRoots);
    CONFIG_TEST_ASSERT(loaded.ignoreRules == cfg.ignoreRules);
    CONFIG_TEST_ASSERT(loaded.autoCloseOnUnfocus == cfg.autoCloseOnUnfocus);
    CONFIG_TEST_ASSERT(loaded.theme == cfg.theme);
    CONFIG_TEST_ASSERT(loaded.accent == cfg.accent);
    CONFIG_TEST_ASSERT(loaded.window_width == cfg.window_width);
    CONFIG_TEST_ASSERT(loaded.window_height == cfg.window_height);
    CONFIG_TEST_ASSERT(loaded.font_path == cfg.font_path);
    CONFIG_TEST_ASSERT(loaded.font_size == cfg.font_size);
    CONFIG_TEST_ASSERT(loaded.binary_cache_path == cfg.binary_cache_path);
    CONFIG_TEST_ASSERT(loaded.view_state_grace_seconds == cfg.view_state_grace_seconds);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_serializer_round_trip passed.\n";
}

void test_config_serializer_unrecognized_keys() {
    std::cout << "[TEST] Running test_config_serializer_unrecognized_keys...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_save_unrec";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path confFile = tempDir / "fizz.conf";

    FizzConfig cfg = FizzConfig::defaults();
    cfg.unrecognizedKeys.push_back({"experimental_ai_ranking", "true"});
    cfg.unrecognizedKeys.push_back({"custom_plugin_path", "/usr/local/lib/fizz/plugin.so"});
    cfg.unrecognizedKeys.push_back({"ArbitraryKey", "ArbitraryValue"});

    bool saved = saveConfig(cfg, confFile.string());
    CONFIG_TEST_ASSERT(saved);

    std::ifstream in(confFile);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    CONFIG_TEST_ASSERT(content.find("experimental_ai_ranking = true") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("custom_plugin_path = /usr/local/lib/fizz/plugin.so") != std::string::npos);
    CONFIG_TEST_ASSERT(content.find("ArbitraryKey = ArbitraryValue") != std::string::npos);

    FizzConfig loaded = loadConfig(confFile.string());
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys.size() == 3);
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys[0].first == "experimental_ai_ranking");
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys[0].second == "true");
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys[1].first == "custom_plugin_path");
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys[1].second == "/usr/local/lib/fizz/plugin.so");
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys[2].first == "ArbitraryKey");
    CONFIG_TEST_ASSERT(loaded.unrecognizedKeys[2].second == "ArbitraryValue");

    // Modify a known option and re-save: unrecognized keys must remain untouched
    loaded.theme = "light";
    bool savedAgain = saveConfig(loaded, confFile.string());
    CONFIG_TEST_ASSERT(savedAgain);

    FizzConfig loadedAgain = loadConfig(confFile.string());
    CONFIG_TEST_ASSERT(loadedAgain.theme == "light");
    CONFIG_TEST_ASSERT(loadedAgain.unrecognizedKeys.size() == 3);
    CONFIG_TEST_ASSERT(loadedAgain.unrecognizedKeys == loaded.unrecognizedKeys);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_serializer_unrecognized_keys passed.\n";
}

void test_config_serializer_default_path() {
    std::cout << "[TEST] Running test_config_serializer_default_path...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_save_default_path";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path customPath = tempDir / "default_fizz.conf";
    setConfigPath(customPath.string());

    FizzConfig cfg = FizzConfig::defaults();
    cfg.theme = "light";

    // Calling saveConfig without path argument should use getConfigPath()
    bool saved = saveConfig(cfg);
    CONFIG_TEST_ASSERT(saved);
    CONFIG_TEST_ASSERT(fs::exists(customPath));

    FizzConfig loaded = loadConfig(customPath.string());
    CONFIG_TEST_ASSERT(loaded.theme == "light");

    setConfigPath("");
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_serializer_default_path passed.\n";
}

void test_config_serializer_permission_modes() {
    std::cout << "[TEST] Running test_config_serializer_permission_modes...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_permissions";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    // 1. New file creation defaults to strict 0600 mode regardless of umask
    fs::path newConf = tempDir / "new_strict.conf";
    FizzConfig cfg = FizzConfig::defaults();
    bool savedNew = saveConfig(cfg, newConf.string());
    CONFIG_TEST_ASSERT(savedNew);

    struct stat stNew;
    CONFIG_TEST_ASSERT(stat(newConf.c_str(), &stNew) == 0);
    CONFIG_TEST_ASSERT((stNew.st_mode & 0777) == 0600);

    // 2. Existing file permissions are preserved (e.g. 0644)
    fs::path existingConf = tempDir / "existing_0644.conf";
    {
        std::ofstream out(existingConf);
        out << "theme = dark\n";
    }
    chmod(existingConf.c_str(), 0644);
    struct stat stInitial;
    CONFIG_TEST_ASSERT(stat(existingConf.c_str(), &stInitial) == 0);
    CONFIG_TEST_ASSERT((stInitial.st_mode & 0777) == 0644);

    cfg.theme = "light";
    bool savedExisting = saveConfig(cfg, existingConf.string());
    CONFIG_TEST_ASSERT(savedExisting);

    struct stat stPreserved;
    CONFIG_TEST_ASSERT(stat(existingConf.c_str(), &stPreserved) == 0);
    CONFIG_TEST_ASSERT((stPreserved.st_mode & 0777) == 0644);

    // 3. Creating intermediate parent directories
    fs::path nestedConf = tempDir / "sub1" / "sub2" / "nested.conf";
    bool savedNested = saveConfig(cfg, nestedConf.string());
    CONFIG_TEST_ASSERT(savedNested);
    CONFIG_TEST_ASSERT(fs::exists(nestedConf));
    struct stat stNested;
    CONFIG_TEST_ASSERT(stat(nestedConf.c_str(), &stNested) == 0);
    CONFIG_TEST_ASSERT((stNested.st_mode & 0777) == 0600);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_serializer_permission_modes passed.\n";
}

void test_config_serializer_world_writable_and_security() {
    std::cout << "[TEST] Running test_config_serializer_world_writable_and_security...\n";

    FizzConfig cfg = FizzConfig::defaults();

    // 1. Refuse write to non-existent file in world-writable directory (/tmp)
    std::string tmpNew = "/tmp/fizz_test_nonexistent_" + std::to_string(getpid()) + "_" + std::to_string(rand()) + ".conf";
    bool resNew = saveConfig(cfg, tmpNew);
    CONFIG_TEST_ASSERT(!resNew);
    std::error_code ec;
    CONFIG_TEST_ASSERT(!fs::exists(tmpNew, ec));

    // 2. Allow write to existing regular file owned by current user in /tmp
    std::string tmpExisting = "/tmp/fizz_test_existing_" + std::to_string(getpid()) + "_" + std::to_string(rand()) + ".conf";
    int fd = open(tmpExisting.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    CONFIG_TEST_ASSERT(fd >= 0);
    const char dummy[] = "config_version = 1\n";
    write(fd, dummy, sizeof(dummy) - 1);
    close(fd);

    bool resExisting = saveConfig(cfg, tmpExisting);
    CONFIG_TEST_ASSERT(resExisting);
    unlink(tmpExisting.c_str());

    // 3. Refuse write to symlink in world-writable directory
    std::string tmpSymlink = "/tmp/fizz_test_symlink_" + std::to_string(getpid()) + "_" + std::to_string(rand()) + ".conf";
    std::string symTarget = "/tmp/fizz_test_sym_target_" + std::to_string(getpid()) + "_" + std::to_string(rand()) + ".conf";
    {
        std::ofstream out(symTarget);
        out << "config_version = 1\n";
    }
    symlink(symTarget.c_str(), tmpSymlink.c_str());
    bool resSymlink = saveConfig(cfg, tmpSymlink);
    CONFIG_TEST_ASSERT(!resSymlink);
    unlink(tmpSymlink.c_str());
    unlink(symTarget.c_str());

    // 4. Refuse write to symlink in standard (non-world-writable) directory
    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_symlink_dir";
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path realTarget = tempDir / "real_file.conf";
    {
        std::ofstream out(realTarget);
        out << "config_version = 1\n";
    }
    fs::path linkPath = tempDir / "link_to_real.conf";
    symlink(realTarget.c_str(), linkPath.c_str());

    bool resSecLink = saveConfig(cfg, linkPath.string());
    CONFIG_TEST_ASSERT(!resSecLink);

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_serializer_world_writable_and_security passed.\n";
}

void test_config_serializer_migration_pre_v1_backup() {
    std::cout << "[TEST] Running test_config_serializer_migration_pre_v1_backup...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_config_migration";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    chmod(tempDir.c_str(), 0700);

    fs::path confPath = tempDir / "fizz.conf";
    fs::path backupPath = tempDir / "fizz.conf.pre-v1";

    // 1. Create an unversioned legacy config file (no config_version)
    std::string legacyContent =
        "# Legacy fizz config without schema version\n"
        "search_roots = /usr/local/bin, /opt/bin\n"
        "theme = dark\n"
        "auto_close_on_unfocus = false\n";

    {
        std::ofstream out(confPath);
        out << legacyContent;
    }
    chmod(confPath.c_str(), 0644);

    CONFIG_TEST_ASSERT(!fs::exists(backupPath));

    // 2. Saving config should create .pre-v1 backup of original legacy file
    FizzConfig newCfg;
    newCfg.config_version = 1;
    newCfg.theme = "light";
    newCfg.accent = "green";
    newCfg.searchRoots = {"/usr/local/bin", "/opt/bin"};

    bool saved = saveConfig(newCfg, confPath.string());
    CONFIG_TEST_ASSERT(saved);

    // Backup exists and contains original legacy content
    CONFIG_TEST_ASSERT(fs::exists(backupPath));
    std::ifstream bIn(backupPath);
    std::string backedUpContent((std::istreambuf_iterator<char>(bIn)), std::istreambuf_iterator<char>());
    bIn.close();
    CONFIG_TEST_ASSERT(backedUpContent == legacyContent);

    // Preserves mode on backup
    struct stat stBackup;
    CONFIG_TEST_ASSERT(stat(backupPath.c_str(), &stBackup) == 0);
    CONFIG_TEST_ASSERT((stBackup.st_mode & 0777) == 0644);

    // New file contains config_version = 1
    std::ifstream cIn(confPath);
    std::string newContent((std::istreambuf_iterator<char>(cIn)), std::istreambuf_iterator<char>());
    cIn.close();
    CONFIG_TEST_ASSERT(newContent.find("config_version = 1") != std::string::npos);
    CONFIG_TEST_ASSERT(newContent.find("theme = light") != std::string::npos);

    // 3. Second save: file already contains config_version, backup must NOT be modified
    newCfg.theme = "emerald";
    bool savedAgain = saveConfig(newCfg, confPath.string());
    CONFIG_TEST_ASSERT(savedAgain);

    std::ifstream bIn2(backupPath);
    std::string backedUpContent2((std::istreambuf_iterator<char>(bIn2)), std::istreambuf_iterator<char>());
    bIn2.close();
    CONFIG_TEST_ASSERT(backedUpContent2 == legacyContent); // Still identical to original legacy

    // 4. File that starts with config_version = 1 never creates .pre-v1
    fs::path freshVersioned = tempDir / "fresh_v1.conf";
    {
        std::ofstream out(freshVersioned);
        out << "config_version = 1\nsearch_roots = /tmp\n";
    }
    bool savedFresh = saveConfig(newCfg, freshVersioned.string());
    CONFIG_TEST_ASSERT(savedFresh);
    CONFIG_TEST_ASSERT(!fs::exists(freshVersioned.string() + ".pre-v1"));

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_serializer_migration_pre_v1_backup passed.\n";
}

void run_config_tests() {
    test_config_parser_defaults_and_missing_file();
    test_config_parser_custom_values();
    test_config_parser_corrupt_and_unreadable();
    test_config_parser_auto_close_on_unfocus();
    test_config_custom_roots_and_ignores_traversal();
    test_custom_ignore_rules_matching();
    test_user_runtime_dir_and_fallback_isolation();
    test_config_schema_version_and_defaults();
    test_config_bounded_fields_and_enum_fallbacks();
    test_config_permissive_parsing_unrecognized_keys();
    test_config_serializer_round_trip();
    test_config_serializer_unrecognized_keys();
    test_config_serializer_default_path();
    test_config_serializer_permission_modes();
    test_config_serializer_world_writable_and_security();
    test_config_serializer_migration_pre_v1_backup();
}
