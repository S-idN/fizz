#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include "services/FileSearch.h"
#include "services/LoadFile.h"

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

int main() {
    test_file_classification();
    test_default_search_roots();
    test_dual_search_roots_and_traversal();
    test_binary_cache_path_resolution();
    test_binary_cache_sandboxed_roundtrip();
    test_binary_cache_corruption_detection();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
