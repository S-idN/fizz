#pragma once
#include <vector>
#include <ctime>
#include <sys/stat.h>

extern std::vector<char> g_InMemoryIndexBuffer;

// Loads the Binary Cache into memory, updating the in-memory timestamp.
// Opens with O_NOFOLLOW | O_CLOEXEC | O_RDONLY to prevent symlink traversal.
bool loadIndexToMemory();

// Checks st_mtime on the Binary Cache. If the on-disk file is newer than the
// in-memory index timestamp, cleanly reloads the index buffer and struct table.
// Returns true if a reload was performed, false if already up-to-date or on error.
bool reloadIndexIfModified();

// Returns the timestamp (st_mtim) of the currently loaded in-memory index.
struct timespec getLoadedIndexTimestamp();

// Resets in-memory timestamp and index buffer state (for testing / cleanup).
void resetLoadedIndex();

