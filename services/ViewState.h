#pragma once

#include "ViewMode.h"
#include <string>
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>

// Saves the active view mode and timestamp to the runtime state file.
// If mode is ViewMode::Search, clears the state file instead.
// File is created with O_NOFOLLOW | O_CLOEXEC and permissions 0600.
// If customTimestamp is 0, the current time is used.
// Returns true on success.
bool saveViewState(ViewMode mode, const std::string& customPath = "", int64_t customTimestamp = 0);

// Loads the view state from the runtime state file.
// Validates regular file (!S_ISLNK, S_ISREG) and ownership (st_uid == geteuid()).
// If elapsed time <= graceSeconds, returns the saved ViewMode.
// If elapsed time > graceSeconds, or file is invalid/corrupt, clears state file and returns ViewMode::Search.
// If currentTimestamp is 0, the current time is used.
ViewMode loadViewState(int graceSeconds, const std::string& customPath = "", int64_t currentTimestamp = 0, uid_t expectedUid = geteuid());

// Clears (unlinks) the runtime state file.
// Returns true if cleared or did not exist.
bool clearViewState(const std::string& customPath = "");

// Resolves default path to runtime state file ($XDG_RUNTIME_DIR/fizz/view_state).
std::string getViewStateFilePath();

namespace fizz {
    using ::saveViewState;
    using ::loadViewState;
    using ::clearViewState;
    using ::getViewStateFilePath;
}
