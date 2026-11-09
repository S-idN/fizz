#include <iostream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <sys/inotify.h>
#include "services/FileCacheBuilder.h"
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

void test_watch_dir_enospc_diagnostic() {
    std::cout << "[TEST] Running test_watch_dir_enospc_diagnostic...\n";

    std::unordered_map<int, std::string> dirMap;

    // 1. Mock inotify_add_watch that fails with ENOSPC
    auto mockEnospc = [](int, const char*, uint32_t) -> int {
        errno = ENOSPC;
        return -1;
    };

    // Redirect stderr to capture the warning diagnostic
    std::stringstream capturedStderr;
    std::streambuf* oldCerr = std::cerr.rdbuf(capturedStderr.rdbuf());

    int wd = watchDir(100, "/home/user/large_project", dirMap, mockEnospc);

    // Restore stderr
    std::cerr.rdbuf(oldCerr);

    TEST_ASSERT(wd < 0);
    TEST_ASSERT(dirMap.empty());

    std::string errStr = capturedStderr.str();
    TEST_ASSERT(errStr.find("ENOSPC") != std::string::npos);
    TEST_ASSERT(errStr.find("sysctl fs.inotify.max_user_watches") != std::string::npos);
    TEST_ASSERT(errStr.find("/home/user/large_project") != std::string::npos);

    // 2. Mock inotify_add_watch that succeeds
    auto mockSuccess = [](int, const char*, uint32_t) -> int {
        return 42;
    };

    capturedStderr.str("");
    oldCerr = std::cerr.rdbuf(capturedStderr.rdbuf());
    wd = watchDir(100, "/home/user/valid_dir", dirMap, mockSuccess);
    std::cerr.rdbuf(oldCerr);

    TEST_ASSERT(wd == 42);
    TEST_ASSERT(dirMap.count(42) == 1);
    TEST_ASSERT(dirMap[42] == "/home/user/valid_dir");
    TEST_ASSERT(capturedStderr.str().empty());

    std::cout << "[PASS] test_watch_dir_enospc_diagnostic passed.\n";
}

void test_read_events_in_q_overflow_handling() {
    std::cout << "[TEST] Running test_read_events_in_q_overflow_handling...\n";

    int pfd[2];
    TEST_ASSERT(pipe2(pfd, O_NONBLOCK) == 0);

    std::unordered_map<int, std::string> dirMap;
    dirMap[1] = "/watched/dir1";
    dirMap[2] = "/watched/dir2";

    // Simulate an IN_Q_OVERFLOW event from kernel.
    // Inotify kernel documentation guarantees wd == -1 and len == 0 on overflow.
    inotify_event overflowEv{};
    overflowEv.wd = -1;
    overflowEv.mask = IN_Q_OVERFLOW;
    overflowEv.cookie = 0;
    overflowEv.len = 0;

    ssize_t written = write(pfd[1], &overflowEv, sizeof(overflowEv));
    TEST_ASSERT(written == sizeof(overflowEv));

    // Redirect stderr to capture warning log
    std::stringstream capturedStderr;
    std::streambuf* oldCerr = std::cerr.rdbuf(capturedStderr.rdbuf());

    bool overflowDetected = false;
    bool changed = readEvents(pfd[0], dirMap, &overflowDetected);

    std::cerr.rdbuf(oldCerr);

    // Verify recovery behavior:
    // 1. readEvents must report changed = true to mark daemon dirty
    TEST_ASSERT(changed == true);
    TEST_ASSERT(overflowDetected == true);

    // 2. dirMap[-1] must NOT be accessed or created
    TEST_ASSERT(dirMap.count(-1) == 0);
    TEST_ASSERT(dirMap.size() == 2);
    TEST_ASSERT(dirMap[1] == "/watched/dir1");
    TEST_ASSERT(dirMap[2] == "/watched/dir2");

    // 3. Stderr should report IN_Q_OVERFLOW warning
    std::string errStr = capturedStderr.str();
    TEST_ASSERT(errStr.find("IN_Q_OVERFLOW") != std::string::npos);

    close(pfd[0]);
    close(pfd[1]);

    std::cout << "[PASS] test_read_events_in_q_overflow_handling passed.\n";
}

void test_deferred_rebuild_scheduling_on_overflow() {
    std::cout << "[TEST] Running test_deferred_rebuild_scheduling_on_overflow...\n";

    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    RebuildScheduler scheduler(2s, 10s, 1h);
    auto t0 = Clock::now();
    scheduler.onRebuildExecuted(t0);

    TEST_ASSERT(!scheduler.isDirty());
    TEST_ASSERT(!scheduler.shouldRebuild(t0));

    // Event overflow triggers dirty flag
    scheduler.onEvent(t0);
    TEST_ASSERT(scheduler.isDirty());

    // Within quiet window (2s), rebuild is deferred
    TEST_ASSERT(!scheduler.shouldRebuild(t0 + 500ms));
    TEST_ASSERT(!scheduler.shouldRebuild(t0 + 1900ms));

    // After quiet window expires without new events, rebuild triggers
    TEST_ASSERT(scheduler.shouldRebuild(t0 + 2000ms));
    TEST_ASSERT(scheduler.shouldRebuild(t0 + 2100ms));

    // Execute rebuild
    scheduler.onRebuildExecuted(t0 + 2100ms);
    TEST_ASSERT(!scheduler.isDirty());

    // Test max defer window under sustained event storms
    auto stormStart = t0 + 5000ms;
    scheduler.onEvent(stormStart);
    TEST_ASSERT(scheduler.isDirty());

    // Events continue arriving every 1s (quiet window never reaches 2s)
    for (int i = 1; i <= 9; ++i) {
        scheduler.onEvent(stormStart + std::chrono::seconds(i));
        // Still within 10s max deferral, quiet not reached -> should not rebuild yet
        TEST_ASSERT(!scheduler.shouldRebuild(stormStart + std::chrono::seconds(i)));
    }

    // At 10s from firstDirty, max deferral triggers rebuild despite ongoing events
    TEST_ASSERT(scheduler.shouldRebuild(stormStart + 10s));

    scheduler.onRebuildExecuted(stormStart + 10s);
    TEST_ASSERT(!scheduler.isDirty());

    std::cout << "[PASS] test_deferred_rebuild_scheduling_on_overflow passed.\n";
}

void test_overflow_recovery_end_to_end() {
    std::cout << "[TEST] Running test_overflow_recovery_end_to_end...\n";

    fs::path sandbox = fs::temp_directory_path() / "fizz_test_sandbox_overflow_recovery";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox / "docs", ec);

    fs::path initialFile = sandbox / "docs" / "initial.txt";
    {
        std::ofstream out(initialFile);
        out << "initial content\n";
    }

    fs::path cachePath = sandbox / "cache.bin";
    setBinaryCachePath(cachePath.string());

    // 1. Initial build: only initial.txt is indexed
    systemFileSearch(sandbox.string());
    TEST_ASSERT(fs::exists(cachePath));

    bool loaded = loadIndexToMemory();
    TEST_ASSERT(loaded == true);
    bool foundInitial = false;
    for (const auto& ent : fileStructTable) {
        std::string name(&g_InMemoryIndexBuffer[ent.f_offset]);
        if (name.find("initial.txt") != std::string::npos) foundInitial = true;
    }
    TEST_ASSERT(foundInitial);

    // 2. Simulate inotify queue overflow while a new file is created on filesystem
    fs::path afterOverflowFile = sandbox / "docs" / "after_overflow.txt";
    {
        std::ofstream out(afterOverflowFile);
        out << "new content created during overflow\n";
    }

    int pfd[2];
    TEST_ASSERT(pipe2(pfd, O_NONBLOCK) == 0);

    inotify_event overflowEv{};
    overflowEv.wd = -1;
    overflowEv.mask = IN_Q_OVERFLOW;
    write(pfd[1], &overflowEv, sizeof(overflowEv));

    std::unordered_map<int, std::string> dirMap;
    bool overflowDetected = false;

    // Suppress expected warning output
    std::stringstream suppressedCerr;
    std::streambuf* oldCerr = std::cerr.rdbuf(suppressedCerr.rdbuf());
    bool changed = readEvents(pfd[0], dirMap, &overflowDetected);
    std::cerr.rdbuf(oldCerr);

    close(pfd[0]);
    close(pfd[1]);

    TEST_ASSERT(changed == true);
    TEST_ASSERT(overflowDetected == true);

    // 3. Deferred rebuild scheduling
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    RebuildScheduler scheduler(2s, 10s, 1h);
    auto now = Clock::now();

    scheduler.onEvent(now);
    TEST_ASSERT(scheduler.isDirty());
    TEST_ASSERT(!scheduler.shouldRebuild(now));

    // Quiet interval passes -> rebuild is scheduled
    TEST_ASSERT(scheduler.shouldRebuild(now + 2s));

    // Execute the deferred rebuild
    systemFileSearch(sandbox.string());
    scheduler.onRebuildExecuted(now + 2s);
    TEST_ASSERT(!scheduler.isDirty());

    // 4. Verify recovery: both files are now indexed in the Binary Cache
    loaded = loadIndexToMemory();
    TEST_ASSERT(loaded == true);

    foundInitial = false;
    bool foundAfterOverflow = false;
    for (const auto& ent : fileStructTable) {
        std::string name(&g_InMemoryIndexBuffer[ent.f_offset]);
        if (name.find("initial.txt") != std::string::npos) foundInitial = true;
        if (name.find("after_overflow.txt") != std::string::npos) foundAfterOverflow = true;
    }

    TEST_ASSERT(foundInitial);
    TEST_ASSERT(foundAfterOverflow);

    setBinaryCachePath("");
    fs::remove_all(sandbox, ec);

    std::cout << "[PASS] test_overflow_recovery_end_to_end passed.\n";
}

void run_file_cache_builder_tests() {
    test_watch_dir_enospc_diagnostic();
    test_read_events_in_q_overflow_handling();
    test_deferred_rebuild_scheduling_on_overflow();
    test_overflow_recovery_end_to_end();
}
