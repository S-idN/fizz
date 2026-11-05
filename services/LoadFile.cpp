#include "FileSearch.h"
#include "LoadFile.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>

std::vector<char> g_InMemoryIndexBuffer;

bool loadIndexToMemory() {
    const std::string path = getBinaryCachePath();
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "[ERROR] Could not open Binary Cache: " << path << "\n";
        fileStructTable.clear();
        g_InMemoryIndexBuffer.clear();
        return false;
    }

    auto cleanupFailure = [&]() -> bool {
        fileStructTable.clear();
        g_InMemoryIndexBuffer.clear();
        std::cout << "[ERROR] Binary Cache is truncated or corrupted; rebuild it.\n";
        return false;
    };

    f.seekg(0, std::ios::end);
    std::streampos fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    const size_t headerSize = sizeof(size_t) * 2;
    if (fileSize < static_cast<std::streampos>(headerSize)) {
        return cleanupFailure();
    }

    size_t structCount = 0, charCount = 0;
    f.read(reinterpret_cast<char*>(&structCount), sizeof(structCount));
    f.read(reinterpret_cast<char*>(&charCount),   sizeof(charCount));
    if (!f) {
        return cleanupFailure();
    }

    // Guard against arithmetic overflow and unreasonable allocations
    if (structCount > (SIZE_MAX / sizeof(fileEntity))) {
        return cleanupFailure();
    }

    const size_t expectedDataBytes = structCount * sizeof(fileEntity) + charCount;
    const size_t actualDataBytes = static_cast<size_t>(fileSize) - headerSize;
    if (expectedDataBytes != actualDataBytes) {
        return cleanupFailure();
    }

    fileStructTable.resize(structCount);
    if (structCount > 0) {
        f.read(reinterpret_cast<char*>(fileStructTable.data()),
               structCount * sizeof(fileEntity));
    }

    g_InMemoryIndexBuffer.resize(charCount);
    if (charCount > 0) {
        f.read(g_InMemoryIndexBuffer.data(), charCount);
    }

    if (!f) {
        return cleanupFailure();
    }

    // Strictly validate entry string offsets, length bounds, and null-termination
    for (const auto& ent : fileStructTable) {
        if (ent.f_length == 0) {
            return cleanupFailure();
        }
        // Offset and length must lie strictly within the in-memory string pool buffer without overflow
        if (ent.f_offset >= charCount || ent.f_length > charCount || ent.f_offset > charCount - ent.f_length) {
            return cleanupFailure();
        }
        // Path must be null-terminated at the designated end of the slice
        if (g_InMemoryIndexBuffer[ent.f_offset + ent.f_length - 1] != '\0') {
            return cleanupFailure();
        }
        // Path must not contain premature null terminators
        if (std::strlen(&g_InMemoryIndexBuffer[ent.f_offset]) != ent.f_length - 1) {
            return cleanupFailure();
        }
    }

    std::cout << "[SYSTEM] Loaded " << structCount << " entries, "
              << charCount << " bytes.\n";
    return true;
}
