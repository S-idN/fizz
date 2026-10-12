#include <algorithm>
#include <cctype>
#include <thread>
#include <iostream>
#include <vector> 
#include <fstream> 
#include <unistd.h>
#include <string_view>
#include <mutex> 
#include "LoadFile.h"

extern std::ifstream binaryFile;
std::vector<std::pair<int, std::string>> globalMatches; 
std::mutex matchesMutex;


int levenshteinCheck(std::string_view path, std::string_view query, int maxDistance) {
    std::string_view filename = path;
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string_view::npos) {
        filename = path.substr(lastSlash + 1);
    }

    if (filename.empty() || query.empty()) return 0;

    auto toLowerChar = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    size_t filenameLen = filename.length();

    std::vector<std::string_view> queryTokens;
    size_t qStart = 0;
    while (qStart < query.length()) {
        while (qStart < query.length() && query[qStart] == ' ') {
            qStart++;
        }
        if (qStart >= query.length()) break;
        
        size_t qEnd = query.find(' ', qStart);
        if (qEnd == std::string_view::npos) qEnd = query.length();
        
        queryTokens.push_back(query.substr(qStart, qEnd - qStart));
        qStart = qEnd + 1;
    }

    if (queryTokens.empty()) return 0;

    int totalScore = 0;
    int matchesCount = 0;

    for (std::string_view subQuery : queryTokens) {
        size_t queryLen = subQuery.length();
        int subQueryScore = 0;

        bool exactMatch = (filenameLen == queryLen);
        if (exactMatch) {
            for (size_t i = 0; i < queryLen; ++i) {
                if (toLowerChar(filename[i]) != toLowerChar(subQuery[i])) {
                    exactMatch = false;
                    break;
                }
            }
            if (exactMatch) subQueryScore = std::max(subQueryScore, 10000);
        }

        bool isSubstring = false;
        size_t substringPos = std::string_view::npos;
        if (filenameLen >= queryLen) {
            for (size_t i = 0; i <= filenameLen - queryLen; ++i) {
                bool match = true;
                for (size_t j = 0; j < queryLen; ++j) {
                    if (toLowerChar(filename[i + j]) != toLowerChar(subQuery[j])) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    isSubstring = true;
                    substringPos = i;
                    break;
                }
            }
        }

        if (isSubstring) {
            int score = 5000;
            if (substringPos == 0) score += 1000;
            score += std::max(0, 500 - static_cast<int>(filenameLen));
            subQueryScore = std::max(subQueryScore, score);
        }

        size_t qIdx = 0;
        for (size_t fIdx = 0; fIdx < filenameLen && qIdx < queryLen; ++fIdx) {
            if (toLowerChar(filename[fIdx]) == toLowerChar(subQuery[qIdx])) {
                qIdx++;
            }
        }
        
        if (qIdx >= (queryLen - maxDistance)) {
            int windowSizeStart = std::max(1, static_cast<int>(queryLen) - maxDistance);
            int windowSizeEnd = static_cast<int>(queryLen) + maxDistance;

            std::vector<int> prevRow(queryLen + 1);
            std::vector<int> currRow(queryLen + 1, 0);
            int bestLevDistance = maxDistance + 1;

            for (int wSize = windowSizeStart; wSize <= windowSizeEnd; ++wSize) {
                if (wSize > static_cast<int>(filenameLen)) break;

                for (int startPos = 0; startPos <= static_cast<int>(filenameLen) - wSize; ++startPos) {
                    std::string_view filenameWindow = filename.substr(startPos, wSize);
                    for (int j = 0; j <= queryLen; ++j) prevRow[j] = j;
                    bool windowMatched = true;

                    for (int i = 1; i <= wSize; ++i) {
                        currRow[0] = i;
                        bool exceedErrLim = true;
                        int start = std::max(1, i - maxDistance);
                        int end = std::min(static_cast<int>(queryLen), i + maxDistance);

                        if (i - maxDistance <= 0 && currRow[0] <= maxDistance) exceedErrLim = false;

                        for (int j = start; j <= end; ++j) {
                            if (toLowerChar(filenameWindow[i - 1]) == toLowerChar(subQuery[j - 1])) {
                                currRow[j] = prevRow[j - 1];
                            } else {
                                currRow[j] = 1 + std::min({prevRow[j - 1], prevRow[j], currRow[j - 1]});
                            }
                            if (currRow[j] <= maxDistance) exceedErrLim = false;
                        }

                        if (exceedErrLim) {
                            windowMatched = false;
                            break; 
                        }
                        std::swap(prevRow, currRow);
                    }
                    if (windowMatched && prevRow[queryLen] <= maxDistance) {
                        bestLevDistance = std::min(bestLevDistance, prevRow[queryLen]);
                    }
                }
            }

            if (bestLevDistance <= maxDistance) {
                subQueryScore = std::max(subQueryScore, 1000 / (bestLevDistance + 1));
            }
        }

        if (subQueryScore > 0) {
            totalScore += subQueryScore;
            matchesCount++;
        }
    }

    if (matchesCount < static_cast<int>(queryTokens.size())) {
        return 0; 
    }

    return totalScore;
}

void searchSegmentWorker(std::string_view segment, std::string_view query, int maxDistance) {
    size_t startIdx = 0;
    size_t segmentSize = segment.size();
    
    std::vector<std::pair<int, std::string_view>> localMatches;

    while (startIdx < segmentSize) {
        size_t endIdx = segment.find('\0', startIdx);
        if (endIdx == std::string_view::npos) {
            endIdx = segmentSize;
        }

        size_t length = endIdx - startIdx;
        if (length > 0) {
            std::string_view pathStr(&segment[startIdx], length);
            
            int score = levenshteinCheck(pathStr, query, maxDistance);
            if (score > 0) {
                localMatches.push_back({score, pathStr});
            }
        }
        startIdx = endIdx + 1;
    }

    if (!localMatches.empty()) {
        std::lock_guard<std::mutex> lock(matchesMutex);
        for (const auto& match : localMatches) {
	    globalMatches.push_back({match.first, std::string(match.second)}); 
        }
    }
}

void performSearch(const std::string& inpString) {
    if (g_InMemoryIndexBuffer.empty()) { 
        return; 
    }

    std::string_view fullBufferView(g_InMemoryIndexBuffer.data(), g_InMemoryIndexBuffer.size());
    std::string_view query = inpString; 
    int maxDistance = 2;             

    unsigned int numCores = std::thread::hardware_concurrency();
    int threadNums = (numCores > 0) ? numCores : 4; 
    
    std::vector<std::thread> threads;
    globalMatches.clear(); 

    size_t totalSize = fullBufferView.size();
    size_t chunkEstimate = totalSize / threadNums;
    size_t currentStart = 0;

    for (int i = 0; i < threadNums; ++i) {
        if (currentStart >= totalSize) break;
        size_t currentEnd = currentStart + chunkEstimate;
        
        if (currentEnd < totalSize) {
            size_t nextDelimiter = fullBufferView.find('\0', currentEnd);
            if (nextDelimiter != std::string_view::npos) {
                currentEnd = nextDelimiter + 1; 
            } else {
                currentEnd = totalSize;
            }
        } else {
            currentEnd = totalSize;
        }

        std::string_view threadChunk = fullBufferView.substr(currentStart, currentEnd - currentStart);
        threads.emplace_back(searchSegmentWorker, threadChunk, query, maxDistance);
        currentStart = currentEnd;
    }

    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    std::sort(globalMatches.begin(), globalMatches.end(), 
        [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
            return a.first > b.first;
        }
    );

    std::cout << "\n--- Search Results (Sorted by Relevance) ---\n";
    for (const auto& match : globalMatches) {
        std::cout << "[Score: " << match.first << "] " << match.second << "\n";
    }
}

