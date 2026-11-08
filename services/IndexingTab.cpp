#include "IndexingTab.h"
#include "Theme.h"

#include <cctype>
#include <algorithm>

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

void IndexingTab::render(FizzConfig& config, const std::string& configPath) {
    renderIndexingTab(m_state, config, configPath);
}

bool IndexingTab::renderTabItem(FizzConfig& config, const std::string& configPath) {
    if (ImGui::BeginTabItem("Indexing")) {
        render(config, configPath);
        ImGui::EndTabItem();
        return true;
    }
    return false;
}

MutationResult IndexingTab::submitNewRoot(FizzConfig& config, const std::string& configPath) {
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
        m_state.setStatus("Please enter a non-empty exclusion pattern", true);
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

void renderIndexingTab(IndexingTabState& state, FizzConfig& config, const std::string& configPath) {
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
    // Fixed UI elements take ~140px total
    float sectionListH = std::max(70.0f, (availH - 160.0f) * 0.5f);

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
    bool ruleEnter = ImGui::InputTextWithHint("##new_rule_input", "Enter exclusion pattern (e.g. node_modules, *.log)...",
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
            state.setStatus("Please enter a non-empty exclusion pattern", true);
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
}

} // namespace fizz
