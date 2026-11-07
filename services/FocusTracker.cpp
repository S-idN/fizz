#include "FocusTracker.h"

FocusTracker::FocusTracker(bool autoCloseOnUnfocus)
    : m_armed(false), m_autoCloseOnUnfocus(autoCloseOnUnfocus) {}

bool FocusTracker::isArmed() const {
    return m_armed;
}

bool FocusTracker::autoCloseOnUnfocus() const {
    return m_autoCloseOnUnfocus;
}

bool FocusTracker::handleFocusChange(bool focused) {
    if (focused) {
        m_armed = true;
        return false;
    }

    // Losing focus before initial focus acquisition does not trigger dismissal
    // (e.g. during Wayland / tiling compositor mapping).
    if (!m_armed) {
        return false;
    }

    // Dismiss only if autoCloseOnUnfocus is enabled.
    return m_autoCloseOnUnfocus;
}
