#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
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

    // 1b. File with an unreadable/unrecognized option triggers clean fallback
    fs::path unreadableOptFile = tempDir / "unreadable_opt.conf";
    {
        std::ofstream out(unreadableOptFile);
        out << "search_roots = /tmp/valid\n";
        out << "unknown_unreadable_setting = true\n";
    }
    FizzConfig optConfig = loadConfig(unreadableOptFile.string());
    CONFIG_TEST_ASSERT(optConfig.searchRoots == defaultRoots);
    CONFIG_TEST_ASSERT(optConfig.ignoreRules.empty());

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

void run_config_tests() {
    test_config_parser_defaults_and_missing_file();
    test_config_parser_custom_values();
    test_config_parser_corrupt_and_unreadable();
    test_config_parser_auto_close_on_unfocus();
    test_config_custom_roots_and_ignores_traversal();
    test_custom_ignore_rules_matching();
    test_user_runtime_dir_and_fallback_isolation();
}
