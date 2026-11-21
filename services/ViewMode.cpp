#include "ViewMode.h"
#include "FileSearch.h"
#include "QueryOps.h"
#include <algorithm>
#include <cctype>

const char* viewModeToString(ViewMode mode) {
    switch (mode) {
        case ViewMode::Search:   return "Search";
        case ViewMode::Settings: return "Settings";
        case ViewMode::About:    return "About";
    }
    return "Search";
}

static bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

ViewMode stringToViewMode(std::string_view str) {
    if (iequals(str, "Search")) {
        return ViewMode::Search;
    }
    if (iequals(str, "Settings")) {
        return ViewMode::Settings;
    }
    if (iequals(str, "About")) {
        return ViewMode::About;
    }
    return ViewMode::Search;
}

ViewModeStateMachine::ViewModeStateMachine(ViewMode initialMode)
    : m_currentMode(initialMode) {}

ViewMode ViewModeStateMachine::getCurrentMode() const {
    return m_currentMode;
}

bool ViewModeStateMachine::transitionTo(ViewMode newMode) {
    if (m_currentMode == newMode) {
        return false;
    }

    ViewMode oldMode = m_currentMode;

    if (oldMode == ViewMode::Search && (newMode == ViewMode::Settings || newMode == ViewMode::About)) {
        setSearchInterrupted(true);
        if (m_cancelSearchCallback) {
            m_cancelSearchCallback();
        }
        if (m_queryPtr) {
            m_queryPtr->clear();
        }
        if (m_resultsPtr) {
            m_resultsPtr->clear();
        }
        if (m_bufPtr) {
            m_bufPtr[0] = '\0';
        }
    } else if (newMode == ViewMode::Search) {
        setSearchInterrupted(false);
    }

    m_currentMode = newMode;

    if (m_modeChangeCallback) {
        m_modeChangeCallback(oldMode, newMode);
    }

    return true;
}

bool ViewModeStateMachine::handleEscape() {
    if (m_currentMode == ViewMode::Settings || m_currentMode == ViewMode::About) {
        transitionTo(ViewMode::Search);
        return false;
    }
    return true;
}

void ViewModeStateMachine::setOnModeChange(ModeChangeCallback callback) {
    m_modeChangeCallback = std::move(callback);
}

void ViewModeStateMachine::setOnCancelSearch(CancelSearchCallback callback) {
    m_cancelSearchCallback = std::move(callback);
}

void ViewModeStateMachine::setSearchStatePointers(std::string* query, std::vector<SearchHit>* results, char* buf) {
    m_queryPtr = query;
    m_resultsPtr = results;
    m_bufPtr = buf;
}
