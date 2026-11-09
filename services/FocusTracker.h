#pragma once

class FocusTracker {
public:
    explicit FocusTracker(bool autoCloseOnUnfocus = true);

    bool isArmed() const;
    bool autoCloseOnUnfocus() const;

    // Handles window focus change events (e.g. from glfwSetWindowFocusCallback).
    // Returns true if this focus transition warrants dismissing/closing the window.
    bool handleFocusChange(bool focused);

private:
    bool m_armed = false;
    bool m_autoCloseOnUnfocus = true;
};
