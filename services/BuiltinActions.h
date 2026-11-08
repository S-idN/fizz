#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "QueryOps.h"
#include "ViewMode.h"
#include "Theme.h"

namespace fizz {

// Canonical action paths
inline constexpr std::string_view kActionPathSettings = "fizz:settings";
inline constexpr std::string_view kActionPathAbout    = "fizz:about";

// Display titles for builtin actions
inline constexpr std::string_view kActionTitleSettings = "⚙️ Fizz Settings";
inline constexpr std::string_view kActionTitleAbout    = "ℹ️ About Fizz";

// Dedicated badge label for builtin actions
inline constexpr std::string_view kActionBadgeLabel = "action";

enum class BuiltinActionType {
    None,
    Settings,
    About
};

struct BuiltinActionHit {
    BuiltinActionType type = BuiltinActionType::None;
    ViewMode targetMode = ViewMode::Search;
    std::string_view actionPath;
    std::string_view title;
    std::string_view description;
    int score = 1000;
};

// Checks if a query matches any builtin action reserved keywords:
// - Settings: "settings", "fizz settings", "fizz:settings", ":settings"
// - About: "about", "fizz about", "fizz:about", ":about"
// Case-insensitive, trims leading/trailing whitespace.
BuiltinActionType matchBuiltinActionQuery(std::string_view query);

// Returns true if query matches a reserved builtin action keyword
bool isBuiltinActionQuery(std::string_view query);

// Generates BuiltinActionHit instances matching the query
std::vector<BuiltinActionHit> generateBuiltinActionHits(std::string_view query);

// Converts a BuiltinActionHit into a SearchHit suitable for frontend result lists
SearchHit toSearchHit(const BuiltinActionHit& action);

// Returns true if path is a builtin action URI (e.g. "fizz:settings", "fizz:about", ":settings", ":about")
bool isBuiltinActionPath(std::string_view path);

// Returns true if hit is a builtin action
bool isBuiltinAction(const SearchHit& hit);

// Gets display title for builtin action path (e.g. "⚙️ Fizz Settings" for "fizz:settings")
std::string_view getBuiltinActionTitle(std::string_view path);

// Gets target ViewMode for builtin action path
std::optional<ViewMode> getBuiltinActionTargetMode(std::string_view path);

// Checks if a path is a desktop entry that shares or matches the query name
bool desktopEntryMatchesQuery(std::string_view path, std::string_view query);

// Ranks search results alongside synthesized builtin action hits according to App-First discovery rules:
// - Matching installed Desktop Entries strictly ahead of Builtin Action Hits.
// - Builtin Action Hits strictly ahead of regular (non-desktop) files.
// - Regular files follow Builtin Action Hits.
std::vector<SearchHit> rankHitsWithBuiltinActions(
    std::string_view query,
    const std::vector<SearchHit>& fileHits,
    size_t limit = 50
);

void rankHitsWithBuiltinActionsInPlace(
    std::string_view query,
    std::vector<SearchHit>& inOutHits,
    size_t limit = 50
);

// Activates / executes a builtin action on the ViewModeStateMachine
bool executeBuiltinAction(std::string_view path, ViewModeStateMachine& stateMachine);

} // namespace fizz
