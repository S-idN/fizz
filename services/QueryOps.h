#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "FileSearch.h"

struct SearchHit {
    std::string_view path;   
    ExecState execState;
    // unsigned char f_type;
    int score;
};

void searchPaths(std::string_view query, std::vector<SearchHit>& out, size_t limit = 50);

void performSearch(const std::string& inpString);
