#include <iostream>

struct fileEntity {
    unsigned long inode;
    unsigned char f_type;
    size_t f_offset;
    size_t f_length;
};

void systemFileSearch(const std::string myName);
