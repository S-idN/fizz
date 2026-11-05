#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#include <pwd.h>
#include <vector>
#include "FileSearch.h"

std::vector<std::string> stack = {};

std::string rootDir = "/";

std::vector<std::string> absoluteBlacklist = {
    "/dev", "/proc", 
    "/sys"};

std::vector<std::string> componentBlacklist = {
    ".cache", ".git", "node_modules", "Flatpak", ".local/share/Trash"
};

const std::string hiddenPattern(R"((^|[/\\])\.[^/\\]+)");

std::vector<fileEntity> fileStructTable;
std::vector<char> fileTable;

static std::string s_CustomBinaryCachePath = "";

std::string getBinaryCachePath() {
    if (!s_CustomBinaryCachePath.empty()) {
        return s_CustomBinaryCachePath;
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.fizzLocalCache.bin";
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
        return std::string(pw->pw_dir) + "/.fizzLocalCache.bin";
    }
    return "/tmp/.fizzLocalCache.bin";
}

void setBinaryCachePath(const std::string& path) {
    s_CustomBinaryCachePath = path;
}

// //Weird wizard shit
// //This is 128 in decimal or 1 0 0 0 0 0 0 0 in Decimal, bit 7 acts as the toggle
// #define FT_EXEC_MASK 0x80

void addToLocalCache(unsigned long inode, unsigned char f_type, size_t f_length, std::string f_name, ExecState isExecutable){
    struct fileEntity fileEnt;
    fileEnt.inode = inode;
    fileEnt.f_offset = fileTable.size();
    fileEnt.f_length = f_length + 1;
    fileEnt.f_type = f_type;
    fileEnt.f_execState = isExecutable;
	//    if (isExecutable == ExecState::Executable) {
	// fileEnt.f_type = f_type | FT_EXEC_MASK;
	//    } else {
	// fileEnt.f_type = f_type;
	//    }
    for(int i=0;i<f_length ;i++){
	fileTable.push_back(f_name[i]);
	// fileTable[fileTableOffset++] = f_name[i];
    }
    fileStructTable.push_back(fileEnt);
    fileTable.push_back('\0');
}

bool isHidden(const char* name) {
    return name[0] == '.' && name[1] != '\0' && !(name[1] == '.' && name[2] == '\0');
}

bool fileValChecker(std::string_view fullpath, const char *d_name) {
    for (const auto& blacklisted : absoluteBlacklist) {
        if (fullpath.substr(0,blacklisted.size())==blacklisted) {
            return false;
        }
    }

    for (const auto& blacklisted : componentBlacklist) {
        if (fullpath.find(blacklisted) != std::string_view::npos) {
            return false;
        }
    }
    return true;
}

void performSystemFileSearch(const std::string currPath) {
    fileEntity currFileEnt;
    // std::ofstream file("TestFilenames.txt", std::ios::app);
    // file << currPath+"\n";
    struct dirent *dp;
    struct stat st;
    struct stat lst;
    DIR *dirp = opendir(currPath.c_str());
    // std::cout << dirp << std::endl;
    if (dirp == nullptr) {
	return;
    }
    // std::cout << "[PATH]" << currPath << std::endl;

    while ((dp = readdir(dirp))!=NULL){
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
	    continue;
	}
	std::string currFileName = dp->d_name;
	// std::cout << dp->d_name << std::endl;
	currFileName = currPath + (currPath.back() == '/' ? "" : "/") + currFileName;
	
	if (!fileValChecker(currFileName,dp->d_name)){
	    continue;
	}
	// std::cout << "[new]: " << currFileName << std::endl;
	currFileEnt.inode = dp->d_ino;
	currFileEnt.f_length = std::strlen(dp->d_name);
	currFileEnt.f_type = dp->d_type ? dp->d_type : '\0';
	currFileEnt.f_execState = ExecState::NotExecutable;
	if (dp->d_type == DT_REG) {
	    struct stat st;
	    if (stat(currFileName.c_str(), &st) == 0) {
		bool isExec = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
		currFileEnt.f_execState = static_cast<ExecState>(isExec);
	    }

	}
	addToLocalCache(dp->d_ino, dp->d_type, currFileName.size(), currFileName, currFileEnt.f_execState);
	if (dp->d_type == DT_DIR) {
	    performSystemFileSearch(currFileName);
	} 
    }
    closedir(dirp);
}

void systemFileSearch(const std::string& currPath){
    fileStructTable.clear();
    fileTable.clear();

    const std::string binaryCachePath = getBinaryCachePath();
    std::ofstream binaryCacheStream(binaryCachePath, std::ios::binary | std::ios::trunc);
    if (!binaryCacheStream.is_open()) {
        std::cerr << "[ERROR] Could not open Binary Cache for writing: " << binaryCachePath << "\n";
        return;
    }
    
    performSystemFileSearch(currPath);
    //Error checking or somn idk add it later
    size_t structCount = fileStructTable.size();
    size_t charCount = fileTable.size();

    binaryCacheStream.write(reinterpret_cast<const char*>(&structCount), sizeof(structCount));
    binaryCacheStream.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));

    if (structCount > 0) {
        binaryCacheStream.write(reinterpret_cast<const char*>(fileStructTable.data()), 
	        structCount * sizeof(fileEntity));
    }

    if (charCount > 0) {
        binaryCacheStream.write(fileTable.data(), charCount);
    }

    binaryCacheStream.close();
}
