#include "TrigramIndex.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

static const size_t kMaxIndexBytes = 1 << 20;   
static const size_t   kBinarySniff = 4096;        
static const uint32_t kStartVal       = 0x31495446; 

static std::unordered_map<uint32_t, std::vector<uint32_t>> postings;

static std::vector<uint32_t> pathOffs;
static std::string paths;                        

static std::vector<uint64_t> seenBits(1 << 18);
static std::vector<uint32_t> touched;            
static std::vector<char> buf;                     
static uint8_t lowerByte(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static bool skipPath(const std::string& p) {
    const char* skip[] = {"/proc/", "/sys/", "/dev/", "/run/"};
    for (const char* s : skip) {
        if (p.compare(0, strlen(s), s) == 0) return true;
    }
    return false;
}

static bool readWhole(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 3) {
	close(fd);
	return false;
    }

    size_t want = st.st_size;
    if (want > kMaxIndexBytes) want = kMaxIndexBytes;   

    buf.resize(want);

    size_t got = 0;
    while (got < buf.size()) {
        ssize_t n = read(fd, buf.data() + got, buf.size() - got);
        if (n <= 0) break;
        got += n;
    }
    close(fd);

    buf.resize(got);
    return got >= 3;
}

void triAddFile(const std::string& path) {
    if (skipPath(path)) return;
    if (!readWhole(path)) return;

    size_t sniff = std::min(buf.size(), kBinarySniff);
    if (memchr(buf.data(), 0, sniff) != nullptr) return;

    uint32_t id = pathOffs.size();
    pathOffs.push_back(paths.size());
    paths += path;
    paths.push_back('\0');

    // Trigram bit 
    const uint8_t* b = reinterpret_cast<const uint8_t*>(buf.data());
    uint32_t t = (lowerByte(b[0]) << 8) | lowerByte(b[1]);

    for (size_t i = 2; i < buf.size(); i++) {
        t = ((t << 8) | lowerByte(b[i])) & 0xFFFFFF;

        uint64_t& word = seenBits[t / 64];
        uint64_t bit = 1ULL << (t % 64);
        if (word & bit) continue;

        word |= bit;
        touched.push_back(t);
        postings[t].push_back(id);   
    }

    for (uint32_t k : touched) {
        seenBits[k / 64] &= ~(1ULL << (k % 64));
    }
    touched.clear();
}

bool triWrite(const std::string& outPath) {
    std::vector<uint32_t> keys;
    for (auto& entry : postings) keys.push_back(entry.first);
    std::sort(keys.begin(), keys.end());

    std::vector<TriSlot> slots;
    uint32_t off = 0;
    for (uint32_t k : keys) {
        uint32_t len = postings[k].size();
        slots.push_back({k, off, len});
        off += len;
    }

    std::string tmp = outPath + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;

    TriHeader h;
    h.magic         = kStartVal; 
    h.version       = 2;
    h.fileCount     = pathOffs.size();
    h.slotCount     = slots.size();
    h.postingsCount = off;
    h.pathsBytes    = paths.size();

    bool ok = true;
    ok = ok && fwrite(&h, sizeof(h), 1, f) == 1;
    ok = ok && fwrite(pathOffs.data(), sizeof(uint32_t), pathOffs.size(), f) == pathOffs.size();
    ok = ok && fwrite(slots.data(), sizeof(TriSlot), slots.size(), f) == slots.size();
    for (uint32_t k : keys) {
        std::vector<uint32_t>& list = postings[k];
        ok = ok && fwrite(list.data(), sizeof(uint32_t), list.size(), f) == list.size();
    }
    ok = ok && fwrite(paths.data(), 1, paths.size(), f) == paths.size();
    if (fclose(f) != 0) ok = false;

    std::unordered_map<uint32_t, std::vector<uint32_t>>().swap(postings);
    pathOffs.clear();
    paths.clear();

    if (!ok) {
        unlink(tmp.c_str());
        return false;
    }
    return rename(tmp.c_str(), outPath.c_str()) == 0;
}
