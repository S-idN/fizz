#include "BuiltinActions.h"
#include "AppLauncher.h"
#include <cctype>
#include <algorithm>
#include <fstream>

namespace fizz {

namespace {

std::string normalizeQuery(std::string_view q) {
    while (!q.empty() && std::isspace(static_cast<unsigned char>(q.front()))) {
        q.remove_prefix(1);
    }
    while (!q.empty() && std::isspace(static_cast<unsigned char>(q.back()))) {
        q.remove_suffix(1);
    }
    std::string result;
    result.reserve(q.size());
    bool prevSpace = false;
    for (char ch : q) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!prevSpace) {
                result.push_back(' ');
                prevSpace = true;
            }
        } else {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            prevSpace = false;
        }
    }
    return result;
}

std::string extractLowerStem(std::string_view path) {
    size_t slash = path.rfind('/');
    std::string_view file = (slash == std::string_view::npos) ? path : path.substr(slash + 1);
    constexpr std::string_view kExt = ".desktop";
    if (file.size() >= kExt.size()) {
        std::string_view tail = file.substr(file.size() - kExt.size());
        bool matchExt = true;
        for (size_t i = 0; i < kExt.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(tail[i])) != kExt[i]) {
                matchExt = false;
                break;
            }
        }
        if (matchExt) {
            file.remove_suffix(kExt.size());
        }
    }
    std::string out;
    out.reserve(file.size());
    for (char ch : file) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

} // namespace

BuiltinActionType matchBuiltinActionQuery(std::string_view query) {
    std::string norm = normalizeQuery(query);
    if (norm.empty()) {
        return BuiltinActionType::None;
    }

    if (norm == "settings" || norm == "fizz settings" || norm == "fizz:settings" || norm == ":settings") {
        return BuiltinActionType::Settings;
    }

    if (norm == "about" || norm == "fizz about" || norm == "fizz:about" || norm == ":about") {
        return BuiltinActionType::About;
    }

    return BuiltinActionType::None;
}

bool isBuiltinActionQuery(std::string_view query) {
    return matchBuiltinActionQuery(query) != BuiltinActionType::None;
}

std::vector<BuiltinActionHit> generateBuiltinActionHits(std::string_view query) {
    BuiltinActionType type = matchBuiltinActionQuery(query);
    std::vector<BuiltinActionHit> hits;
    if (type == BuiltinActionType::Settings) {
        hits.push_back(BuiltinActionHit{
            BuiltinActionType::Settings,
            ViewMode::Settings,
            kActionPathSettings,
            kActionTitleSettings,
            "In-App View",
            1000
        });
    } else if (type == BuiltinActionType::About) {
        hits.push_back(BuiltinActionHit{
            BuiltinActionType::About,
            ViewMode::About,
            kActionPathAbout,
            kActionTitleAbout,
            "In-App View",
            1000
        });
    }
    return hits;
}

SearchHit toSearchHit(const BuiltinActionHit& action) {
    return SearchHit{
        action.actionPath,
        ExecState::NotExecutable,
        action.score
    };
}

bool isBuiltinActionPath(std::string_view path) {
    return path == kActionPathSettings || path == kActionPathAbout ||
           path == ":settings" || path == ":about";
}

bool isBuiltinAction(const SearchHit& hit) {
    return isBuiltinActionPath(hit.path);
}

std::string_view getBuiltinActionTitle(std::string_view path) {
    if (path == kActionPathSettings || path == ":settings") {
        return kActionTitleSettings;
    }
    if (path == kActionPathAbout || path == ":about") {
        return kActionTitleAbout;
    }
    return "";
}

std::optional<ViewMode> getBuiltinActionTargetMode(std::string_view path) {
    if (path == kActionPathSettings || path == ":settings") {
        return ViewMode::Settings;
    }
    if (path == kActionPathAbout || path == ":about") {
        return ViewMode::About;
    }
    return std::nullopt;
}

bool desktopEntryMatchesQuery(std::string_view path, std::string_view query) {
    if (!isDesktopEntry(path)) {
        return false;
    }
    BuiltinActionType actionType = matchBuiltinActionQuery(query);
    std::string stem = extractLowerStem(path);
    std::string targetKeyword;
    if (actionType == BuiltinActionType::Settings) {
        targetKeyword = "settings";
    } else if (actionType == BuiltinActionType::About) {
        targetKeyword = "about";
    } else {
        targetKeyword = normalizeQuery(query);
    }
    if (targetKeyword.empty()) {
        return false;
    }

    if (stem.find(targetKeyword) != std::string::npos) {
        return true;
    }

    // Also inspect Name= field in the .desktop file content
    std::string pathStr(path);
    std::ifstream infile(pathStr);
    if (infile.is_open()) {
        std::string line;
        bool inDesktopEntry = false;
        while (std::getline(infile, line)) {
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            std::string_view trimmed = std::string_view(line).substr(start);
            if (!trimmed.empty() && trimmed.front() == '[') {
                inDesktopEntry = (trimmed.rfind("[Desktop Entry]", 0) == 0);
                continue;
            }
            if (inDesktopEntry && trimmed.rfind("Name=", 0) == 0) {
                std::string val(trimmed.substr(5));
                std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (val.find(targetKeyword) != std::string::npos) {
                    return true;
                }
            }
        }
    }

    return false;
}

std::vector<SearchHit> rankHitsWithBuiltinActions(
    std::string_view query,
    const std::vector<SearchHit>& fileHits,
    size_t limit
) {
    auto actionHits = generateBuiltinActionHits(query);
    if (actionHits.empty()) {
        std::vector<SearchHit> out = fileHits;
        if (limit > 0 && out.size() > limit) {
            out.resize(limit);
        }
        return out;
    }

    std::vector<SearchHit> desktopMatches;
    std::vector<SearchHit> otherDesktopHits;
    std::vector<SearchHit> regularHits;

    for (const auto& hit : fileHits) {
        if (isDesktopEntry(hit.path)) {
            if (desktopEntryMatchesQuery(hit.path, query)) {
                desktopMatches.push_back(hit);
            } else {
                otherDesktopHits.push_back(hit);
            }
        } else {
            regularHits.push_back(hit);
        }
    }

    std::vector<SearchHit> results;
    results.reserve(desktopMatches.size() + otherDesktopHits.size() + actionHits.size() + regularHits.size());

    // 1. Installed Desktop Entries matching query strictly rank first
    for (auto& hit : desktopMatches) {
        results.push_back(std::move(hit));
    }

    // 2. Builtin Action Hits strictly rank ahead of regular files and non-matching desktop entries
    for (const auto& action : actionHits) {
        results.push_back(toSearchHit(action));
    }

    // 3. Other desktop entries in search results rank next
    for (auto& hit : otherDesktopHits) {
        results.push_back(std::move(hit));
    }

    // 4. Regular files follow
    for (auto& hit : regularHits) {
        results.push_back(std::move(hit));
    }

    if (limit > 0 && results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

void rankHitsWithBuiltinActionsInPlace(
    std::string_view query,
    std::vector<SearchHit>& inOutHits,
    size_t limit
) {
    inOutHits = rankHitsWithBuiltinActions(query, inOutHits, limit);
}

bool executeBuiltinAction(std::string_view path, ViewModeStateMachine& stateMachine) {
    auto targetMode = getBuiltinActionTargetMode(path);
    if (!targetMode.has_value()) {
        return false;
    }
    return stateMachine.transitionTo(*targetMode);
}

} // namespace fizz
