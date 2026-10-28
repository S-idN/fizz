// #pragma once
// #include <sys/stat.h>
// #include <cstdlib>
// #include <string>
//
// inline std::string fizzCacheDir() {
//     const char* xdg  = std::getenv("XDG_CACHE_HOME");
//     const char* home = std::getenv("HOME");
//     if (xdg && *xdg)   return std::string(xdg) + "/fizz";
//     if (home && *home) return std::string(home) + "/.cache/fizz";
//     return "";
// }
//
// inline std::string fizzFileCachePath() {
//     std::string d = fizzCacheDir();
//     return d.empty() ? "" : d + "/files.bin";
// }
//
// inline std::string fizzContentIndexPath() {
//     std::string d = fizzCacheDir();
//     return d.empty() ? "" : d + "/content.idx";
// }
//
// inline bool fizzEnsureCacheDir() {
//     std::string d = fizzCacheDir();
//     if (d.empty()) return false;
//     std::string parent = d.substr(0, d.rfind('/'));
//     if (!parent.empty()) mkdir(parent.c_str(), 0700);
//     mkdir(d.c_str(), 0700);
//     struct stat st;
//     return stat(d.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
// }
