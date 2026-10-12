#pragma once
#include <string>
#include <string_view>
#include <vector>

struct SearchHit {
    std::string_view path;   // points into g_InMemoryIndexBuffer
    int score;
};

void searchPaths(std::string_view query, std::vector<SearchHit>& out, size_t limit = 50);

void performSearch(const std::string& inpString);
