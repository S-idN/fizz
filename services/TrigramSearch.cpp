#include "TrigramSearch.h"
#include "TrigramIndex.h"   

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

static const size_t   kMaxIndexBytes = 1 << 20;
static const uint32_t kMagic         = 0x31495446; //FTI1, ids the file
static const uint32_t kVersion       = 2;

static void*           g_map       = nullptr;
static size_t          g_mapLen    = 0;
static const uint32_t* g_pathOffs  = nullptr;
static const TriSlot*  g_slots     = nullptr;
static const uint32_t* g_post      = nullptr;
static const char*     g_paths     = nullptr;
static uint32_t        g_fileCount = 0;
static uint32_t        g_slotCount = 0;
static uint32_t        g_postCount = 0;
static uint32_t        g_pathBytes = 0;

static uint8_t lowerByte(uint8_t c) { return (c >= 'A' && c <= 'Z') ? (uint8_t)(c + 32) : c; }

void triUnload() {
    if (g_map) munmap(g_map, g_mapLen);
    g_map = nullptr; g_mapLen = 0;
    g_pathOffs = nullptr; g_slots = nullptr; g_post = nullptr; g_paths = nullptr;
    g_fileCount = g_slotCount = g_postCount = g_pathBytes = 0;
}

bool triLoaded() { return g_map != nullptr; }

bool triLoad(const std::string& path) {
    triUnload();

    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(TriHeader)) { close(fd); return false; }

    //To access file data, mapping and all that
    void* m = mmap(nullptr, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m == MAP_FAILED) return false;

    const TriHeader* h = static_cast<const TriHeader*>(m);
    size_t need = sizeof(TriHeader)
                + (size_t)h->fileCount * sizeof(uint32_t)
                + (size_t)h->slotCount * sizeof(TriSlot)
                + (size_t)h->postingsCount * sizeof(uint32_t)
                + (size_t)h->pathsBytes;

    if (h->magic != kMagic || h->version != kVersion || need != (size_t)st.st_size) {
        munmap(m, (size_t)st.st_size);
        return false;
    }

    const char* p = static_cast<const char*>(m) + sizeof(TriHeader);
    g_pathOffs = reinterpret_cast<const uint32_t*>(p); p += (size_t)h->fileCount * sizeof(uint32_t);
    g_slots    = reinterpret_cast<const TriSlot*>(p);  p += (size_t)h->slotCount * sizeof(TriSlot);
    g_post     = reinterpret_cast<const uint32_t*>(p); p += (size_t)h->postingsCount * sizeof(uint32_t);
    g_paths    = p;

    g_fileCount = h->fileCount;
    g_slotCount = h->slotCount;
    g_postCount = h->postingsCount;
    g_pathBytes = h->pathsBytes;
    g_map = m;
    g_mapLen = (size_t)st.st_size;
    return true;
}

// Kindo of a redundant check? Assess later mayb
static bool fileContains(const char* path, const std::string& lq) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;

    static thread_local std::vector<char> data(kMaxIndexBytes);   
    size_t got = 0;
    while (got < data.size()) {
        ssize_t n = read(fd, data.data() + got, data.size() - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    close(fd);

    for (size_t i = 0; i < got; i++) data[i] = (char)lowerByte((uint8_t)data[i]);
    return std::string_view(data.data(), got).find(lq) != std::string_view::npos;
}

void triSearch(std::string_view q, std::vector<std::string>& out, size_t limit) {
    out.clear();
    if (!g_map || q.size() < 3 || limit == 0) return;

    std::string lq(q);
    for (char& c : lq) c = (char)lowerByte((uint8_t)c);

    struct List { const uint32_t* p; uint32_t n; };
    std::vector<List> lists;
    const TriSlot* sb = g_slots;
    const TriSlot* se = g_slots + g_slotCount;

    for (size_t i = 0; i + 3 <= lq.size(); i++) {
        uint32_t key = ((uint32_t)(uint8_t)lq[i] << 16)
                     | ((uint32_t)(uint8_t)lq[i + 1] << 8)
                     |  (uint32_t)(uint8_t)lq[i + 2];

        const TriSlot* s = std::lower_bound(sb, se, key,
            [](const TriSlot& a, uint32_t k) { return a.key < k; });

        if (s == se || s->key != key) return;                    
        if ((size_t)s->off + s->len > g_postCount) return;       
        lists.push_back({g_post + s->off, s->len});
    }

    // Start from the shortest list, intersect the rest.
    std::sort(lists.begin(), lists.end(),
              [](const List& a, const List& b) { return a.n < b.n; });

    std::vector<uint32_t> cand(lists[0].p, lists[0].p + lists[0].n), next;
    for (size_t k = 1; k < lists.size() && !cand.empty(); k++) {
        next.clear();
        std::set_intersection(cand.begin(), cand.end(),
                              lists[k].p, lists[k].p + lists[k].n,
                              std::back_inserter(next));
        cand.swap(next);
    }

    for (uint32_t id : cand) {
        if (out.size() >= limit) break;
        if (id >= g_fileCount || g_pathOffs[id] >= g_pathBytes) continue;
        const char* path = g_paths + g_pathOffs[id];
        if (fileContains(path, lq)) out.emplace_back(path);
    }
}
