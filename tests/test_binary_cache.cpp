#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include "services/FileSearch.h"
#include "services/LoadFile.h"
#include "services/Config.h"

namespace fs = std::filesystem;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_binary_cache_path_resolution() {
    std::cout << "[TEST] Running test_binary_cache_path_resolution...\n";

    const char* home = std::getenv("HOME");
    std::string expectedHomeCache = std::string(home ? home : ".") + "/.fizzLocalCache.bin";

    // 1. By default, getBinaryCachePath should match $HOME/.fizzLocalCache.bin
    TEST_ASSERT(getBinaryCachePath() == expectedHomeCache);

    // 2. Setting an explicit override path
    std::string customPath = "/tmp/test_fizz_override.bin";
    setBinaryCachePath(customPath);
    TEST_ASSERT(getBinaryCachePath() == customPath);

    // 3. Resetting override (passing empty string) restores $HOME default
    setBinaryCachePath("");
    TEST_ASSERT(getBinaryCachePath() == expectedHomeCache);

    std::cout << "[PASS] test_binary_cache_path_resolution passed.\n";
}

void test_binary_cache_sandboxed_roundtrip() {
    std::cout << "[TEST] Running test_binary_cache_sandboxed_roundtrip...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_sandbox_roundtrip";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir / "bin", ec);
    fs::create_directories(sandboxDir / "docs", ec);

    fs::path execFilePath = sandboxDir / "bin" / "tool.sh";
    {
        std::ofstream out(execFilePath);
        out << "#!/bin/sh\necho hello\n";
    }
    chmod(execFilePath.c_str(), 0755);

    fs::path docFilePath = sandboxDir / "docs" / "note.txt";
    {
        std::ofstream out(docFilePath);
        out << "plain note\n";
    }
    chmod(docFilePath.c_str(), 0644);

    fs::path cacheFilePath = sandboxDir / "custom_cache.bin";

    // Set binary cache path override
    setBinaryCachePath(cacheFilePath.string());

    // Execute system file search in the sandbox
    systemFileSearch(sandboxDir.string());

    // Verify cache file was created at the overridden path
    TEST_ASSERT(fs::exists(cacheFilePath));
    TEST_ASSERT(fs::file_size(cacheFilePath) > 0);

    // Load the cache into memory
    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded == true);
    TEST_ASSERT(!fileStructTable.empty());
    TEST_ASSERT(!g_InMemoryIndexBuffer.empty());

    // Verify contents
    bool foundTool = false;
    bool foundDoc = false;

    for (const auto& ent : fileStructTable) {
        TEST_ASSERT(ent.f_offset < g_InMemoryIndexBuffer.size());
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string name(namePtr);

        if (name.find("tool.sh") != std::string::npos) {
            foundTool = true;
            TEST_ASSERT(ent.f_execState == ExecState::Executable);
        } else if (name.find("note.txt") != std::string::npos) {
            foundDoc = true;
            TEST_ASSERT(ent.f_execState == ExecState::NotExecutable);
        }
    }

    TEST_ASSERT(foundTool);
    TEST_ASSERT(foundDoc);

    // Reset override and clean up
    setBinaryCachePath("");
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_binary_cache_sandboxed_roundtrip passed.\n";
}

void test_binary_cache_corruption_detection() {
    std::cout << "[TEST] Running test_binary_cache_corruption_detection...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_sandbox_corrupt";
    std::error_code ec;
    fs::create_directories(sandboxDir, ec);

    // 1. Truncated header (less than 16 bytes)
    fs::path truncatedHeaderFile = sandboxDir / "truncated_header.bin";
    {
        std::ofstream out(truncatedHeaderFile, std::ios::binary);
        uint32_t val = 42;
        out.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    setBinaryCachePath(truncatedHeaderFile.string());
    bool headerLoadSuccess = loadIndexToMemory();
    TEST_ASSERT(!headerLoadSuccess);
    TEST_ASSERT(fileStructTable.empty());
    TEST_ASSERT(g_InMemoryIndexBuffer.empty());

    // 2. Truncated body (header specifies more data than present on disk)
    fs::path truncatedBodyFile = sandboxDir / "truncated_body.bin";
    {
        std::ofstream out(truncatedBodyFile, std::ios::binary);
        size_t structCount = 100;
        size_t charCount = 5000;
        out.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
        out.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
        uint32_t partial = 123;
        out.write(reinterpret_cast<const char*>(&partial), sizeof(partial));
    }
    setBinaryCachePath(truncatedBodyFile.string());
    bool bodyLoadSuccess = loadIndexToMemory();
    TEST_ASSERT(!bodyLoadSuccess);
    TEST_ASSERT(fileStructTable.empty());
    TEST_ASSERT(g_InMemoryIndexBuffer.empty());

    // 3. Corrupt giant count header (guard against bad_alloc crash)
    fs::path hugeCountFile = sandboxDir / "huge_count.bin";
    {
        std::ofstream out(hugeCountFile, std::ios::binary);
        size_t structCount = 0xFFFFFFFFFFFFFFULL;
        size_t charCount = 100;
        out.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
        out.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
    }
    setBinaryCachePath(hugeCountFile.string());
    bool hugeLoadSuccess = loadIndexToMemory();
    TEST_ASSERT(!hugeLoadSuccess);
    TEST_ASSERT(fileStructTable.empty());
    TEST_ASSERT(g_InMemoryIndexBuffer.empty());

    setBinaryCachePath("");
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_binary_cache_corruption_detection passed.\n";
}

void test_file_classification() {
    std::cout << "[TEST] Running test_file_classification...\n";

    // Desktop entries should always be classified as "app" regardless of exec state
    TEST_ASSERT(classifyPath("/usr/share/applications/firefox.desktop", ExecState::NotExecutable) == "app");
    TEST_ASSERT(classifyPath("/home/user/.local/share/applications/custom.desktop", ExecState::Executable) == "app");
    TEST_ASSERT(classifyPath("TEST.DESKTOP", ExecState::NotExecutable) == "app");

    // Executables
    TEST_ASSERT(classifyPath("/usr/bin/ls", ExecState::Executable) == "exec");

    // Standard file extensions
    TEST_ASSERT(classifyPath("/home/user/document.txt", ExecState::NotExecutable) == "text");
    TEST_ASSERT(classifyPath("/home/user/src/main.cpp", ExecState::NotExecutable) == "code");
    TEST_ASSERT(classifyPath("/home/user/photo.png", ExecState::NotExecutable) == "image");
    TEST_ASSERT(classifyPath("/home/user/song.mp3", ExecState::NotExecutable) == "media");
    TEST_ASSERT(classifyPath("/home/user/manual.pdf", ExecState::NotExecutable) == "doc");
    TEST_ASSERT(classifyPath("/home/user/bundle.tar.gz", ExecState::NotExecutable) == "archive");

    // Unrecognized or extensionless non-executables
    TEST_ASSERT(classifyPath("/home/user/random_file", ExecState::NotExecutable) == "");

    std::cout << "[PASS] test_file_classification passed.\n";
}

void test_default_search_roots() {
    std::cout << "[TEST] Running test_default_search_roots...\n";

    auto roots = getDefaultSearchRoots();
    TEST_ASSERT(!roots.empty());

    const char* home = std::getenv("HOME");
    std::string expectedHome = home ? home : "";

    bool foundHome = false;
    bool foundApps = false;
    bool foundRootSlash = false;

    for (const auto& root : roots) {
        if (!expectedHome.empty() && root == expectedHome) {
            foundHome = true;
        }
        if (root.find("applications") != std::string::npos) {
            foundApps = true;
        }
        if (root == "/") {
            foundRootSlash = true;
        }
    }

    TEST_ASSERT(foundHome);
    TEST_ASSERT(foundApps);
    TEST_ASSERT(!foundRootSlash);

    std::cout << "[PASS] test_default_search_roots passed.\n";
}

void test_dual_search_roots_and_traversal() {
    std::cout << "[TEST] Running test_dual_search_roots_and_traversal...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_dual_roots";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);

    fs::path userHome = sandboxDir / "user_home";
    fs::path appDir = sandboxDir / "app_dir";

    fs::create_directories(userHome / "work", ec);
    fs::create_directories(userHome / ".cache", ec);
    fs::create_directories(userHome / ".git", ec);
    fs::create_directories(userHome / "restricted_dir", ec);
    fs::create_directories(appDir / "sub", ec);

    // Create user files
    {
        std::ofstream out(userHome / "file1.txt");
        out << "hello\n";
    }
    {
        std::ofstream out(userHome / "work" / "project.cpp");
        out << "int main() {}\n";
    }
    {
        std::ofstream out(userHome / ".cache" / "cached_data.tmp");
        out << "temp\n";
    }
    {
        std::ofstream out(userHome / ".git" / "config");
        out << "[core]\n";
    }

    // Create desktop entries in appDir
    {
        std::ofstream out(appDir / "editor.desktop");
        out << "[Desktop Entry]\nName=Editor\nType=Application\n";
    }
    {
        std::ofstream out(appDir / "sub" / "terminal.desktop");
        out << "[Desktop Entry]\nName=Terminal\nType=Application\n";
    }
    // Simulate Flatpak symlink export
    fs::create_symlink(appDir / "sub" / "terminal.desktop", appDir / "flatpak_app.desktop", ec);

    // Make restricted_dir unreadable (permission denied)
    {
        std::ofstream out(userHome / "restricted_dir" / "secret.txt");
        out << "classified\n";
    }
    chmod((userHome / "restricted_dir").c_str(), 0000);

    fs::path cachePath = sandboxDir / "dual_roots_cache.bin";
    setBinaryCachePath(cachePath.string());

    // Search dual roots
    std::vector<std::string> searchRoots = { userHome.string(), appDir.string() };
    systemFileSearch(searchRoots);

    // Verify Binary Cache was generated
    TEST_ASSERT(fs::exists(cachePath));
    TEST_ASSERT(fs::file_size(cachePath) > 0);

    // Load and verify contents
    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded);

    bool foundFile1 = false;
    bool foundProject = false;
    bool foundEditorDesktop = false;
    bool foundTerminalDesktop = false;
    bool foundFlatpakSymlink = false;
    bool foundIgnoredCache = false;
    bool foundIgnoredGit = false;
    int projectCount = 0;

    for (const auto& ent : fileStructTable) {
        TEST_ASSERT(ent.f_offset < g_InMemoryIndexBuffer.size());
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string pathStr(namePtr);

        if (pathStr.find("file1.txt") != std::string::npos) foundFile1 = true;
        if (pathStr.find("project.cpp") != std::string::npos) {
            foundProject = true;
            projectCount++;
        }
        if (pathStr.find("editor.desktop") != std::string::npos) {
            foundEditorDesktop = true;
            TEST_ASSERT(classifyPath(pathStr, ent.f_execState) == "app");
        }
        if (pathStr.find("terminal.desktop") != std::string::npos) {
            foundTerminalDesktop = true;
            TEST_ASSERT(classifyPath(pathStr, ent.f_execState) == "app");
        }
        if (pathStr.find("flatpak_app.desktop") != std::string::npos) {
            foundFlatpakSymlink = true;
            TEST_ASSERT(classifyPath(pathStr, ent.f_execState) == "app");
        }
        if (pathStr.find(".cache") != std::string::npos) foundIgnoredCache = true;
        if (pathStr.find(".git") != std::string::npos) foundIgnoredGit = true;
    }

    TEST_ASSERT(foundFile1);
    TEST_ASSERT(foundProject);
    TEST_ASSERT(foundEditorDesktop);
    TEST_ASSERT(foundTerminalDesktop);
    TEST_ASSERT(foundFlatpakSymlink);
    TEST_ASSERT(!foundIgnoredCache);
    TEST_ASSERT(!foundIgnoredGit);
    TEST_ASSERT(projectCount == 1);

    // Test deduplication with overlapping roots
    std::vector<std::string> overlappingRoots = { userHome.string(), (userHome / "work").string() };
    systemFileSearch(overlappingRoots);
    loadIndexToMemory();

    int dupProjectCount = 0;
    for (const auto& ent : fileStructTable) {
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string pathStr(namePtr);
        if (pathStr.find("project.cpp") != std::string::npos) {
            dupProjectCount++;
        }
    }
    TEST_ASSERT(dupProjectCount == 1); // Deduplication prevents redundant indexing

    // Restore permissions for cleanup
    chmod((userHome / "restricted_dir").c_str(), 0755);
    setBinaryCachePath("");
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_dual_search_roots_and_traversal passed.\n";
}

void test_config_parser_defaults_and_missing_file() {
    std::cout << "[TEST] Running test_config_parser_defaults_and_missing_file...\n";

    const char* home = std::getenv("HOME");
    std::string expectedHome = home ? home : "";
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    std::string expectedConfig = (xdgConfigHome && xdgConfigHome[0] != '\0')
        ? std::string(xdgConfigHome) + "/fizz/fizz.conf"
        : expectedHome + "/.config/fizz/fizz.conf";

    // 1. By default, getConfigPath() resolves to expected ~/.config/fizz/fizz.conf
    TEST_ASSERT(getConfigPath() == expectedConfig);

    // 2. setConfigPath override
    std::string overridePath = "/tmp/nonexistent_fizz.conf";
    setConfigPath(overridePath);
    TEST_ASSERT(getConfigPath() == overridePath);

    // 3. Loading missing config falls back cleanly to defaults without error
    FizzConfig config = loadConfig(overridePath);
    auto defaultRoots = getDefaultSearchRoots(true);
    TEST_ASSERT(config.searchRoots == defaultRoots);
    TEST_ASSERT(config.ignoreRules.empty());

    // 4. Resetting config path override restores default
    setConfigPath("");
    TEST_ASSERT(getConfigPath() == expectedConfig);

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

    TEST_ASSERT(config.searchRoots.size() == 3);
    TEST_ASSERT(config.searchRoots[0] == expectedExpandedHome);
    TEST_ASSERT(config.searchRoots[1] == "/mnt/storage");
    TEST_ASSERT(config.searchRoots[2] == "/opt/tools"); // normalized without trailing slash

    TEST_ASSERT(config.ignoreRules.size() == 4);
    TEST_ASSERT(config.ignoreRules[0] == ".git");
    TEST_ASSERT(config.ignoreRules[1] == "node_modules");
    TEST_ASSERT(config.ignoreRules[2] == "build");
    TEST_ASSERT(config.ignoreRules[3] == "/data/tmp");

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
    TEST_ASSERT(config.searchRoots == defaultRoots);
    TEST_ASSERT(config.ignoreRules.empty());

    // 1b. File with an unreadable/unrecognized option triggers clean fallback
    fs::path unreadableOptFile = tempDir / "unreadable_opt.conf";
    {
        std::ofstream out(unreadableOptFile);
        out << "search_roots = /tmp/valid\n";
        out << "unknown_unreadable_setting = true\n";
    }
    FizzConfig optConfig = loadConfig(unreadableOptFile.string());
    TEST_ASSERT(optConfig.searchRoots == defaultRoots);
    TEST_ASSERT(optConfig.ignoreRules.empty());

    // 2. Binary / unreadable data
    fs::path binConf = tempDir / "binary.conf";
    {
        std::ofstream out(binConf, std::ios::binary);
        char binData[] = { '\0', '\1', '\x80', '\xFF', '\x05' };
        out.write(binData, sizeof(binData));
    }

    FizzConfig binConfig = loadConfig(binConf.string());
    TEST_ASSERT(binConfig.searchRoots == defaultRoots);
    TEST_ASSERT(binConfig.ignoreRules.empty());

    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_config_parser_corrupt_and_unreadable passed.\n";
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
    fs::create_directories(customRoot1 / "build_prefix" / "build", ec); // tests recurring component search
    fs::create_directories(customRoot1 / ".git", ec); // default ignore
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
    TEST_ASSERT(config.searchRoots.size() == 2);
    TEST_ASSERT(config.ignoreRules.size() == 3);

    setCustomIgnoreRules(config.ignoreRules);
    systemFileSearch(config.searchRoots);

    TEST_ASSERT(fs::exists(cachePath));
    TEST_ASSERT(fs::file_size(cachePath) > 0);

    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded);

    bool foundFileA = false;
    bool foundFileB = false;
    bool foundAppDesktop = false;
    bool foundSecret = false;
    bool foundGitHead = false;
    bool foundPrefixIgnored = false;
    bool foundBuildArtifact = false;

    for (const auto& ent : fileStructTable) {
        TEST_ASSERT(ent.f_offset < g_InMemoryIndexBuffer.size());
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string pathStr(namePtr);

        if (pathStr.find("file_a.txt") != std::string::npos) foundFileA = true;
        if (pathStr.find("file_b.txt") != std::string::npos) foundFileB = true;
        if (pathStr.find("app.desktop") != std::string::npos) {
            foundAppDesktop = true;
            TEST_ASSERT(classifyPath(pathStr, ent.f_execState) == "app");
        }
        if (pathStr.find("secret.txt") != std::string::npos) foundSecret = true;
        if (pathStr.find("HEAD") != std::string::npos) foundGitHead = true;
        if (pathStr.find("should_not_index.txt") != std::string::npos) foundPrefixIgnored = true;
        if (pathStr.find("nested_build_artifact.o") != std::string::npos) foundBuildArtifact = true;
    }

    TEST_ASSERT(foundFileA);
    TEST_ASSERT(foundFileB);
    TEST_ASSERT(foundAppDesktop);
    TEST_ASSERT(!foundSecret);        // Custom Ignore Rule (directory name)
    TEST_ASSERT(!foundPrefixIgnored); // Custom Ignore Rule (path prefix)
    TEST_ASSERT(!foundGitHead);       // Default Ignore Rule (.git)
    TEST_ASSERT(!foundBuildArtifact); // Custom Ignore Rule with prior partial component match

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
    TEST_ASSERT(isPathIgnored("/var/log/custom", "custom"));
    TEST_ASSERT(isPathIgnored("/var/log/custom/app.log", "app.log"));
    TEST_ASSERT(isPathIgnored("/var/log/custom/nested/file.txt", "file.txt"));
    // Must NOT match prefix that is merely a substring / shared prefix of directory name
    TEST_ASSERT(!isPathIgnored("/var/log/custom_other/file.txt", "file.txt"));

    // 2. Component / directory name rules
    TEST_ASSERT(isPathIgnored("/home/user/rust/target/debug/bin", "bin"));
    TEST_ASSERT(isPathIgnored("/home/user/rust/target", "target"));
    TEST_ASSERT(isPathIgnored("", "target"));
    TEST_ASSERT(isPathIgnored("/home/user/project/build/output.o", "output.o"));
    // Must NOT match partial component name
    TEST_ASSERT(!isPathIgnored("/home/user/rust/target_backup/bin", "bin"));
    TEST_ASSERT(!isPathIgnored("/home/user/build_dir/output.o", "output.o"));

    // 3. Default ignore rules must remain active
    TEST_ASSERT(isPathIgnored("/home/user/.git/config", "config"));
    TEST_ASSERT(isPathIgnored("", ".git"));
    TEST_ASSERT(isPathIgnored("/home/user/.cache/app", "app"));
    TEST_ASSERT(isPathIgnored("/home/user/node_modules/pkg", "pkg"));

    // Reset custom ignore rules
    setCustomIgnoreRules({});
    TEST_ASSERT(!isPathIgnored("/var/log/custom/app.log", "app.log"));
    TEST_ASSERT(!isPathIgnored("/home/user/rust/target/debug/bin", "bin"));

    std::cout << "[PASS] test_custom_ignore_rules_matching passed.\n";
}

int main() {
    test_config_parser_defaults_and_missing_file();
    test_config_parser_custom_values();
    test_config_parser_corrupt_and_unreadable();
    test_config_custom_roots_and_ignores_traversal();
    test_custom_ignore_rules_matching();
    test_file_classification();
    test_default_search_roots();
    test_dual_search_roots_and_traversal();
    test_binary_cache_path_resolution();
    test_binary_cache_sandboxed_roundtrip();
    test_binary_cache_corruption_detection();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
