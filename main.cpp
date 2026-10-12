#include <iostream>
#include "services/FileSearch.h"
#include "services/InvertedSearchIndex.h"
#include "services/QueryOps.h"

int firstLaunch = 0;

// int levenshteinCalc(std::string userInput, std::string comparisonString){
//
// }
//
// void initCache(){
// }
//

void callFileSearch(){
    const char *testMessage = "/";
    systemFileSearch(testMessage);
}

int main(){
    callFileSearch();
    createInvertedSearchIndex();
    std::cout << "Enter your fuzzy query: ";
    
    std::string inputBuffer;
    std::getline(std::cin, inputBuffer); 

    performSearch(inputBuffer);
}
