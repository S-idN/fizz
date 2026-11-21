#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <vector>

enum class ViewMode {
    Search,
    Settings,
    About
};

const char* viewModeToString(ViewMode mode);
ViewMode stringToViewMode(std::string_view str);

struct SearchHit;

class ViewModeStateMachine {
public:
    using ModeChangeCallback = std::function<void(ViewMode oldMode, ViewMode newMode)>;
    using CancelSearchCallback = std::function<void()>;

    explicit ViewModeStateMachine(ViewMode initialMode = ViewMode::Search);

    ViewMode getCurrentMode() const;
    bool transitionTo(ViewMode newMode);

    // Handles Escape key press according to active ViewMode:
    // - In Settings or About: transitions back to Search view without exit, returning false.
    // - In Search: signals application exit, returning true.
    bool handleEscape();

    void setOnModeChange(ModeChangeCallback callback);
    void setOnCancelSearch(CancelSearchCallback callback);
    void setSearchStatePointers(std::string* query, std::vector<SearchHit>* results, char* buf = nullptr);

private:
    ViewMode m_currentMode = ViewMode::Search;
    ModeChangeCallback m_modeChangeCallback;
    CancelSearchCallback m_cancelSearchCallback;
    std::string* m_queryPtr = nullptr;
    std::vector<SearchHit>* m_resultsPtr = nullptr;
    char* m_bufPtr = nullptr;
};

namespace fizz {
    using ::ViewMode;
    using ::viewModeToString;
    using ::stringToViewMode;
    using ::ViewModeStateMachine;
}
