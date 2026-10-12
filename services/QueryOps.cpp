#include <algorithm>
#include <cctype>
#include <iostream>
#include <thread>
#include <vector>
#include <string_view>
#include "LoadFile.h"
#include "QueryOps.h"

namespace {

constexpr size_t kMaxQueryLen = 255;   
constexpr int    kMaxTokens   = 16;

inline char lowerChar(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

struct PreparedQuery {
    char buf[kMaxQueryLen + 1];
    std::string_view tokens[kMaxTokens];
    int count = 0;
};

void prepareQuery(std::string_view q, PreparedQuery& pq) {
    size_t n = std::min(q.size(), kMaxQueryLen);
    for (size_t i = 0; i < n; ++i) pq.buf[i] = lowerChar(q[i]);

    pq.count = 0;
    size_t i = 0;
    while (i < n && pq.count < kMaxTokens) {
        while (i < n && pq.buf[i] == ' ') ++i;
        if (i >= n) break;
        size_t j = i;
        while (j < n && pq.buf[j] != ' ') ++j;
        pq.tokens[pq.count++] = std::string_view(pq.buf + i, j - i);
        i = j;
    }
}

int scoreEntry(std::string_view path, const PreparedQuery& pq, int maxDistance) {
    size_t slash = path.find_last_of('/');
    std::string_view name = (slash == std::string_view::npos) ? path : path.substr(slash + 1);
    if (name.empty() || pq.count == 0) return 0;

    char fn[256];
    const size_t flen = std::min(name.size(), sizeof(fn));
    for (size_t i = 0; i < flen; ++i) fn[i] = lowerChar(name[i]);
    const std::string_view filename(fn, flen);
    const int fl = static_cast<int>(flen);

    int total = 0;

    for (int t = 0; t < pq.count; ++t) {
        const std::string_view sub = pq.tokens[t];
        const int ql = static_cast<int>(sub.size());
        int s = 0;

        if (flen == sub.size() && filename == sub) s = 10000;

        size_t pos = filename.find(sub);
        if (pos != std::string_view::npos) {
            int score = 5000;
            if (pos == 0) score += 1000;
            score += std::max(0, 500 - fl);
            s = std::max(s, score);
        }

        if (s == 0 && ql > maxDistance) {
            int qIdx = 0;
            for (int f = 0; f < fl && qIdx < ql; ++f)
                if (fn[f] == sub[qIdx]) ++qIdx;

            if (qIdx >= ql - maxDistance) {
                int rowA[kMaxQueryLen + 1], rowB[kMaxQueryLen + 1];
                int* prev = rowA;
                int* curr = rowB;
                for (int j = 0; j <= ql; ++j) curr[j] = 0;

                int best = maxDistance + 1;
                const int wStart = std::max(1, ql - maxDistance);
                const int wEnd   = ql + maxDistance;

                for (int w = wStart; w <= wEnd && w <= fl; ++w) {
                    for (int sp = 0; sp + w <= fl; ++sp) {
                        const char* win = fn + sp;
                        for (int j = 0; j <= ql; ++j) prev[j] = j;
                        bool ok = true;

                        for (int i = 1; i <= w; ++i) {
                            curr[0] = i;
                            bool exceed = true;
                            const int start = std::max(1, i - maxDistance);
                            const int end   = std::min(ql, i + maxDistance);
                            if (i - maxDistance <= 0 && curr[0] <= maxDistance) exceed = false;

                            for (int j = start; j <= end; ++j) {
                                if (win[i - 1] == sub[j - 1]) curr[j] = prev[j - 1];
                                else curr[j] = 1 + std::min({prev[j - 1], prev[j], curr[j - 1]});
                                if (curr[j] <= maxDistance) exceed = false;
                            }
                            if (exceed) { ok = false; break; }
                            std::swap(prev, curr);
                        }
                        if (ok && prev[ql] <= maxDistance) best = std::min(best, prev[ql]);
                    }
                }
                if (best <= maxDistance) s = std::max(s, 1000 / (best + 1));
            }
        }

        if (s == 0) return 0;   
        total += s;
    }
    return total;
}

void searchSegment(std::string_view seg, const PreparedQuery& pq, int maxDist,
                   size_t limit, std::vector<SearchHit>& top) {
    auto cmp = [](const SearchHit& a, const SearchHit& b) { return a.score > b.score; };
    top.clear();

    size_t start = 0;
    while (start < seg.size()) {
        size_t end = seg.find('\0', start);
        if (end == std::string_view::npos) end = seg.size();

        if (end > start) {
            std::string_view path(seg.data() + start, end - start);
            int score = scoreEntry(path, pq, maxDist);
            if (score > 0) {
                if (top.size() < limit) {
                    top.push_back({path, score});
                    std::push_heap(top.begin(), top.end(), cmp);
                } else if (score > top.front().score) {
                    std::pop_heap(top.begin(), top.end(), cmp);
                    top.back() = {path, score};
                    std::push_heap(top.begin(), top.end(), cmp);
                }
            }
        }
        start = end + 1;
    }
}

} 

void searchPaths(std::string_view query, std::vector<SearchHit>& out, size_t limit) {
    out.clear();
    if (g_InMemoryIndexBuffer.empty() || query.empty() || limit == 0) return;

    PreparedQuery pq;
    prepareQuery(query, pq);
    if (pq.count == 0) return;

    const int maxDistance = 2;
    std::string_view full(g_InMemoryIndexBuffer.data(), g_InMemoryIndexBuffer.size());

    unsigned hw = std::thread::hardware_concurrency();
    const int nThreads = hw ? static_cast<int>(hw) : 4;

    static std::vector<std::vector<SearchHit>> partials;   
    partials.resize(nThreads);

    std::vector<std::thread> threads;
    threads.reserve(nThreads);

    const size_t total = full.size();
    const size_t chunk = total / nThreads;
    size_t cur = 0;
    int spawned = 0;

    for (int i = 0; i < nThreads && cur < total; ++i) {
        size_t end = cur + chunk;
        if (end < total) {
            size_t d = full.find('\0', end);
            end = (d == std::string_view::npos) ? total : d + 1;
        } else {
            end = total;
        }
        std::string_view seg = full.substr(cur, end - cur);
        const int idx = spawned++;
        threads.emplace_back([&pq, &limit, seg, idx, maxDistance] {
            searchSegment(seg, pq, maxDistance, limit, partials[idx]);
        });
        cur = end;
    }
    for (auto& th : threads) th.join();

    for (int i = 0; i < spawned; ++i)
        out.insert(out.end(), partials[i].begin(), partials[i].end());

    std::sort(out.begin(), out.end(), [](const SearchHit& a, const SearchHit& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.path.data() < b.path.data();      
    });
    if (out.size() > limit) out.resize(limit);
}

void performSearch(const std::string& inpString) {
    std::vector<SearchHit> hits;
    searchPaths(inpString, hits, 100);
    std::cout << "\n--- Search Results (Sorted by Relevance) ---\n";
    for (const auto& h : hits)
        std::cout << "[Score: " << h.score << "] " << h.path << "\n";
}
