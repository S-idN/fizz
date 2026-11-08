#pragma once

#include "Config.h"
#include "imgui.h"
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace fizz {

// Normalizes path for search roots:
// 1. Trims leading and trailing whitespace.
// 2. Expands leading '~' to home directory (uses customHome if provided, else getHomeDir()).
// 3. Strips redundant trailing slashes while preserving root "/".
std::string normalizeSearchRoot(std::string_view path, const std::string& customHome = "");

// Normalizes rule for ignore patterns:
// 1. Trims leading and trailing whitespace.
// 2. Strips surrounding matching quotes (' or ") if present.
std::string normalizeIgnoreRule(std::string_view rule);

enum class PresetType {
    Minimalist,
    StandardDesktop,
    Developer,
    GodMode
};

struct PresetProfile {
    std::string name;
    std::vector<std::string> searchRoots;
    std::vector<std::string> ignoreRules;
};

PresetProfile getPresetProfile(PresetType type, const std::string& customHome = "");

enum class MutationResult {
    Success,
    EmptyInput,
    Duplicate,
    IndexOutOfBounds,
    SaveFailed
};

const char* mutationResultToString(MutationResult result);

MutationResult addSearchRoot(FizzConfig& config, std::string_view rawPath,
                             const std::string& configPath = "",
                             const std::string& customHome = "");

MutationResult removeSearchRoot(FizzConfig& config, size_t index,
                                const std::string& configPath = "");

MutationResult addIgnoreRule(FizzConfig& config, std::string_view rawRule,
                             const std::string& configPath = "");

MutationResult removeIgnoreRule(FizzConfig& config, size_t index,
                                const std::string& configPath = "");

MutationResult applyPreset(FizzConfig& config, PresetType type,
                           const std::string& configPath = "",
                           const std::string& customHome = "");

enum class SectionFocus {
    None,
    SearchRoots,
    IgnoreRules
};

struct IndexingTabState {
    char newRootBuffer[1024] = {0};
    char newRuleBuffer[1024] = {0};

    int selectedRootIndex = -1;
    int selectedRuleIndex = -1;

    SectionFocus activeFocus = SectionFocus::None;

    std::string statusMessage = "";
    bool isErrorMessage = false;

    std::optional<PresetType> pendingPreset = std::nullopt;
    bool showPresetConfirmationModal = false;
    bool showGodModeWarningModal = false;

    void requestApplyPreset(PresetType type) {
        pendingPreset = type;
        if (type == PresetType::GodMode) {
            showGodModeWarningModal = true;
            showPresetConfirmationModal = false;
        } else {
            showPresetConfirmationModal = true;
            showGodModeWarningModal = false;
        }
    }

    void cancelPresetConfirmation() {
        pendingPreset = std::nullopt;
        showPresetConfirmationModal = false;
        showGodModeWarningModal = false;
    }

    void setStatus(std::string_view msg, bool isError = false) {
        statusMessage = msg;
        isErrorMessage = isError;
    }

    void clearStatus() {
        statusMessage.clear();
        isErrorMessage = false;
    }
};

class IndexingTab {
public:
    IndexingTab() = default;

    IndexingTabState& getState() { return m_state; }
    const IndexingTabState& getState() const { return m_state; }

    void render(FizzConfig& config, const std::string& configPath = "");
    bool renderTabItem(FizzConfig& config, const std::string& configPath = "");

    MutationResult submitNewRoot(FizzConfig& config, const std::string& configPath = "");
    MutationResult submitNewRule(FizzConfig& config, const std::string& configPath = "");
    MutationResult removeSelectedRoot(FizzConfig& config, const std::string& configPath = "");
    MutationResult removeSelectedRule(FizzConfig& config, const std::string& configPath = "");

    // Preset profile operations
    void requestApplyPreset(PresetType type) { m_state.requestApplyPreset(type); }
    void cancelPresetConfirmation() { m_state.cancelPresetConfirmation(); }
    bool confirmPresetApplication(FizzConfig& config, const std::string& configPath = "", const std::string& customHome = "");
    bool isPresetConfirmationOpen() const { return m_state.showPresetConfirmationModal; }
    bool isGodModeWarningOpen() const { return m_state.showGodModeWarningModal; }
    std::optional<PresetType> getPendingPreset() const { return m_state.pendingPreset; }
    std::string getPresetConfirmationMessage(const FizzConfig& config) const;
    std::string getGodModeWarningMessage(const FizzConfig& config) const;

private:
    IndexingTabState m_state;
};

void renderIndexingTab(IndexingTabState& state, FizzConfig& config, const std::string& configPath = "");

} // namespace fizz
