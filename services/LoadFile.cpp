#include "FileSearch.h"
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

#include "FileSearch.h"   
extern std::vector<fileEntity> fileStructTable;

void loadIndexToMemory() {
    std::ifstream f(cachePath(), std::ios::binary);
    if (!f.is_open()) {
        std::cout << "[ERROR] Could not open " << cachePath() << "\n";
        return;
    }

    size_t structCount = 0, charCount = 0;
    f.read(reinterpret_cast<char*>(&structCount), sizeof(structCount));
    f.read(reinterpret_cast<char*>(&charCount),   sizeof(charCount));
    if (!f) return;

    fileStructTable.resize(structCount);
    f.read(reinterpret_cast<char*>(fileStructTable.data()),
           structCount * sizeof(fileEntity));

    g_InMemoryIndexBuffer.resize(charCount);
    f.read(g_InMemoryIndexBuffer.data(), charCount);

    if (!f) {           fileStructTable.clear();
	g_InMemoryIndexBuffer.clear();
	std::cout << "[ERROR] Cache is truncated or in an old format; rebuild it.\n";
	return;
    }
    std::cout << "[SYSTEM] Loaded " << structCount << " entries, "
	<< charCount << " bytes.\n";
}
