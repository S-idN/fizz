#include <iostream>
#include <string>
#include "services/QueryOps.h"
#include "services/LoadFile.h"

int main() {
    loadIndexToMemory(); 
    
    std::cout << "Enter your fuzzy query: ";
    std::string inputBuffer;
    std::getline(std::cin, inputBuffer); 

    performSearch(inputBuffer);

    return 0;
}

