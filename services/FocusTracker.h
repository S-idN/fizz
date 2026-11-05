#pragma once

class FocusTracker {
public:
    explicit FocusTracker(bool autoCloseOnUnfocus = true);

    bool isArmed() const;

    void setAutoCloseOnUnfocus(bool enabled);
    bool getAutoCloseOnUnfocus() const;

    // Handles window focus change events (e.g. from glfwSetWindowFocusCallback).
    // Returns true if this focus transition warrants dismissing/closing the window.
    bool handleFocusChange(bool focused);
    bool handleFocusChange(int focused);

private:
    bool m_armed = false;
    bool m_autoCloseOnUnfocus = true;
};
