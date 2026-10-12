#include <iostream>
#include <vector>
#include <fstream>
#include "FileSearch.h"

std::vector<std::string> ignoreWords;
std::ifstream binaryFile(".fizzLocalCache.bin", std::ios::binary);

void stringTokenizer(std::string_view inpString, int strLen) {
    // std::cout << "FILE VIEW:" << inpString << "\n";
    int bgn = 0;
    int curr_id = 0;
    for (char c: inpString) {
	if (c == '\0' || c == '\\' || c == '/' || c == '-') {
	    int curr_token_len = curr_id - bgn;
	    // std::cout << "[bgn]" << inpString.substr(bgn, curr_token_len) << "[fin]\n";
	    bgn = curr_id + 1;
	}
	curr_id ++;
    }
    // std::cout << '\n';
}

void createInvertedSearchIndex(){
    fileEntity fileStruct;
    size_t structCount = 0;
    size_t charCount = 0;

    binaryFile.read(reinterpret_cast<char*>(&structCount), sizeof(structCount));
    binaryFile.read(reinterpret_cast<char*>(&charCount), sizeof(charCount));

    // std::cout << "Structs to read:" << structCount << "\n";
    // std::cout << "Chars to read:" << charCount << "\n";

    std::vector<fileEntity> entities(structCount);
    binaryFile.read(reinterpret_cast<char*>(entities.data()), structCount * sizeof(fileEntity));

    std::vector<char> charBuffer(charCount);
    binaryFile.read(charBuffer.data(), charCount);
    binaryFile.close();

    for (size_t i = 0; i < structCount; i++) {
	const fileEntity& currFileStruct = entities[i];

	std::string_view fileView(charBuffer.data() + currFileStruct.f_offset, currFileStruct.f_length);

	stringTokenizer(fileView, currFileStruct.f_length);
    }

    // std::cout << "End of createInvertedSearchIndex function\n";
}
