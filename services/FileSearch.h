#pragma once
#include <iostream>
#include <optional>
#include <string>
#include <vector>

enum class ExecState : unsigned char {
    NotExecutable = 0,
    Executable = 1
};

struct fileEntity {
    unsigned long inode;
    size_t f_offset;
    size_t f_length;
    // std::optional<bool> f_isExecutable;
    unsigned char f_type;
    ExecState f_execState;
};

extern std::vector<fileEntity> fileStructTable;

void systemFileSearch(const std::vector<std::string>& searchRoots);
void systemFileSearch(const std::string& currPath);
void systemFileSearch();

std::vector<std::string> getDefaultSearchRoots(bool existingOnly = true);

std::string_view classifyPath(std::string_view path, ExecState state);

std::string getBinaryCachePath();
void setBinaryCachePath(const std::string& path);
