#pragma once
#include <string>
#include <string_view>
#include <vector>

bool triLoad(const std::string& path);   
void triUnload();
bool triLoaded();
void triSearch(std::string_view query, std::vector<std::string>& out, size_t limit);
