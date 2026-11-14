#include "IndexingTab.h"
#include "Theme.h"
#include "FileSearch.h"

#include <cctype>
#include <algorithm>
#include <set>
#include <filesystem>

namespace fizz {

static std::string trim(std::string_view str) {
    size_t first = 0;
    while (first < str.size() && (std::isspace(static_cast<unsigned char>(str[first])) != 0)) {
        ++first;
    }
    if (first == str.size()) return "";
    size_t last = str.size() - 1;
    while (last > first && (std::isspace(static_cast<unsigned char>(str[last])) != 0)) {
        --last;
    }
    return std::string(str.substr(first, last - first + 1));
}

static std::string stripQuotes(std::string str) {
    if (str.size() >= 2) {
        if ((str.front() == '"' && str.back() == '"') ||
            (str.front() == '\'' && str.back() == '\'')) {
            return str.substr(1, str.size() - 2);
        }
    }
    return str;
}

const char* mutationResultToString(MutationResult result) {
    switch (result) {
        case MutationResult::Success: return "Success";
        case MutationResult::EmptyInput: return "Empty input";
        case MutationResult::Duplicate: return "Duplicate entry";
        case MutationResult::IndexOutOfBounds: return "Index out of bounds";
        case MutationResult::SaveFailed: return "Failed to save configuration";
    }
    return "Unknown";
}

PresetProfile getPresetProfile(PresetType type, const std::string& customHome) {
    std::string home = !customHome.empty() ? customHome : getHomeDir();

    PresetProfile profile;
    switch (type) {
        case PresetType::Minimalist:
            profile.name = "Minimalist";
            profile.searchRoots = {
                home + "/Documents",
                home + "/Downloads",
                home + "/Desktop",
                home + "/Pictures",
                home + "/Music",
                home + "/Videos",
                "/usr/share/applications",
                home + "/.local/share/applications"
            };
            profile.ignoreRules = {
                ".cache"
            };
            break;

        case PresetType::StandardDesktop:
            profile.name = "Standard Desktop";
            profile.searchRoots = {
                home,
                "/usr/share/applications"
            };
            profile.ignoreRules = {
                ".cache",
                ".git",
                ".cargo",
                ".rustup",
                ".npm",
                ".mozilla",
                "Trash"
            };
            break;

        case PresetType::Developer:
            profile.name = "Developer";
            profile.searchRoots = {
                home,
                "/usr/share/applications"
            };
            profile.ignoreRules = {
                // Python / uv
                ".venv",
                "venv",
                "__pycache__",
                ".pytest_cache",
                ".mypy_cache",
                ".uv",
                ".ruff_cache",
                // Node / pnpm
                "node_modules",
                ".pnpm-store",
                ".npm",
                ".yarn",
                // Rust
                "target",
                ".cargo",
                ".rustup",
                // Go
                "go/pkg",
                // VCS / Build
                ".git",
                "build",
                "dist",
                "bin",
                "obj",
                ".cache",
                ".idea",
                ".vscode"
            };
            break;

        case PresetType::GodMode:
            profile.name = "God Mode";
            profile.searchRoots = {
                "/"
            };
            profile.ignoreRules = {};
            break;
    }
    return profile;
}

std::string getPresetTooltipText(PresetType type, const std::string& customHome) {
    PresetProfile profile = getPresetProfile(type, customHome);
    std::string tooltip;

    tooltip += "Includes (Search Roots):\n";
    if (profile.searchRoots.empty()) {
        tooltip += "  • (none)\n";
    } else {
        for (const auto& root : profile.searchRoots) {
            tooltip += "  • " + root + "\n";
        }
    }

    tooltip += "\nExcludes (Ignore Rules):\n";
    if (profile.ignoreRules.empty()) {
        tooltip += "  • (none)\n";
    } else {
        for (const auto& rule : profile.ignoreRules) {
            tooltip += "  • " + rule + "\n";
        }
    }

    if (type == PresetType::GodMode) {
        tooltip += "\n⚠️ Warning: Full root traversal ('/') indexes the entire filesystem and places heavy demand on kernel inotify watches.";
    }

    return tooltip;
}

std::string normalizeSearchRoot(std::string_view rawPath, const std::string& customHome) {
    std::string path = trim(rawPath);
    if (path.empty()) return "";

    std::string home = !customHome.empty() ? customHome : getHomeDir();

    // Tilde expansion
    if (path == "~") {
        path = home;
    } else if (path.rfind("~/", 0) == 0) {
        path = home + path.substr(1);
    }

    // Strip redundant trailing slashes, preserving root "/"
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }

    return path;
}

std::string normalizeIgnoreRule(std::string_view rawRule) {
    std::string rule = trim(rawRule);
    if (rule.empty()) return "";
    rule = stripQuotes(rule);
    rule = trim(rule);
    return rule;
}

static std::string canonicalizeRoot(std::string_view rawPath, const std::string& customHome) {
    std::string s = normalizeSearchRoot(rawPath, customHome);
    if (s.empty()) return "";
    std::filesystem::path p(s);
    std::string norm = p.lexically_normal().string();
    while (norm.size() > 1 && norm.back() == '/') {
        norm.pop_back();
    }
    return norm;
}

static std::string canonicalizeRule(std::string_view rawRule) {
    return normalizeIgnoreRule(rawRule);
}

std::string getCustomBadgeTooltipText() {
    return "Custom Search Roots or Ignore Rules are currently active.";
}

std::optional<PresetType> detectMatchingPreset(const FizzConfig& config, const std::string& customHome) {
    std::string home = !customHome.empty() ? customHome : getHomeDir();

    // Canonicalize and deduplicate config search roots
    std::set<std::string> cfgRoots;
    for (const auto& r : config.searchRoots) {
        std::string c = canonicalizeRoot(r, home);
        if (!c.empty()) {
            cfgRoots.insert(c);
        }
    }
    // If config has duplicate or empty roots, it does not match clean preset definitions
    if (cfgRoots.size() != config.searchRoots.size()) {
        return std::nullopt;
    }

    // Canonicalize and deduplicate config ignore rules
    std::set<std::string> cfgRules;
    for (const auto& r : config.ignoreRules) {
        std::string c = canonicalizeRule(r);
        if (!c.empty()) {
            cfgRules.insert(c);
        }
    }
    if (cfgRules.size() != config.ignoreRules.size()) {
        return std::nullopt;
    }

    const PresetType candidatePresets[] = {
        PresetType::StandardDesktop,
        PresetType::Minimalist,
        PresetType::Developer,
        PresetType::GodMode
    };

    for (PresetType type : candidatePresets) {
        PresetProfile prof = getPresetProfile(type, home);

        std::set<std::string> profRoots;
        for (const auto& r : prof.searchRoots) {
            std::string c = canonicalizeRoot(r, home);
            if (!c.empty()) {
                profRoots.insert(c);
            }
        }

        std::set<std::string> profRules;
        for (const auto& r : prof.ignoreRules) {
            std::string c = canonicalizeRule(r);
            if (!c.empty()) {
                profRules.insert(c);
            }
        }

        if (cfgRoots == profRoots && cfgRules == profRules) {
            return type;
        }
    }

    return std::nullopt;
}

std::optional<PresetType> detectActivePreset(const FizzConfig& config, const std::string& customHome) {
    return detectMatchingPreset(config, customHome);
}

MutationResult addSearchRoot(FizzConfig& config, std::string_view rawPath,
                             const std::string& configPath,
                             const std::string& customHome) {
    std::string normalized = normalizeSearchRoot(rawPath, customHome);
    if (normalized.empty()) {
        return MutationResult::EmptyInput;
    }

    for (const auto& existing : config.searchRoots) {
        if (normalizeSearchRoot(existing, customHome) == normalized) {
            return MutationResult::Duplicate;
        }
    }

    config.searchRoots.push_back(normalized);

    if (!saveConfig(config, configPath)) {
        config.searchRoots.pop_back();
        return MutationResult::SaveFailed;
    }

    return MutationResult::Success;
}

MutationResult removeSearchRoot(FizzConfig& config, size_t index,
                                const std::string& configPath) {
    if (index >= config.searchRoots.size()) {
        return MutationResult::IndexOutOfBounds;
    }

    std::string removed = config.searchRoots[index];
    config.searchRoots.erase(config.searchRoots.begin() + index);

    if (!saveConfig(config, configPath)) {
        config.searchRoots.insert(config.searchRoots.begin() + index, removed);
        return MutationResult::SaveFailed;
    }

    return MutationResult::Success;
}

MutationResult addIgnoreRule(FizzConfig& config, std::string_view rawRule,
                             const std::string& configPath) {
    std::string normalized = normalizeIgnoreRule(rawRule);
    if (normalized.empty()) {
        return MutationResult::EmptyInput;
    }

    for (const auto& existing : config.ignoreRules) {
        if (normalizeIgnoreRule(existing) == normalized) {
            return MutationResult::Duplicate;
        }
    }

    config.ignoreRules.push_back(normalized);

    if (!saveConfig(config, configPath)) {
        config.ignoreRules.pop_back();
        return MutationResult::SaveFailed;
    }

    return MutationResult::Success;
}

MutationResult removeIgnoreRule(FizzConfig& config, size_t index,
                                const std::string& configPath) {
    if (index >= config.ignoreRules.size()) {
        return MutationResult::IndexOutOfBounds;
    }

    std::string removed = config.ignoreRules[index];
    config.ignoreRules.erase(config.ignoreRules.begin() + index);

    if (!saveConfig(config, configPath)) {
        config.ignoreRules.insert(config.ignoreRules.begin() + index, removed);
        return MutationResult::SaveFailed;
    }

    return MutationResult::Success;
}

void IndexingTab::render(FizzConfig& config, const std::string& configPath, const std::string& customHome) {
    std::string home = !customHome.empty() ? customHome : m_state.customHome;
    renderIndexingTab(m_state, config, configPath, home);
}

bool IndexingTab::renderTabItem(FizzConfig& config, const std::string& configPath, const std::string& customHome) {
    if (ImGui::BeginTabItem("Indexing")) {
        render(config, configPath, customHome);
        ImGui::EndTabItem();
        return true;
    }
    return false;
}

MutationResult IndexingTab::submitNewRoot(FizzConfig& config, const std::string& configPath) {
    std::string normalized = normalizeSearchRoot(m_state.newRootBuffer);
    if (normalized == "/") {
        m_state.requestApplyPreset(PresetType::GodMode);
        m_state.setStatus("Adding root ('/') triggers God Mode. Please confirm in the warning modal.", true);
        return MutationResult::Duplicate;
    }

    MutationResult res = addSearchRoot(config, m_state.newRootBuffer, configPath);
    if (res == MutationResult::Success) {
        m_state.newRootBuffer[0] = '\0';
        m_state.selectedRootIndex = static_cast<int>(config.searchRoots.size()) - 1;
        m_state.activeFocus = SectionFocus::SearchRoots;
        m_state.setStatus("Added search root: " + config.searchRoots.back(), false);
    } else if (res == MutationResult::Duplicate) {
        m_state.setStatus("Directory is already in Search Roots", true);
    } else if (res == MutationResult::EmptyInput) {
        m_state.setStatus("Please enter a non-empty directory path", true);
    } else if (res == MutationResult::SaveFailed) {
        m_state.setStatus("Failed to persist changes to configuration", true);
    }
    return res;
}

MutationResult IndexingTab::submitNewRule(FizzConfig& config, const std::string& configPath) {
    MutationResult res = addIgnoreRule(config, m_state.newRuleBuffer, configPath);
    if (res == MutationResult::Success) {
        m_state.newRuleBuffer[0] = '\0';
        m_state.selectedRuleIndex = static_cast<int>(config.ignoreRules.size()) - 1;
        m_state.activeFocus = SectionFocus::IgnoreRules;
        m_state.setStatus("Added ignore rule: " + config.ignoreRules.back(), false);
    } else if (res == MutationResult::Duplicate) {
        m_state.setStatus("Pattern is already in Ignore Rules", true);
    } else if (res == MutationResult::EmptyInput) {
        m_state.setStatus("Please enter a non-empty ignore rule", true);
    } else if (res == MutationResult::SaveFailed) {
        m_state.setStatus("Failed to persist changes to configuration", true);
    }
    return res;
}

MutationResult IndexingTab::removeSelectedRoot(FizzConfig& config, const std::string& configPath) {
    if (m_state.selectedRootIndex < 0 || m_state.selectedRootIndex >= static_cast<int>(config.searchRoots.size())) {
        m_state.setStatus("No search root selected to remove", true);
        return MutationResult::IndexOutOfBounds;
    }

    size_t idx = static_cast<size_t>(m_state.selectedRootIndex);
    std::string removedName = config.searchRoots[idx];
    MutationResult res = removeSearchRoot(config, idx, configPath);
    if (res == MutationResult::Success) {
        if (config.searchRoots.empty()) {
            m_state.selectedRootIndex = -1;
        } else if (m_state.selectedRootIndex >= static_cast<int>(config.searchRoots.size())) {
            m_state.selectedRootIndex = static_cast<int>(config.searchRoots.size()) - 1;
        }
        m_state.setStatus("Removed search root: " + removedName, false);
    } else if (res == MutationResult::SaveFailed) {
        m_state.setStatus("Failed to persist changes to configuration", true);
    }
    return res;
}

MutationResult IndexingTab::removeSelectedRule(FizzConfig& config, const std::string& configPath) {
    if (m_state.selectedRuleIndex < 0 || m_state.selectedRuleIndex >= static_cast<int>(config.ignoreRules.size())) {
        m_state.setStatus("No ignore rule selected to remove", true);
        return MutationResult::IndexOutOfBounds;
    }

    size_t idx = static_cast<size_t>(m_state.selectedRuleIndex);
    std::string removedName = config.ignoreRules[idx];
    MutationResult res = removeIgnoreRule(config, idx, configPath);
    if (res == MutationResult::Success) {
        if (config.ignoreRules.empty()) {
            m_state.selectedRuleIndex = -1;
        } else if (m_state.selectedRuleIndex >= static_cast<int>(config.ignoreRules.size())) {
            m_state.selectedRuleIndex = static_cast<int>(config.ignoreRules.size()) - 1;
        }
        m_state.setStatus("Removed ignore rule: " + removedName, false);
    } else if (res == MutationResult::SaveFailed) {
        m_state.setStatus("Failed to persist changes to configuration", true);
    }
    return res;
}

MutationResult applyPreset(FizzConfig& config, PresetType type,
                           const std::string& configPath,
                           const std::string& customHome) {
    PresetProfile profile = getPresetProfile(type, customHome);
    std::vector<std::string> prevRoots = config.searchRoots;
    std::vector<std::string> prevRules = config.ignoreRules;

    config.searchRoots = profile.searchRoots;
    config.ignoreRules = profile.ignoreRules;

    if (!saveConfig(config, configPath)) {
        config.searchRoots = prevRoots;
        config.ignoreRules = prevRules;
        return MutationResult::SaveFailed;
    }

    setCustomIgnoreRules(config.ignoreRules);
    return MutationResult::Success;
}

bool IndexingTab::confirmPresetApplication(FizzConfig& config, const std::string& configPath, const std::string& customHome) {
    if (!m_state.pendingPreset.has_value()) {
        return false;
    }
    PresetType type = *m_state.pendingPreset;
    MutationResult res = applyPreset(config, type, configPath, customHome);
    if (res == MutationResult::Success) {
        m_state.setStatus("Applied preset: " + getPresetProfile(type, customHome).name, false);
        m_state.selectedRootIndex = config.searchRoots.empty() ? -1 : 0;
        m_state.selectedRuleIndex = config.ignoreRules.empty() ? -1 : 0;
    } else {
        m_state.setStatus("Failed to persist preset changes to configuration", true);
    }
    m_state.cancelPresetConfirmation();
    return (res == MutationResult::Success);
}

std::string IndexingTab::getPresetConfirmationMessage(const FizzConfig& config) const {
    std::string name = m_state.pendingPreset.has_value() ?
        getPresetProfile(*m_state.pendingPreset).name : "Preset";
    return "Applying the '" + name + "' preset will overwrite your current configuration (" +
           std::to_string(config.searchRoots.size()) + " Search Roots and " +
           std::to_string(config.ignoreRules.size()) + " Ignore Rules).\n\n" +
           "Do you want to proceed?";
}

std::string IndexingTab::getGodModeWarningMessage(const FizzConfig& config) const {
    return "Warning: God Mode indexes the entire root filesystem ('/'). This can cause high CPU and RAM usage.\n\n"
           "Note: kernel inotify watch limits may be reached, leading to inotify watch exhaustion.\n"
           "You may need to tune the limit using 'fs.inotify.max_user_watches' (e.g. sysctl fs.inotify.max_user_watches=524288).\n\n"
           "This will overwrite your current configuration (" +
           std::to_string(config.searchRoots.size()) + " Search Roots and " +
           std::to_string(config.ignoreRules.size()) + " Ignore Rules).\n\n"
           "Are you sure you want to enable God Mode?";
}

void renderIndexingTab(IndexingTabState& state, FizzConfig& config, const std::string& configPath, const std::string& customHome) {
    std::string home = !customHome.empty() ? customHome : state.customHome;
    state.activePreset = detectMatchingPreset(config, home);
    state.isCustomActive = !state.activePreset.has_value();

    ImGuiIO& io = ImGui::GetIO();
    float availW = ImGui::GetContentRegionAvail().x;
    float availH = ImGui::GetContentRegionAvail().y;

    // Status / feedback line
    if (!state.statusMessage.empty()) {
        ImVec4 msgColor = state.isErrorMessage ? theme::Palette::kAmber : theme::Palette::kTagGreen;
        ImGui::TextColored(msgColor, "%s", state.statusMessage.c_str());
    } else {
        ImGui::TextDisabled("Select an entry to delete with Delete/Backspace, or click ✕.");
    }
    ImGui::Spacing();

    // Key handling for deletion / arrow keys when not typing into input fields
    int toRemoveRoot = -1;
    int toRemoveRule = -1;

    if (!io.WantTextInput) {
        bool deletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace);
        if (state.activeFocus == SectionFocus::SearchRoots) {
            if (deletePressed && state.selectedRootIndex >= 0 && state.selectedRootIndex < static_cast<int>(config.searchRoots.size())) {
                toRemoveRoot = state.selectedRootIndex;
            } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && state.selectedRootIndex + 1 < static_cast<int>(config.searchRoots.size())) {
                ++state.selectedRootIndex;
            } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && state.selectedRootIndex > 0) {
                --state.selectedRootIndex;
            }
        } else if (state.activeFocus == SectionFocus::IgnoreRules) {
            if (deletePressed && state.selectedRuleIndex >= 0 && state.selectedRuleIndex < static_cast<int>(config.ignoreRules.size())) {
                toRemoveRule = state.selectedRuleIndex;
            } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && state.selectedRuleIndex + 1 < static_cast<int>(config.ignoreRules.size())) {
                ++state.selectedRuleIndex;
            } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && state.selectedRuleIndex > 0) {
                --state.selectedRuleIndex;
            }
        }
    }

    // Allocate list heights dynamically
    // Fixed UI elements take ~190px total with Presets toolbar
    float sectionListH = std::max(70.0f, (availH - 200.0f) * 0.5f);

    // ==========================================
    // PRESET PROFILES BAR
    // ==========================================
    ImGui::TextColored(theme::Palette::kAccent, "Presets:");

    auto renderPresetButton = [&](const char* label, PresetType type, int index) {
        ImGui::SameLine();
        bool isActive = (state.activePreset.has_value() && *state.activePreset == type);
        if (isActive) {
            ImVec4 accent = theme::getActiveAccent();
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::withAlpha(accent, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::withAlpha(accent, 0.70f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        if (ImGui::Button(label)) {
            state.requestApplyPreset(type);
        }
        state.presetButtonMin[index] = ImGui::GetItemRectMin();
        state.presetButtonMax[index] = ImGui::GetItemRectMax();
        ImGui::SetItemTooltip("%s", getPresetTooltipText(type, home).c_str());
        if (isActive) {
            ImGui::PopStyleColor(4);
        }
    };

    renderPresetButton("Minimalist##btn_preset_minimalist", PresetType::Minimalist, 0);
    renderPresetButton("Standard Desktop##btn_preset_standard", PresetType::StandardDesktop, 1);
    renderPresetButton("Developer##btn_preset_developer", PresetType::Developer, 2);
    renderPresetButton("⚠️ God Mode##btn_preset_god_mode", PresetType::GodMode, 3);

    if (state.isCustomActive) {
        ImGui::SameLine();
        ImVec4 accent = theme::getActiveAccent();
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::withAlpha(accent, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::withAlpha(accent, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        ImGui::Button("[Custom]##btn_preset_custom");
        state.customBadgeMin = ImGui::GetItemRectMin();
        state.customBadgeMax = ImGui::GetItemRectMax();
        ImGui::SetItemTooltip("%s", getCustomBadgeTooltipText().c_str());

        ImGui::PopStyleColor(4);
    } else {
        state.customBadgeMin = ImVec2(0.0f, 0.0f);
        state.customBadgeMax = ImVec2(0.0f, 0.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==========================================
    // SECTION 1: SEARCH ROOTS
    // ==========================================
    ImGui::TextColored(theme::Palette::kAccent, "Search Roots (%zu)", config.searchRoots.size());
    ImGui::SameLine();
    ImGui::TextDisabled("— directories scanned for files and applications");

    // Input line
    float addBtnW = 85.0f;
    float inputW = std::max(100.0f, availW - addBtnW - ImGui::GetStyle().ItemSpacing.x);
    ImGui::SetNextItemWidth(inputW);
    bool rootEnter = ImGui::InputTextWithHint("##new_root_input", "Enter directory path (e.g. ~/Projects)...",
                                              state.newRootBuffer, sizeof(state.newRootBuffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool rootAddClicked = ImGui::Button("Add Root##btn_add_root", ImVec2(addBtnW, 0));
    if (rootEnter || rootAddClicked) {
        std::string normalized = normalizeSearchRoot(state.newRootBuffer);
        if (normalized == "/") {
            state.requestApplyPreset(PresetType::GodMode);
            state.setStatus("Adding root ('/') triggers God Mode. Please confirm in the warning modal.", true);
        } else {
            MutationResult res = addSearchRoot(config, state.newRootBuffer, configPath);
            if (res == MutationResult::Success) {
                state.newRootBuffer[0] = '\0';
                state.selectedRootIndex = static_cast<int>(config.searchRoots.size()) - 1;
                state.activeFocus = SectionFocus::SearchRoots;
                state.setStatus("Added search root: " + config.searchRoots.back(), false);
            } else if (res == MutationResult::Duplicate) {
                state.setStatus("Directory is already in Search Roots", true);
            } else if (res == MutationResult::EmptyInput) {
                state.setStatus("Please enter a non-empty directory path", true);
            } else if (res == MutationResult::SaveFailed) {
                state.setStatus("Failed to persist changes to configuration", true);
            }
        }
    }

    // Scrollable search roots list
    ImGui::BeginChild("search_roots_scroll", ImVec2(0, sectionListH), ImGuiChildFlags_Borders);
    float remBtnW = 26.0f;
    for (size_t i = 0; i < config.searchRoots.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        bool isSelected = (state.selectedRootIndex == static_cast<int>(i) && state.activeFocus == SectionFocus::SearchRoots);
        float rowAvailW = ImGui::GetContentRegionAvail().x;
        float itemW = std::max(50.0f, rowAvailW - remBtnW - ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Selectable(config.searchRoots[i].c_str(), isSelected, 0, ImVec2(itemW, 0))) {
            state.selectedRootIndex = static_cast<int>(i);
            state.activeFocus = SectionFocus::SearchRoots;
            state.clearStatus();
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.20f, 0.20f, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::Palette::kDim);
        if (ImGui::Button("✕##del_root", ImVec2(remBtnW, 0))) {
            toRemoveRoot = static_cast<int>(i);
        }
        ImGui::PopStyleColor(4);

        ImGui::PopID();
    }
    if (config.searchRoots.empty()) {
        ImGui::TextDisabled("No search roots configured.");
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==========================================
    // SECTION 2: IGNORE RULES
    // ==========================================
    ImGui::TextColored(theme::Palette::kAccent, "Ignore Rules (%zu)", config.ignoreRules.size());
    ImGui::SameLine();
    ImGui::TextDisabled("— patterns and folders excluded from indexing");

    // Input line
    ImGui::SetNextItemWidth(inputW);
    bool ruleEnter = ImGui::InputTextWithHint("##new_rule_input", "Enter ignore rule (e.g. node_modules, *.log)...",
                                              state.newRuleBuffer, sizeof(state.newRuleBuffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool ruleAddClicked = ImGui::Button("Add Rule##btn_add_rule", ImVec2(addBtnW, 0));
    if (ruleEnter || ruleAddClicked) {
        MutationResult res = addIgnoreRule(config, state.newRuleBuffer, configPath);
        if (res == MutationResult::Success) {
            state.newRuleBuffer[0] = '\0';
            state.selectedRuleIndex = static_cast<int>(config.ignoreRules.size()) - 1;
            state.activeFocus = SectionFocus::IgnoreRules;
            state.setStatus("Added ignore rule: " + config.ignoreRules.back(), false);
        } else if (res == MutationResult::Duplicate) {
            state.setStatus("Pattern is already in Ignore Rules", true);
        } else if (res == MutationResult::EmptyInput) {
            state.setStatus("Please enter a non-empty ignore rule", true);
        } else if (res == MutationResult::SaveFailed) {
            state.setStatus("Failed to persist changes to configuration", true);
        }
    }

    // Scrollable ignore rules list
    ImGui::BeginChild("ignore_rules_scroll", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < config.ignoreRules.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        bool isSelected = (state.selectedRuleIndex == static_cast<int>(i) && state.activeFocus == SectionFocus::IgnoreRules);
        float rowAvailW = ImGui::GetContentRegionAvail().x;
        float itemW = std::max(50.0f, rowAvailW - remBtnW - ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Selectable(config.ignoreRules[i].c_str(), isSelected, 0, ImVec2(itemW, 0))) {
            state.selectedRuleIndex = static_cast<int>(i);
            state.activeFocus = SectionFocus::IgnoreRules;
            state.clearStatus();
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.20f, 0.20f, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::Palette::kDim);
        if (ImGui::Button("✕##del_rule", ImVec2(remBtnW, 0))) {
            toRemoveRule = static_cast<int>(i);
        }
        ImGui::PopStyleColor(4);

        ImGui::PopID();
    }
    if (config.ignoreRules.empty()) {
        ImGui::TextDisabled("No ignore rules configured.");
    }
    ImGui::EndChild();

    // Execute any pending removals
    if (toRemoveRoot >= 0 && toRemoveRoot < static_cast<int>(config.searchRoots.size())) {
        std::string removed = config.searchRoots[toRemoveRoot];
        MutationResult res = removeSearchRoot(config, static_cast<size_t>(toRemoveRoot), configPath);
        if (res == MutationResult::Success) {
            if (config.searchRoots.empty()) {
                state.selectedRootIndex = -1;
            } else if (state.selectedRootIndex >= static_cast<int>(config.searchRoots.size())) {
                state.selectedRootIndex = static_cast<int>(config.searchRoots.size()) - 1;
            }
            state.setStatus("Removed search root: " + removed, false);
        } else {
            state.setStatus("Failed to remove search root", true);
        }
    }

    if (toRemoveRule >= 0 && toRemoveRule < static_cast<int>(config.ignoreRules.size())) {
        std::string removed = config.ignoreRules[toRemoveRule];
        MutationResult res = removeIgnoreRule(config, static_cast<size_t>(toRemoveRule), configPath);
        if (res == MutationResult::Success) {
            if (config.ignoreRules.empty()) {
                state.selectedRuleIndex = -1;
            } else if (state.selectedRuleIndex >= static_cast<int>(config.ignoreRules.size())) {
                state.selectedRuleIndex = static_cast<int>(config.ignoreRules.size()) - 1;
            }
            state.setStatus("Removed ignore rule: " + removed, false);
        } else {
            state.setStatus("Failed to remove ignore rule", true);
        }
    }

    // ==========================================
    // PRESET CONFIRMATION MODALS
    // ==========================================
    if (state.showPresetConfirmationModal) {
        ImGui::OpenPopup("Apply Preset Confirmation##preset_modal");
    }

    if (ImGui::BeginPopupModal("Apply Preset Confirmation##preset_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string presetName = state.pendingPreset.has_value() ?
            getPresetProfile(state.pendingPreset.value()).name : "Preset";
        ImGui::Text("Apply '%s' Preset?", presetName.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("Applying this preset will overwrite your current configuration (%zu Search Roots and %zu Ignore Rules).",
                           config.searchRoots.size(), config.ignoreRules.size());
        ImGui::TextWrapped("Do you want to proceed?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Apply##btn_confirm_preset", ImVec2(120, 0))) {
            if (state.pendingPreset.has_value()) {
                PresetType type = state.pendingPreset.value();
                MutationResult res = applyPreset(config, type, configPath);
                if (res == MutationResult::Success) {
                    state.setStatus("Applied preset: " + getPresetProfile(type).name, false);
                    state.selectedRootIndex = config.searchRoots.empty() ? -1 : 0;
                    state.selectedRuleIndex = config.ignoreRules.empty() ? -1 : 0;
                } else {
                    state.setStatus("Failed to persist preset changes to configuration", true);
                }
            }
            state.cancelPresetConfirmation();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel##btn_cancel_preset", ImVec2(120, 0))) {
            state.cancelPresetConfirmation();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Dedicated God Mode Warning Modal
    if (state.showGodModeWarningModal) {
        ImGui::OpenPopup("Warning: God Mode##god_mode_modal");
    }

    if (ImGui::BeginPopupModal("Warning: God Mode##god_mode_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "⚠️ WARNING: High Resource Usage");
        ImGui::Spacing();
        ImGui::TextWrapped("God Mode indexes the entire root filesystem ('/'). This can cause high CPU and RAM usage.");
        ImGui::Spacing();
        ImGui::TextWrapped("Note: kernel inotify watch limits may be reached, leading to inotify watch exhaustion.");
        ImGui::TextWrapped("You may need to tune the limit using 'fs.inotify.max_user_watches' (e.g. sysctl fs.inotify.max_user_watches=524288).");
        ImGui::Spacing();
        ImGui::TextWrapped("This will overwrite your current configuration (%zu Search Roots and %zu Ignore Rules).",
                           config.searchRoots.size(), config.ignoreRules.size());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Enable God Mode##btn_confirm_god_mode", ImVec2(150, 0))) {
            MutationResult res = applyPreset(config, PresetType::GodMode, configPath);
            if (res == MutationResult::Success) {
                state.setStatus("Applied preset: God Mode", false);
                state.selectedRootIndex = config.searchRoots.empty() ? -1 : 0;
                state.selectedRuleIndex = config.ignoreRules.empty() ? -1 : 0;
            } else {
                state.setStatus("Failed to persist preset changes to configuration", true);
            }
            state.cancelPresetConfirmation();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel##btn_cancel_god_mode", ImVec2(120, 0))) {
            state.cancelPresetConfirmation();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace fizz
