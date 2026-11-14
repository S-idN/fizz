#include "GeneralTab.h"
#include "Theme.h"
#include "imgui.h"
#include <algorithm>

namespace fizz {

GeneralTab::GeneralTab(std::string configPath, std::chrono::milliseconds debounceDuration)
    : m_configPath(std::move(configPath)), m_debounceDuration(debounceDuration) {}

const std::string& GeneralTab::getConfigPath() const {
    return m_configPath;
}

void GeneralTab::setConfigPath(const std::string& path) {
    m_configPath = path;
}

void GeneralTab::setOnConfigChanged(ConfigChangeCallback cb) {
    m_onConfigChanged = std::move(cb);
}

void GeneralTab::setOnReset(ConfigChangeCallback cb) {
    m_onReset = std::move(cb);
}

bool GeneralTab::save(const FizzConfig& config) {
    std::string path = m_configPath.empty() ? getConfigPath() : m_configPath;
    bool success = saveConfig(config, path);
    if (success) {
        m_saveCount++;
    }
    return success;
}

bool GeneralTab::setAutoClose(FizzConfig& config, bool enabled) {
    config.autoCloseOnUnfocus = enabled;
    m_hasPendingSave = false;
    bool ok = save(config);
    if (m_onConfigChanged) {
        m_onConfigChanged(config);
    }
    return ok;
}

void GeneralTab::setGracePeriod(FizzConfig& config, int seconds,
                                std::chrono::steady_clock::time_point now) {
    int clamped = std::clamp(seconds, 1, 15);
    config.view_state_grace_seconds = clamped;
    m_hasPendingSave = true;
    m_lastGraceChangeTime = now;
    if (m_onConfigChanged) {
        m_onConfigChanged(config);
    }
}

bool GeneralTab::update(FizzConfig& config, std::chrono::steady_clock::time_point now) {
    if (m_hasPendingSave) {
        if (now - m_lastGraceChangeTime >= m_debounceDuration) {
            m_hasPendingSave = false;
            return save(config);
        }
    }
    return false;
}

bool GeneralTab::flushPendingSave(FizzConfig& config) {
    if (m_hasPendingSave) {
        m_hasPendingSave = false;
        return save(config);
    }
    return false;
}

bool GeneralTab::hasPendingSave() const {
    return m_hasPendingSave;
}

void GeneralTab::requestReset() {
    m_resetConfirmationOpen = true;
}

void GeneralTab::cancelReset() {
    m_resetConfirmationOpen = false;
}

bool GeneralTab::confirmReset(FizzConfig& config) {
    if (!m_resetConfirmationOpen) {
        return false;
    }
    config = FizzConfig::defaults();
    m_hasPendingSave = false;
    m_resetConfirmationOpen = false;
    bool ok = save(config);
    if (m_onReset) {
        m_onReset(config);
    }
    if (m_onConfigChanged) {
        m_onConfigChanged(config);
    }
    return ok;
}

bool GeneralTab::isResetConfirmationOpen() const {
    return m_resetConfirmationOpen;
}

std::string_view GeneralTab::getResetConfirmationMessage() const {
    return kResetConfirmationText;
}

void GeneralTab::setDebounceDuration(std::chrono::milliseconds duration) {
    m_debounceDuration = duration;
}

std::chrono::milliseconds GeneralTab::getDebounceDuration() const {
    return m_debounceDuration;
}

void GeneralTab::render(FizzConfig& config, std::chrono::steady_clock::time_point now) {
    update(config, now);

    ImGui::Spacing();
    ImGui::TextColored(fizz::theme::getActiveAccent(), "General Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Auto-close on unfocus checkbox
    bool autoClose = config.autoCloseOnUnfocus;
    if (ImGui::Checkbox("Auto-close on unfocus", &autoClose)) {
        setAutoClose(config, autoClose);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Dismisses launcher window automatically when application focus is lost.");
    }

    ImGui::Spacing();

    // 2. View State Grace Period slider (1 to 15 seconds)
    ImGui::Text("View State Grace Period:");
    int grace = config.view_state_grace_seconds;
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::SliderInt("##grace_period_slider", &grace, 1, 15, "%d seconds")) {
        setGracePeriod(config, grace, now);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Duration (1–15 s) before search state resets.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Guarded "Reset to Factory Defaults" button
    ImGui::TextColored(fizz::theme::getActiveAccent(), "Factory Defaults");
    ImGui::Spacing();
    ImGui::TextDisabled("Restore all application configurations, search roots, and appearance preferences to defaults.");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));

    if (ImGui::Button("Reset to Factory Defaults")) {
        requestReset();
    }
    ImGui::PopStyleColor(3);

    if (m_resetConfirmationOpen) {
        ImGui::OpenPopup("Reset to Factory Defaults");
    }

    if (ImGui::BeginPopupModal("Reset to Factory Defaults", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", kResetConfirmationText.data());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Reset", ImVec2(120, 0))) {
            confirmReset(config);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            cancelReset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

size_t GeneralTab::getSaveCount() const {
    return m_saveCount;
}

} // namespace fizz
