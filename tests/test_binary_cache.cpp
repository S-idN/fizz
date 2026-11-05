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

int main() {
    test_binary_cache_path_resolution();
    test_binary_cache_sandboxed_roundtrip();
    test_binary_cache_corruption_detection();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
