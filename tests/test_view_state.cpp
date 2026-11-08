#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "services/ViewState.h"
#include "services/Config.h"
#include "services/ViewMode.h"

namespace fs = std::filesystem;

#define VIEW_STATE_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_view_state_runtime_dir_and_path_resolution() {
    std::cout << "[TEST] Running test_view_state_runtime_dir_and_path_resolution...\n";

    std::string rtDir = getUserRuntimeDir();
    VIEW_STATE_TEST_ASSERT(!rtDir.empty());

    // Verify runtime directory has 0700 permissions, is a directory, not a symlink, owned by getuid()
    struct stat st;
    VIEW_STATE_TEST_ASSERT(lstat(rtDir.c_str(), &st) == 0);
    VIEW_STATE_TEST_ASSERT(S_ISDIR(st.st_mode));
    VIEW_STATE_TEST_ASSERT(!S_ISLNK(st.st_mode));
    VIEW_STATE_TEST_ASSERT(st.st_uid == getuid());
    VIEW_STATE_TEST_ASSERT((st.st_mode & 0777) == 0700);

    // Verify getViewStateFilePath matches rtDir + "/view_state"
    std::string statePath = getViewStateFilePath();
    VIEW_STATE_TEST_ASSERT(statePath == rtDir + "/view_state");

    std::cout << "[PASS] test_view_state_runtime_dir_and_path_resolution passed.\n";
}

void test_view_state_save_and_mode_permissions() {
    std::cout << "[TEST] Running test_view_state_save_and_mode_permissions...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_perms";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    fs::path stateFile = tempDir / "view_state";
    fs::remove(stateFile, ec);

    // Save Settings mode
    bool saved = saveViewState(ViewMode::Settings, stateFile.string());
    VIEW_STATE_TEST_ASSERT(saved);
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // Verify file permissions 0600
    struct stat st;
    VIEW_STATE_TEST_ASSERT(lstat(stateFile.c_str(), &st) == 0);
    VIEW_STATE_TEST_ASSERT(S_ISREG(st.st_mode));
    VIEW_STATE_TEST_ASSERT(!S_ISLNK(st.st_mode));
    VIEW_STATE_TEST_ASSERT(st.st_uid == geteuid());
    VIEW_STATE_TEST_ASSERT((st.st_mode & 0777) == 0600);

    // Verify serialized contents contain mode and timestamp
    std::ifstream in(stateFile);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    VIEW_STATE_TEST_ASSERT(content.find("mode=Settings") != std::string::npos);
    VIEW_STATE_TEST_ASSERT(content.find("timestamp=") != std::string::npos);

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_save_and_mode_permissions passed.\n";
}

void test_view_state_within_grace_period_resumption() {
    std::cout << "[TEST] Running test_view_state_within_grace_period_resumption...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_resume";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    fs::path stateFile = tempDir / "view_state";

    int64_t t0 = 1700000000;

    // 1. Settings view resumed within grace period
    fs::remove(stateFile, ec);
    VIEW_STATE_TEST_ASSERT(saveViewState(ViewMode::Settings, stateFile.string(), t0));
    // 2 seconds later, grace period is 5 seconds -> should resume Settings
    ViewMode resumed = loadViewState(5, stateFile.string(), t0 + 2);
    VIEW_STATE_TEST_ASSERT(resumed == ViewMode::Settings);
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // 2. About view resumed within grace period (at boundary: 5 seconds elapsed)
    fs::remove(stateFile, ec);
    VIEW_STATE_TEST_ASSERT(saveViewState(ViewMode::About, stateFile.string(), t0));
    resumed = loadViewState(5, stateFile.string(), t0 + 5);
    VIEW_STATE_TEST_ASSERT(resumed == ViewMode::About);
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_within_grace_period_resumption passed.\n";
}

void test_view_state_post_grace_period_expiration() {
    std::cout << "[TEST] Running test_view_state_post_grace_period_expiration...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_expire";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    fs::path stateFile = tempDir / "view_state";

    int64_t t0 = 1700000000;

    // Save Settings at t0
    fs::remove(stateFile, ec);
    VIEW_STATE_TEST_ASSERT(saveViewState(ViewMode::Settings, stateFile.string(), t0));
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // Reopening 6 seconds later with 5-second grace period -> expired!
    ViewMode mode = loadViewState(5, stateFile.string(), t0 + 6);
    VIEW_STATE_TEST_ASSERT(mode == ViewMode::Search);

    // Verify state file was deleted upon expiration
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_post_grace_period_expiration passed.\n";
}

void test_view_state_search_dismissal_clears_file() {
    std::cout << "[TEST] Running test_view_state_search_dismissal_clears_file...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_search_clear";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    fs::path stateFile = tempDir / "view_state";

    // 1. Save Settings mode -> state file exists
    fs::remove(stateFile, ec);
    VIEW_STATE_TEST_ASSERT(saveViewState(ViewMode::Settings, stateFile.string()));
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // 2. Dismissing while in Search view clears state file
    VIEW_STATE_TEST_ASSERT(saveViewState(ViewMode::Search, stateFile.string()));
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 3. Loading after clear returns Search view
    ViewMode mode = loadViewState(5, stateFile.string());
    VIEW_STATE_TEST_ASSERT(mode == ViewMode::Search);

    // 4. Calling clearViewState on already removed file is safe and returns true
    VIEW_STATE_TEST_ASSERT(clearViewState(stateFile.string()));

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_search_dismissal_clears_file passed.\n";
}

void test_view_state_corrupt_and_invalid_state() {
    std::cout << "[TEST] Running test_view_state_corrupt_and_invalid_state...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_corrupt";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    fs::path stateFile = tempDir / "view_state";

    int64_t t0 = 1700000000;

    // 1. Non-existent file
    fs::remove(stateFile, ec);
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);

    // 2. Empty file
    {
        std::ofstream out(stateFile);
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 3. Random binary / malformed junk
    {
        std::ofstream out(stateFile);
        out << "corrupt gibberish without keys\nsecond line\n";
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 4. Missing timestamp
    {
        std::ofstream out(stateFile);
        out << "mode=Settings\n";
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 5. Invalid mode string
    {
        std::ofstream out(stateFile);
        out << "mode=UnknownMode\ntimestamp=1700000000\n";
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 6. Non-numeric timestamp
    {
        std::ofstream out(stateFile);
        out << "mode=Settings\ntimestamp=not_a_number\n";
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 7. Negative timestamp
    {
        std::ofstream out(stateFile);
        out << "mode=Settings\ntimestamp=-500\n";
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // 8. Clock skew (timestamp far in future relative to load time)
    {
        std::ofstream out(stateFile);
        out << "mode=Settings\ntimestamp=" << (t0 + 1000) << "\n";
    }
    VIEW_STATE_TEST_ASSERT(loadViewState(5, stateFile.string(), t0) == ViewMode::Search);
    VIEW_STATE_TEST_ASSERT(!fs::exists(stateFile));

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_corrupt_and_invalid_state passed.\n";
}

void test_view_state_symlink_rejection() {
    std::cout << "[TEST] Running test_view_state_symlink_rejection...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_symlink";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    fs::path decoyTarget = tempDir / "sensitive_decoy.txt";
    {
        std::ofstream out(decoyTarget);
        out << "TOP_SECRET_DO_NOT_OVERWRITE";
    }

    fs::path symlinkPath = tempDir / "view_state";
    fs::remove(symlinkPath, ec);
    fs::create_symlink(decoyTarget, symlinkPath, ec);

    // Verify symlink was created
    struct stat symSt;
    VIEW_STATE_TEST_ASSERT(lstat(symlinkPath.c_str(), &symSt) == 0 && S_ISLNK(symSt.st_mode));

    // 1. loadViewState must reject symlink, return Search, and NOT follow or alter decoy target
    ViewMode loaded = loadViewState(5, symlinkPath.string());
    VIEW_STATE_TEST_ASSERT(loaded == ViewMode::Search);

    // Verify decoy target was untouched
    {
        std::ifstream in(decoyTarget);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        VIEW_STATE_TEST_ASSERT(content == "TOP_SECRET_DO_NOT_OVERWRITE");
    }

    // 2. Re-create symlink to test saveViewState O_NOFOLLOW rejection
    fs::remove(symlinkPath, ec);
    fs::create_symlink(decoyTarget, symlinkPath, ec);

    bool saveResult = saveViewState(ViewMode::Settings, symlinkPath.string());
    // saveViewState must fail because O_NOFOLLOW prevents opening symlinks
    VIEW_STATE_TEST_ASSERT(!saveResult);

    // Verify decoy target was NOT overwritten
    {
        std::ifstream in(decoyTarget);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        VIEW_STATE_TEST_ASSERT(content == "TOP_SECRET_DO_NOT_OVERWRITE");
    }

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_symlink_rejection passed.\n";
}

void test_view_state_non_regular_file_rejection() {
    std::cout << "[TEST] Running test_view_state_non_regular_file_rejection...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_dir_rejection";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    fs::path subDir = tempDir / "sub_directory";
    fs::create_directories(subDir, ec);

    // Passing a directory path as the state file path must be rejected by !S_ISREG
    ViewMode loaded = loadViewState(5, subDir.string());
    VIEW_STATE_TEST_ASSERT(loaded == ViewMode::Search);
    // Subdirectory must still exist
    VIEW_STATE_TEST_ASSERT(fs::is_directory(subDir));

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_non_regular_file_rejection passed.\n";
}

void test_view_state_unowned_file_rejection() {
    std::cout << "[TEST] Running test_view_state_unowned_file_rejection...\n";

    fs::path tempDir = fs::temp_directory_path() / "fizz_test_view_state_unowned";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    fs::path stateFile = tempDir / "view_state";
    fs::remove(stateFile, ec);

    // Save a valid Settings state
    VIEW_STATE_TEST_ASSERT(saveViewState(ViewMode::Settings, stateFile.string()));
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // When loading expecting a different UID (simulating unowned file)
    uid_t fakeForeignUid = geteuid() + 9999;
    ViewMode loaded = loadViewState(5, stateFile.string(), 0, fakeForeignUid);

    // Must reject unowned file and return ViewMode::Search
    VIEW_STATE_TEST_ASSERT(loaded == ViewMode::Search);
    // Unowned file must NOT be deleted
    VIEW_STATE_TEST_ASSERT(fs::exists(stateFile));

    // Cleanup
    fs::remove_all(tempDir, ec);
    std::cout << "[PASS] test_view_state_unowned_file_rejection passed.\n";
}

void run_view_state_tests() {
    test_view_state_runtime_dir_and_path_resolution();
    test_view_state_save_and_mode_permissions();
    test_view_state_within_grace_period_resumption();
    test_view_state_post_grace_period_expiration();
    test_view_state_search_dismissal_clears_file();
    test_view_state_corrupt_and_invalid_state();
    test_view_state_symlink_rejection();
    test_view_state_non_regular_file_rejection();
    test_view_state_unowned_file_rejection();
}
