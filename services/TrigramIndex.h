#pragma once
#include <cstdint>
#include <string>

// Layout of content.idx on disk:
// [Header][pathOffs: fileCount x uint32][Slots: slotCount x Slot][postings: uint32s][paths: chars]

struct TriHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t fileCount;      
    uint32_t slotCount;      
    uint32_t postingsCount;  
    uint32_t pathsBytes;   
};

struct TriSlot {
    uint32_t key;     
    uint32_t off;   
    uint32_t len;     
};

void triAddFile(const std::string& path);        
bool triWrite(const std::string& outPath);      
