#include <iostream>
#include <string>
#include "services/QueryOps.h"
#include "services/LoadFile.h"
#include "services/FileSearch.h"

int main() {
    systemFileSearch("/");
    loadIndexToMemory(); 

    std::cout << "Enter your fuzzy query: ";
    std::string inputBuffer;
    std::getline(std::cin, inputBuffer); 

    performSearch(inputBuffer);

    return 0;
}

