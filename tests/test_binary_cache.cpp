#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include "services/FileSearch.h"
#include "services/LoadFile.h"
#include "services/QueryOps.h"

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

    // Verify Binary Cache was created at the overridden path
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

        // After lazy traversal, f_execState is Unknown until a search hit resolves it
        if (name.find("tool.sh") != std::string::npos) {
            foundTool = true;
            TEST_ASSERT(ent.f_execState == ExecState::Unknown ||
                        ent.f_execState == ExecState::Executable);
        } else if (name.find("note.txt") != std::string::npos) {
            foundDoc = true;
            TEST_ASSERT(ent.f_execState == ExecState::Unknown ||
                        ent.f_execState == ExecState::NotExecutable);
        }
    }

    TEST_ASSERT(foundTool);
    TEST_ASSERT(foundDoc);

    // Trigger lazy exec resolution via searchPaths and verify exec state is resolved correctly
    std::vector<SearchHit> hits;
    searchPaths("tool", hits, 10);
    bool toolHitFound = false;
    for (const auto& hit : hits) {
        if (hit.path.find("tool.sh") != std::string::npos) {
            toolHitFound = true;
            TEST_ASSERT(hit.execState == ExecState::Executable);
        }
    }
    TEST_ASSERT(toolHitFound);

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

    // Simulate cyclic directory symlink and external directory symlink
    fs::create_directory_symlink(userHome, userHome / "recursive_dir_loop", ec);
    fs::path outsideDir = sandboxDir / "outside_dir";
    fs::create_directories(outsideDir, ec);
    {
        std::ofstream out(outsideDir / "secret_outside.txt");
        out << "classified\n";
    }
    fs::create_directory_symlink(outsideDir, userHome / "outside_dir_symlink", ec);

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
    bool foundOutsideSecret = false;
    bool foundDirLoop = false;
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
        if (pathStr.find("secret_outside.txt") != std::string::npos) foundOutsideSecret = true;
        if (pathStr.find("recursive_dir_loop") != std::string::npos) foundDirLoop = true;
    }

    TEST_ASSERT(foundFile1);
    TEST_ASSERT(foundProject);
    TEST_ASSERT(foundEditorDesktop);
    TEST_ASSERT(foundTerminalDesktop);
    TEST_ASSERT(foundFlatpakSymlink);
    TEST_ASSERT(!foundIgnoredCache);
    TEST_ASSERT(!foundIgnoredGit);
    TEST_ASSERT(!foundOutsideSecret);
    TEST_ASSERT(!foundDirLoop);
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

void test_query_matcher_end_to_end() {
    std::cout << "[TEST] Running test_query_matcher_end_to_end...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_query_matcher";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir / "docs", ec);
    fs::create_directories(sandboxDir / "apps", ec);

    // Create files with distinctive names
    {
        std::ofstream out(sandboxDir / "docs" / "user_guide.txt");
        out << "manual\n";
    }
    {
        std::ofstream out(sandboxDir / "docs" / "development_plan.md");
        out << "plan\n";
    }
    {
        std::ofstream out(sandboxDir / "apps" / "calculator.desktop");
        out << "[Desktop Entry]\nName=Calculator\nType=Application\n";
    }
    {
        std::ofstream out(sandboxDir / "apps" / "terminal.desktop");
        out << "[Desktop Entry]\nName=Terminal\nType=Application\n";
    }
    {
        std::ofstream out(sandboxDir / "script.sh");
        out << "#!/bin/sh\necho hi\n";
    }
    chmod((sandboxDir / "script.sh").c_str(), 0755);

    fs::path cachePath = sandboxDir / "query_cache.bin";
    setBinaryCachePath(cachePath.string());

    // 1. Index the sandbox directory
    systemFileSearch(sandboxDir.string());
    TEST_ASSERT(fs::exists(cachePath));

    // 2. Load into memory
    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded);

    // 3. Query: "calc" -> should match calculator.desktop
    std::vector<SearchHit> results;
    searchPaths("calc", results, 10);
    TEST_ASSERT(!results.empty());
    bool foundCalc = false;
    for (const auto& hit : results) {
        TEST_ASSERT(hit.score > 0);
        if (hit.path.find("calculator.desktop") != std::string_view::npos) {
            foundCalc = true;
            TEST_ASSERT(classifyPath(hit.path, hit.execState) == "app");
        }
    }
    TEST_ASSERT(foundCalc);

    // 4. Query: "script" -> should match script.sh and indicate Executable
    results.clear();
    searchPaths("script", results, 10);
    TEST_ASSERT(!results.empty());
    bool foundScript = false;
    for (const auto& hit : results) {
        if (hit.path.find("script.sh") != std::string_view::npos) {
            foundScript = true;
            TEST_ASSERT(hit.execState == ExecState::Executable);
        }
    }
    TEST_ASSERT(foundScript);

    // 5. Query: "guide" -> should match user_guide.txt
    results.clear();
    searchPaths("guide", results, 10);
    TEST_ASSERT(!results.empty());
    bool foundGuide = false;
    for (const auto& hit : results) {
        if (hit.path.find("user_guide.txt") != std::string_view::npos) {
            foundGuide = true;
        }
    }
    TEST_ASSERT(foundGuide);

    // 6. Query with no matches
    results.clear();
    searchPaths("nonexistent_pattern_xyz_123", results, 10);
    TEST_ASSERT(results.empty());

    // Cleanup
    setBinaryCachePath("");
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_query_matcher_end_to_end passed.\n";
}

void test_binary_cache_atomic_publication_and_permissions() {
    std::cout << "[TEST] Running test_binary_cache_atomic_publication_and_permissions...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_sandbox_atomic_perms";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir / "data", ec);

    fs::path sampleFile = sandboxDir / "data" / "item.txt";
    {
        std::ofstream out(sampleFile);
        out << "content\n";
    }

    fs::path cacheFilePath = sandboxDir / "cache.bin";
    setBinaryCachePath(cacheFilePath.string());

    // Temporarily set a very permissive umask to verify strict 0600 enforcement
    mode_t oldUmask = umask(0000);

    systemFileSearch(sandboxDir.string());

    umask(oldUmask);

    // 1. Verify destination cache exists and has non-zero size
    TEST_ASSERT(fs::exists(cacheFilePath));
    TEST_ASSERT(fs::file_size(cacheFilePath) > 0);

    // 2. Sibling temporary file .tmp.<pid> should have been cleaned up / renamed
    std::string expectedTmpPath = cacheFilePath.string() + ".tmp." + std::to_string(getpid());
    TEST_ASSERT(!fs::exists(expectedTmpPath));

    // 3. Verify permissions are strictly 0600 (owner read/write only)
    struct stat st;
    TEST_ASSERT(stat(cacheFilePath.c_str(), &st) == 0);
    mode_t filePerms = st.st_mode & 0777;
    TEST_ASSERT(filePerms == 0600);

    // 4. Test atomic replacement of an existing cache
    ino_t oldInode = st.st_ino;
    systemFileSearch(sandboxDir.string());
    struct stat stNew;
    TEST_ASSERT(stat(cacheFilePath.c_str(), &stNew) == 0);
    TEST_ASSERT((stNew.st_mode & 0777) == 0600);
    // Because atomic rename replaced the file with the newly created temp file, inode changed
    TEST_ASSERT(stNew.st_ino != oldInode);

    // 5. Test that existing valid cache remains uncorrupted if writing fails
    std::string validContent;
    {
        std::ifstream in(cacheFilePath, std::ios::binary);
        validContent.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    // Point cache to an unwritable directory to trigger open failure
    fs::path unwriteableDir = sandboxDir / "unwriteable";
    fs::create_directories(unwriteableDir, ec);
    fs::path unwriteableCache = unwriteableDir / "target.bin";
    {
        std::ofstream out(unwriteableCache);
        out << "original content";
    }
    chmod(unwriteableDir.c_str(), 0500); // read-only directory: cannot create files in it
    setBinaryCachePath(unwriteableCache.string());
    systemFileSearch(sandboxDir.string());
    // unwriteableCache must not have been truncated or corrupted
    {
        std::ifstream in(unwriteableCache);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        TEST_ASSERT(content == "original content");
    }
    chmod(unwriteableDir.c_str(), 0755);

    setBinaryCachePath("");
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_binary_cache_atomic_publication_and_permissions passed.\n";
}

void test_binary_cache_deserialization_validation() {
    std::cout << "[TEST] Running test_binary_cache_deserialization_validation...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_sandbox_deser_bounds";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    auto writeBinaryCache = [](const fs::path& p, const std::vector<fileEntity>& entries, const std::string& strPool) {
        std::ofstream out(p, std::ios::binary);
        size_t structCount = entries.size();
        size_t charCount = strPool.size();
        out.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
        out.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
        if (structCount > 0) {
            out.write(reinterpret_cast<const char*>(entries.data()), structCount * sizeof(fileEntity));
        }
        if (charCount > 0) {
            out.write(strPool.data(), charCount);
        }
    };

    auto assertCorruptLoad = [&](const fs::path& p, const std::vector<fileEntity>& entries, const std::string& strPool) {
        writeBinaryCache(p, entries, strPool);
        setBinaryCachePath(p.string());
        bool loaded = loadIndexToMemory();
        TEST_ASSERT(!loaded);
        TEST_ASSERT(fileStructTable.empty());
        TEST_ASSERT(g_InMemoryIndexBuffer.empty());
    };

    // 1. Entry offset completely out of bounds (f_offset >= charCount)
    {
        fileEntity ent{};
        ent.f_offset = 100; // string pool is only 10 bytes
        ent.f_length = 5;
        assertCorruptLoad(sandboxDir / "oob_offset.bin", {ent}, std::string(10, 'a'));
    }

    // 2. Entry length zero
    {
        fileEntity ent{};
        ent.f_offset = 0;
        ent.f_length = 0;
        std::string pool = "hello\0";
        assertCorruptLoad(sandboxDir / "zero_length.bin", {ent}, std::string(pool.c_str(), 6));
    }

    // 3. Entry offset + length exceeds buffer bounds
    {
        fileEntity ent{};
        ent.f_offset = 5;
        ent.f_length = 10; // offset 5 + length 10 = 15 > 10
        assertCorruptLoad(sandboxDir / "oob_length.bin", {ent}, std::string(10, 'a'));
    }

    // 4. Arithmetic overflow in f_offset + f_length
    {
        fileEntity ent{};
        ent.f_offset = SIZE_MAX - 2;
        ent.f_length = 10;
        assertCorruptLoad(sandboxDir / "overflow_bounds.bin", {ent}, std::string(10, 'a'));
    }

    // 5. Missing null termination (last byte in slice is not '\0')
    {
        fileEntity ent{};
        ent.f_offset = 0;
        ent.f_length = 5;
        assertCorruptLoad(sandboxDir / "not_null_terminated.bin", {ent}, "abcde");
    }

    // 6. Premature null termination inside entry
    {
        fileEntity ent{};
        ent.f_offset = 0;
        ent.f_length = 6;
        const char rawPool[6] = {'a', 'b', '\0', 'd', 'e', '\0'};
        assertCorruptLoad(sandboxDir / "premature_null.bin", {ent}, std::string(rawPool, 6));
    }

    // 7. Malformed header: charCount arithmetic overflow relative to structBytes
    {
        fs::path p = sandboxDir / "header_char_overflow.bin";
        std::ofstream out(p, std::ios::binary);
        size_t structCount = 1;
        size_t charCount = SIZE_MAX - sizeof(fileEntity) + 1; // causes structBytes + charCount to overflow
        out.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
        out.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
        fileEntity dummy{};
        out.write(reinterpret_cast<const char*>(&dummy), sizeof(dummy));
        out.close();

        setBinaryCachePath(p.string());
        bool loaded = loadIndexToMemory();
        TEST_ASSERT(!loaded);
        TEST_ASSERT(fileStructTable.empty());
        TEST_ASSERT(g_InMemoryIndexBuffer.empty());
    }

    setBinaryCachePath("");
    fs::remove_all(sandboxDir, ec);

    std::cout << "[PASS] test_binary_cache_deserialization_validation passed.\n";
}

// ---------------------------------------------------------------------------
// Seam 1: sensitive directory exclusion
// ---------------------------------------------------------------------------
void test_sensitive_directory_exclusion() {
    std::cout << "[TEST] Running test_sensitive_directory_exclusion...\n";

    fs::path sandbox = fs::temp_directory_path() / "fizz_test_sensitive_excl";
    std::error_code ec;
    fs::remove_all(sandbox, ec);

    // Sensitive directories that must be excluded by default
    struct SensitiveDir { const char* rel; const char* file; };
    static const SensitiveDir sensitiveDirs[] = {
        { ".ssh",                   "id_rsa"            },
        { ".gnupg",                 "secring.gpg"       },
        { ".aws",                   "credentials"       },
        { ".azure",                 "credentials"       },
        { ".config/gcloud",         "access_token.db"   },
        { ".local/share/keyrings",  "login.keyring"     },
    };

    for (const auto& sd : sensitiveDirs) {
        fs::create_directories(sandbox / sd.rel, ec);
        std::ofstream(sandbox / sd.rel / sd.file) << "secret\n";
    }

    // A regular (non-sensitive) file that SHOULD be found
    fs::create_directories(sandbox / "docs", ec);
    std::ofstream(sandbox / "docs" / "readme.txt") << "hello\n";

    fs::path cachePath = sandbox / "sensitive_cache.bin";
    setBinaryCachePath(cachePath.string());
    setCustomIgnoreRules({});   // ensure no leftover custom rules

    systemFileSearch(sandbox.string());

    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded);

    bool foundReadme = false;
    for (const auto& ent : fileStructTable) {
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string path(namePtr);

        // None of the secret files should appear in results
        TEST_ASSERT(path.find("id_rsa")           == std::string::npos);
        TEST_ASSERT(path.find("secring.gpg")       == std::string::npos);
        TEST_ASSERT(path.find(".aws")              == std::string::npos);
        TEST_ASSERT(path.find(".azure")            == std::string::npos);
        TEST_ASSERT(path.find("access_token.db")   == std::string::npos);
        TEST_ASSERT(path.find("login.keyring")     == std::string::npos);

        if (path.find("readme.txt") != std::string::npos)
            foundReadme = true;
    }

    TEST_ASSERT(foundReadme);

    setBinaryCachePath("");
    fs::remove_all(sandbox, ec);

    std::cout << "[PASS] test_sensitive_directory_exclusion passed.\n";
}

// ---------------------------------------------------------------------------
// Seam 1 (composition): user custom rules still apply alongside defaults
// ---------------------------------------------------------------------------
void test_custom_ignore_rules_compose_with_defaults() {
    std::cout << "[TEST] Running test_custom_ignore_rules_compose_with_defaults...\n";

    fs::path sandbox = fs::temp_directory_path() / "fizz_test_custom_compose";
    std::error_code ec;
    fs::remove_all(sandbox, ec);

    // A file in a default-excluded directory
    fs::create_directories(sandbox / ".ssh", ec);
    std::ofstream(sandbox / ".ssh" / "id_rsa") << "secret\n";

    // A file in a user-excluded directory
    fs::create_directories(sandbox / "private_stuff", ec);
    std::ofstream(sandbox / "private_stuff" / "token.txt") << "secret\n";

    // A file that should be found
    fs::create_directories(sandbox / "public", ec);
    std::ofstream(sandbox / "public" / "hello.txt") << "world\n";

    fs::path cachePath = sandbox / "compose_cache.bin";
    setBinaryCachePath(cachePath.string());

    // Set a custom rule that excludes "private_stuff"
    setCustomIgnoreRules({"private_stuff"});
    systemFileSearch(sandbox.string());
    // Clear custom rules after use so other tests are unaffected
    setCustomIgnoreRules({});

    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded);

    bool foundHello = false;
    for (const auto& ent : fileStructTable) {
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string path(namePtr);

        // Default exclusion must still apply
        TEST_ASSERT(path.find("id_rsa")      == std::string::npos);
        // User custom exclusion must also apply
        TEST_ASSERT(path.find("token.txt")   == std::string::npos);

        if (path.find("hello.txt") != std::string::npos)
            foundHello = true;
    }

    TEST_ASSERT(foundHello);

    setBinaryCachePath("");
    fs::remove_all(sandbox, ec);

    std::cout << "[PASS] test_custom_ignore_rules_compose_with_defaults passed.\n";
}

// ---------------------------------------------------------------------------
// Seam 2 & 3: traversal marks files Unknown; lazy resolution via searchPaths
// ---------------------------------------------------------------------------
void test_traversal_no_stat_on_regular_files() {
    std::cout << "[TEST] Running test_traversal_no_stat_on_regular_files...\n";

    fs::path sandbox = fs::temp_directory_path() / "fizz_test_lazy_exec";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);

    fs::path execFile = sandbox / "run.sh";
    { std::ofstream f(execFile); f << "#!/bin/sh\n"; }
    chmod(execFile.c_str(), 0755);

    fs::path textFile = sandbox / "data.txt";
    { std::ofstream f(textFile); f << "plain text\n"; }
    chmod(textFile.c_str(), 0644);

    fs::path cachePath = sandbox / "lazy_cache.bin";
    setBinaryCachePath(cachePath.string());
    setCustomIgnoreRules({});

    systemFileSearch(sandbox.string());

    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded);

    // After traversal, all regular files must have ExecState::Unknown (no stat done)
    for (const auto& ent : fileStructTable) {
        const char* namePtr = &g_InMemoryIndexBuffer[ent.f_offset];
        std::string path(namePtr);
        if (path.find("run.sh") != std::string::npos ||
            path.find("data.txt") != std::string::npos) {
            TEST_ASSERT(ent.f_execState == ExecState::Unknown);
        }
    }

    // After searchPaths(), exec state must be lazily resolved correctly
    std::vector<SearchHit> hits;
    searchPaths("run", hits, 10);
    bool foundExec = false;
    for (const auto& hit : hits) {
        if (hit.path.find("run.sh") != std::string::npos) {
            foundExec = true;
            TEST_ASSERT(hit.execState == ExecState::Executable);
        }
    }
    TEST_ASSERT(foundExec);

    hits.clear();
    searchPaths("data", hits, 10);
    bool foundNonExec = false;
    for (const auto& hit : hits) {
        if (hit.path.find("data.txt") != std::string::npos) {
            foundNonExec = true;
            TEST_ASSERT(hit.execState == ExecState::NotExecutable);
        }
    }
    TEST_ASSERT(foundNonExec);

    setBinaryCachePath("");
    fs::remove_all(sandbox, ec);

    std::cout << "[PASS] test_traversal_no_stat_on_regular_files passed.\n";
}

void run_binary_cache_tests() {
    test_file_classification();
    test_default_search_roots();
    test_dual_search_roots_and_traversal();
    test_binary_cache_path_resolution();
    test_binary_cache_sandboxed_roundtrip();
    test_binary_cache_corruption_detection();
    test_query_matcher_end_to_end();
    test_binary_cache_atomic_publication_and_permissions();
    test_binary_cache_deserialization_validation();
    test_sensitive_directory_exclusion();
    test_custom_ignore_rules_compose_with_defaults();
    test_traversal_no_stat_on_regular_files();
}
