#include "FileSearch.h"
#include "LoadFile.h"
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

std::vector<char> g_InMemoryIndexBuffer;

static struct timespec g_LoadedIndexMtime = {0, 0};
static bool g_IndexLoaded = false;

struct timespec getLoadedIndexTimestamp() {
    return g_LoadedIndexMtime;
}

void resetLoadedIndex() {
    fileStructTable.clear();
    g_InMemoryIndexBuffer.clear();
    g_LoadedIndexMtime = {0, 0};
    g_IndexLoaded = false;
}

static bool readExact(int fd, void* buf, size_t count) {
    char* p = static_cast<char*>(buf);
    size_t remaining = count;
    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        p += n;
        remaining -= n;
    }
    return true;
}

static bool loadIndexFromFd(int fd, const struct stat& st, bool isInitialLoad) {
    auto cleanupFailure = [&]() -> bool {
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
        if (isInitialLoad || !g_IndexLoaded) {
            fileStructTable.clear();
            g_InMemoryIndexBuffer.clear();
            g_LoadedIndexMtime = {0, 0};
            g_IndexLoaded = false;
        }
        std::cout << "[ERROR] Binary Cache is truncated or corrupted; rebuild it.\n";
        return false;
    };

    const size_t headerSize = sizeof(size_t) * 2;
    size_t fileSize = static_cast<size_t>(st.st_size);
    if (fileSize < headerSize) {
        return cleanupFailure();
    }

    size_t structCount = 0, charCount = 0;
    if (!readExact(fd, &structCount, sizeof(structCount)) ||
        !readExact(fd, &charCount, sizeof(charCount))) {
        return cleanupFailure();
    }

    // Guard against arithmetic overflow and unreasonable allocations
    if (structCount > (SIZE_MAX / sizeof(fileEntity))) {
        return cleanupFailure();
    }
    const size_t structBytes = structCount * sizeof(fileEntity);
    if (charCount > (SIZE_MAX - structBytes)) {
        return cleanupFailure();
    }

    const size_t expectedDataBytes = structBytes + charCount;
    const size_t actualDataBytes = fileSize - headerSize;
    if (expectedDataBytes != actualDataBytes) {
        return cleanupFailure();
    }

    std::vector<fileEntity> newTable(structCount);
    if (structCount > 0) {
        if (!readExact(fd, newTable.data(), structBytes)) {
            return cleanupFailure();
        }
    }

    std::vector<char> newBuffer(charCount);
    if (charCount > 0) {
        if (!readExact(fd, newBuffer.data(), charCount)) {
            return cleanupFailure();
        }
    }

    close(fd);

    // Strictly validate entry string offsets, length bounds, and null-termination
    for (const auto& ent : newTable) {
        if (ent.f_length == 0) {
            return cleanupFailure();
        }
        // Offset and length must lie strictly within the in-memory string pool buffer without overflow
        if (ent.f_offset >= charCount || ent.f_length > charCount || ent.f_offset > charCount - ent.f_length) {
            return cleanupFailure();
        }
        // Path must be null-terminated at the designated end of the slice
        if (newBuffer[ent.f_offset + ent.f_length - 1] != '\0') {
            return cleanupFailure();
        }
        // Path must not contain premature null terminators
        if (std::strlen(&newBuffer[ent.f_offset]) != ent.f_length - 1) {
            return cleanupFailure();
        }
    }

    fileStructTable = std::move(newTable);
    g_InMemoryIndexBuffer = std::move(newBuffer);
    g_LoadedIndexMtime = st.st_mtim;
    g_IndexLoaded = true;

    std::cout << "[SYSTEM] Loaded " << structCount << " entries, "
              << charCount << " bytes.\n";
    return true;
}

bool loadIndexToMemory() {
    const std::string path = getBinaryCachePath();
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        std::cout << "[ERROR] Could not open Binary Cache: " << path << "\n";
        fileStructTable.clear();
        g_InMemoryIndexBuffer.clear();
        g_LoadedIndexMtime = {0, 0};
        g_IndexLoaded = false;
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        std::cout << "[ERROR] Could not stat Binary Cache: " << path << "\n";
        close(fd);
        fileStructTable.clear();
        g_InMemoryIndexBuffer.clear();
        g_LoadedIndexMtime = {0, 0};
        g_IndexLoaded = false;
        return false;
    }

    return loadIndexFromFd(fd, st, true);
}

bool reloadIndexIfModified() {
    const std::string path = getBinaryCachePath();
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return false;
    }

    if (g_IndexLoaded) {
        bool isNewer = (st.st_mtim.tv_sec > g_LoadedIndexMtime.tv_sec) ||
                       (st.st_mtim.tv_sec == g_LoadedIndexMtime.tv_sec && st.st_mtim.tv_nsec > g_LoadedIndexMtime.tv_nsec);
        if (!isNewer) {
            close(fd);
            return false;
        }
    }

    return loadIndexFromFd(fd, st, false);
}
