#include "LoadFile.h"
#include <iostream>
#include <fstream>

extern std::ifstream binaryFile;

std::vector<char> g_InMemoryIndexBuffer;

void loadIndexToMemory() {
    if (binaryFile.is_open()) {
        binaryFile.close();
    }
    binaryFile.clear();

    binaryFile.open(".fizzLocalCache.bin", std::ios::binary);
    if (!binaryFile.is_open()) {
        std::cout << "[ERROR] Could not open .fizzLocalCache.bin to populate RAM cache!\n";
        return;
    }

    size_t charCount = 0;
    binaryFile.seekg(8, std::ios::beg);
    binaryFile.read(reinterpret_cast<char*>(&charCount), sizeof(charCount));
    
    if (charCount > 0) {
        g_InMemoryIndexBuffer.resize(charCount);
        binaryFile.seekg(-static_cast<std::streamoff>(charCount), std::ios::end);
        binaryFile.read(g_InMemoryIndexBuffer.data(), charCount);
    }
    binaryFile.close();
    std::cout << "[SYSTEM] Cached " << g_InMemoryIndexBuffer.size() << " bytes into RAM.\n";
}

