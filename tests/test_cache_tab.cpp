#include <iostream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "services/CacheTab.h"
#include "services/Config.h"
#include "services/FileSearch.h"
#include "imgui.h"

namespace fs = std::filesystem;

#define CACHE_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

static void test_file_size_formatting() {
    std::cout << "[TEST] Running test_file_size_formatting...\n";

    // Sub-megabyte values should be formatted as KB
    CACHE_TEST_ASSERT(fizz::formatFileSize(0) == "0.0 KB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(512) == "0.5 KB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(1024) == "1.0 KB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(10240) == "10.0 KB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(524288) == "512.0 KB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(1048575) == "1024.0 KB");

    // 1 MB and above should be formatted as MB
    CACHE_TEST_ASSERT(fizz::formatFileSize(1048576) == "1.00 MB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(1572864) == "1.50 MB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(2097152) == "2.00 MB");
    CACHE_TEST_ASSERT(fizz::formatFileSize(10485760) == "10.00 MB");

    std::cout << "[PASS] test_file_size_formatting passed.\n";
}

static void test_timestamp_formatting() {
    std::cout << "[TEST] Running test_timestamp_formatting...\n";

    // 0 or negative timestamp indicates never written
    CACHE_TEST_ASSERT(fizz::formatTimestamp(0) == "Never");
    CACHE_TEST_ASSERT(fizz::formatTimestamp(-1) == "Never");

    // Specific known timestamp: verify format is YYYY-MM-DD HH:MM:SS
    time_t now = std::time(nullptr);
    std::string formatted = fizz::formatTimestamp(now);
    CACHE_TEST_ASSERT(formatted.size() == 19);
    CACHE_TEST_ASSERT(formatted[4] == '-' && formatted[7] == '-');
    CACHE_TEST_ASSERT(formatted[10] == ' ');
    CACHE_TEST_ASSERT(formatted[13] == ':' && formatted[16] == ':');

    std::cout << "[PASS] test_timestamp_formatting passed.\n";
}

static void test_cache_diagnostics_calculation() {
    std::cout << "[TEST] Running test_cache_diagnostics_calculation...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_diagnostics";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);

    // 1. Diagnostics for non-existent file
    fs::path nonExistent = sandboxDir / "missing.bin";
    auto diagMissing = fizz::getCacheDiagnostics(nonExistent.string());
    CACHE_TEST_ASSERT(!diagMissing.fileExists);
    CACHE_TEST_ASSERT(diagMissing.fileSizeBytes == 0);
    CACHE_TEST_ASSERT(diagMissing.entryCount == 0);
    CACHE_TEST_ASSERT(diagMissing.lastModified == 0);
    CACHE_TEST_ASSERT(diagMissing.formattedSize == "0.0 KB");
    CACHE_TEST_ASSERT(diagMissing.formattedTimestamp == "Never");

    // 2. Diagnostics for valid binary cache file with binary header
    fs::path cacheFile = sandboxDir / "valid_cache.bin";
    size_t structCount = 1420;
    size_t charCount = 45000;
    {
        std::ofstream out(cacheFile, std::ios::binary);
        out.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
        out.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
        // Write dummy payload matching charCount
        std::string dummy(1024, 'x');
        out.write(dummy.data(), dummy.size());
    }

    auto diagValid = fizz::getCacheDiagnostics(cacheFile.string());
    CACHE_TEST_ASSERT(diagValid.fileExists);
    CACHE_TEST_ASSERT(diagValid.entryCount == 1420);
    size_t expectedSize = sizeof(size_t) * 2 + 1024;
    CACHE_TEST_ASSERT(diagValid.fileSizeBytes == expectedSize);
    CACHE_TEST_ASSERT(diagValid.formattedSize == fizz::formatFileSize(expectedSize));
    CACHE_TEST_ASSERT(diagValid.lastModified > 0);
    CACHE_TEST_ASSERT(diagValid.formattedTimestamp != "Never");

    // Clean up
    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_cache_diagnostics_calculation passed.\n";
}

static void test_path_validation_valid_path() {
    std::cout << "[TEST] Running test_path_validation_valid_path...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_val_valid";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);
    chmod(sandboxDir.c_str(), 0700);

    fs::path candidatePath = sandboxDir / "cache.bin";
    std::string errMsg;
    bool valid = fizz::validateCachePath(candidatePath.string(), &errMsg);
    CACHE_TEST_ASSERT(valid);
    CACHE_TEST_ASSERT(errMsg.empty());

    // Existing regular file owned by current user
    {
        std::ofstream out(candidatePath);
        out << "dummy";
    }
    chmod(candidatePath.c_str(), 0600);
    valid = fizz::validateCachePath(candidatePath.string(), &errMsg);
    CACHE_TEST_ASSERT(valid);

    // Tilde expansion (~/cache.bin) with sandbox HOME
    const char* origHome = std::getenv("HOME");
    std::string origHomeStr = origHome ? origHome : "";
    setenv("HOME", sandboxDir.c_str(), 1);
    valid = fizz::validateCachePath("~/cache.bin", &errMsg);
    if (!origHomeStr.empty()) {
        setenv("HOME", origHomeStr.c_str(), 1);
    } else {
        unsetenv("HOME");
    }
    CACHE_TEST_ASSERT(valid);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_path_validation_valid_path passed.\n";
}

static void test_path_validation_symlink_rejection() {
    std::cout << "[TEST] Running test_path_validation_symlink_rejection...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_val_symlink";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);
    chmod(sandboxDir.c_str(), 0700);

    fs::path targetFile = sandboxDir / "target.bin";
    {
        std::ofstream out(targetFile);
        out << "payload";
    }

    fs::path symlinkFile = sandboxDir / "link_to_target.bin";
    int symlinkRes = ::symlink(targetFile.c_str(), symlinkFile.c_str());
    CACHE_TEST_ASSERT(symlinkRes == 0);

    std::string errMsg;
    bool valid = fizz::validateCachePath(symlinkFile.string(), &errMsg);
    CACHE_TEST_ASSERT(!valid);
    CACHE_TEST_ASSERT(errMsg.find("symbolic link") != std::string::npos ||
                      errMsg.find("symlink") != std::string::npos);

    // Symlink parent directory rejection
    fs::path realDir = sandboxDir / "real_dir";
    fs::create_directories(realDir, ec);
    fs::path symlinkDir = sandboxDir / "link_dir";
    ::symlink(realDir.c_str(), symlinkDir.c_str());

    fs::path pathInSymlinkDir = symlinkDir / "cache.bin";
    valid = fizz::validateCachePath(pathInSymlinkDir.string(), &errMsg);
    CACHE_TEST_ASSERT(!valid);
    CACHE_TEST_ASSERT(errMsg.find("symbolic link") != std::string::npos ||
                      errMsg.find("symlink") != std::string::npos);

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_path_validation_symlink_rejection passed.\n";
}

static void test_path_validation_world_writable_rejection() {
    std::cout << "[TEST] Running test_path_validation_world_writable_rejection...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_val_ww";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);
    chmod(sandboxDir.c_str(), 0777); // Set directory to world-writable

    struct stat st;
    CACHE_TEST_ASSERT(stat(sandboxDir.c_str(), &st) == 0);
    CACHE_TEST_ASSERT((st.st_mode & S_IWOTH) != 0);

    fs::path candidatePath = sandboxDir / "cache.bin";
    std::string errMsg;

    // When the directory owner does not match currentUid (simulating e.g. root or other user),
    // it must be rejected!
    uid_t differentUid = st.st_uid + 9999;
    bool valid = fizz::validateCachePath(candidatePath.string(), &errMsg, differentUid);
    CACHE_TEST_ASSERT(!valid);
    CACHE_TEST_ASSERT(errMsg.find("world-writable") != std::string::npos);

    // When the directory owner matches currentUid, world-writable directory owned by user is permitted
    valid = fizz::validateCachePath(candidatePath.string(), &errMsg, st.st_uid);
    CACHE_TEST_ASSERT(valid);

    // Common system case: path directly in /tmp when geteuid() != 0
    if (geteuid() != 0) {
        struct stat tmpSt;
        if (stat("/tmp", &tmpSt) == 0 && (tmpSt.st_mode & S_IWOTH) != 0 && tmpSt.st_uid != geteuid()) {
            valid = fizz::validateCachePath("/tmp/fizz_cache_test.bin", &errMsg, geteuid());
            CACHE_TEST_ASSERT(!valid);
            CACHE_TEST_ASSERT(errMsg.find("world-writable") != std::string::npos);
        }
    }

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_path_validation_world_writable_rejection passed.\n";
}

static void test_path_validation_parent_checks() {
    std::cout << "[TEST] Running test_path_validation_parent_checks...\n";

    // 1. Empty path rejection
    std::string errMsg;
    CACHE_TEST_ASSERT(!fizz::validateCachePath("", &errMsg));
    CACHE_TEST_ASSERT(!errMsg.empty());

    // 2. Non-existent parent directory
    CACHE_TEST_ASSERT(!fizz::validateCachePath("/non_existent_folder_abc123/cache.bin", &errMsg));
    CACHE_TEST_ASSERT(errMsg.find("exist") != std::string::npos);

    // 3. Parent is not writable
    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_val_nowrite";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);
    chmod(sandboxDir.c_str(), 0500); // Read/execute only, not writable

    fs::path testPath = sandboxDir / "cache.bin";
    CACHE_TEST_ASSERT(!fizz::validateCachePath(testPath.string(), &errMsg));
    CACHE_TEST_ASSERT(errMsg.find("writable") != std::string::npos);

    // Restore permissions so remove_all succeeds
    chmod(sandboxDir.c_str(), 0700);
    fs::remove_all(sandboxDir, ec);

    // 4. Path itself is an existing directory
    fs::path existingDir = fs::temp_directory_path() / "fizz_test_val_dir";
    fs::create_directories(existingDir, ec);
    CACHE_TEST_ASSERT(!fizz::validateCachePath(existingDir.string(), &errMsg));
    CACHE_TEST_ASSERT(errMsg.find("directory") != std::string::npos);
    fs::remove_all(existingDir, ec);

    std::cout << "[PASS] test_path_validation_parent_checks passed.\n";
}

static void test_configuration_synchronization() {
    std::cout << "[TEST] Running test_configuration_synchronization...\n";

    fs::path sandboxDir = fs::temp_directory_path() / "fizz_test_config_sync";
    std::error_code ec;
    fs::remove_all(sandboxDir, ec);
    fs::create_directories(sandboxDir, ec);
    chmod(sandboxDir.c_str(), 0700);

    fs::path confFile = sandboxDir / "fizz.conf";
    fs::path customCache = sandboxDir / "custom_local_cache.bin";

    FizzConfig config = FizzConfig::defaults();
    CACHE_TEST_ASSERT(saveConfig(config, confFile.string()));

    // 1. Applying valid custom cache path
    std::string errMsg;
    bool applied = fizz::applyCustomCachePath(customCache.string(), config, &errMsg, confFile.string());
    CACHE_TEST_ASSERT(applied);
    CACHE_TEST_ASSERT(config.binary_cache_path == customCache.string());
    CACHE_TEST_ASSERT(getBinaryCachePath() == customCache.string());

    // Verify persisted on disk in fizz.conf
    FizzConfig reloaded = loadConfig(confFile.string());
    CACHE_TEST_ASSERT(reloaded.binary_cache_path == customCache.string());

    // 2. Applying invalid path (e.g. symlink) should fail without touching config or file
    fs::path dummyTarget = sandboxDir / "target.txt";
    {
        std::ofstream out(dummyTarget);
        out << "hello";
    }
    fs::path symlinkPath = sandboxDir / "symlink.bin";
    ::symlink(dummyTarget.c_str(), symlinkPath.c_str());

    applied = fizz::applyCustomCachePath(symlinkPath.string(), config, &errMsg, confFile.string());
    CACHE_TEST_ASSERT(!applied);
    CACHE_TEST_ASSERT(!errMsg.empty());
    // Config binary_cache_path remains unchanged
    CACHE_TEST_ASSERT(config.binary_cache_path == customCache.string());
    reloaded = loadConfig(confFile.string());
    CACHE_TEST_ASSERT(reloaded.binary_cache_path == customCache.string());

    // 3. Resetting to default
    bool reset = fizz::resetToDefaultCachePath(config, &errMsg, confFile.string());
    CACHE_TEST_ASSERT(reset);
    CACHE_TEST_ASSERT(config.binary_cache_path.empty());
    reloaded = loadConfig(confFile.string());
    CACHE_TEST_ASSERT(reloaded.binary_cache_path.empty());

    // Reset runtime binary cache path to empty for clean state
    setBinaryCachePath("");

    fs::remove_all(sandboxDir, ec);
    std::cout << "[PASS] test_configuration_synchronization passed.\n";
}

static void test_cache_tab_ui_state() {
    std::cout << "[TEST] Running test_cache_tab_ui_state...\n";

    FizzConfig config = FizzConfig::defaults();
    config.binary_cache_path = "/tmp/fizz_custom_test.bin";

    fizz::CacheTab tab(config);
    CACHE_TEST_ASSERT(tab.getInputPath() == "/tmp/fizz_custom_test.bin");

    // Setting input path programmatically
    tab.setInputPath("/another/path.bin");
    CACHE_TEST_ASSERT(tab.getInputPath() == "/another/path.bin");

    // Headless ImGui context test
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(800, 600);

    ImGui::NewFrame();
    ImGui::Begin("TestWindow");
    tab.render(config);
    ImGui::End();
    ImGui::Render();

    ImGui::DestroyContext(ctx);

    std::cout << "[PASS] test_cache_tab_ui_state passed.\n";
}

void run_cache_tab_tests() {
    std::cout << "--- Running CacheTab Unit Tests ---\n";
    test_file_size_formatting();
    test_timestamp_formatting();
    test_cache_diagnostics_calculation();
    test_path_validation_valid_path();
    test_path_validation_symlink_rejection();
    test_path_validation_world_writable_rejection();
    test_path_validation_parent_checks();
    test_configuration_synchronization();
    test_cache_tab_ui_state();
    std::cout << "--- CacheTab Unit Tests Passed ---\n";
}
