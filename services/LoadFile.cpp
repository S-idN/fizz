#include "LoadFile.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>

// extern std::ifstream binaryFile;

static std::string cachePath() {
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.fizzLocalCache.bin";
}

std::vector<char> g_InMemoryIndexBuffer;

void loadIndexToMemory() {
        std::ifstream localBinaryFile(cachePath(), std::ios::binary);
    if (localBinaryFile.is_open()) {
        localBinaryFile.close();
    }
    localBinaryFile.clear();

    localBinaryFile.open(cachePath(), std::ios::binary);
    if (!localBinaryFile.is_open()) {
        std::cout << "[ERROR] Could not open .fizzLocalCache.bin to populate RAM cache!\n";
        return;
    }

    size_t charCount = 0;
    localBinaryFile.seekg(8, std::ios::beg);
    localBinaryFile.read(reinterpret_cast<char*>(&charCount), sizeof(charCount));
    
    if (charCount > 0) {
        g_InMemoryIndexBuffer.resize(charCount);
        localBinaryFile.seekg(-static_cast<std::streamoff>(charCount), std::ios::end);
        localBinaryFile.read(g_InMemoryIndexBuffer.data(), charCount);
    }
    localBinaryFile.close();
    std::cout << "[SYSTEM] Cached " << g_InMemoryIndexBuffer.size() << " bytes into RAM.\n";
}

