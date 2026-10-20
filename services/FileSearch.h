#pragma once
#include <iostream>
#include <optional>

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

void systemFileSearch(const std::string myName);
