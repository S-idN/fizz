#include <dirent.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include "TrigramIndex.h"

static bool ignored(const char* name) {
    const char* skip[] = {".cache", ".git", "node_modules", "Flatpak", "Trash"};
    for (const char* s : skip) {
        if (strcmp(name, s) == 0) return true;
    }
    return false;
}

static void walk(const std::string& dir) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;

    dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (ignored(e->d_name)) continue;

        std::string path = dir + (dir.back() == '/' ? "" : "/") + e->d_name;

        unsigned char type = e->d_type;
        if (type == DT_UNKNOWN) {
            struct stat st;
            if (lstat(path.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) type = DT_DIR;
            else if (S_ISREG(st.st_mode)) type = DT_REG;
        }

        if (type == DT_DIR) walk(path);
        else if (type == DT_REG) triAddFile(path);   
    }
    closedir(d);
}

#ifdef FIZZINDEX_MAIN
int main(int argc, char** argv) {
    const char* home = getenv("HOME");
    if (!home) return 1;

    const char* xdg = getenv("XDG_CACHE_HOME");
    std::string cacheBase = (xdg && *xdg) ? std::string(xdg) : std::string(home) + "/.cache";
    std::string dir = cacheBase + "/fizz";
    mkdir(dir.c_str(), 0700);

    int lock = open((dir + "/fizzindex.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) < 0) return 0;

    setpriority(PRIO_PROCESS, 0, 19);   
    if (argc > 1) {
        for (int i = 1; i < argc; i++) walk(argv[i]);   
    } else {
        walk(home);
    }

    return triWrite(dir + "/content.idx") ? 0 : 2;
}
#endif
