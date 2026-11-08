//Temp frontend, I'll make a proper one once I learn how to use C++ frontend frameworks properly T-T 
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <cctype>
#include <string>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "services/QueryOps.h"
#include "services/LoadFile.h"
#include "services/FileSearch.h"
#include "services/AppLauncher.h"
#include "services/Config.h"
#include "services/FocusTracker.h"
#include "services/FontDiscovery.h"
#include "services/Theme.h"
#include "services/SearchUi.h"
#include "services/ViewMode.h"
#include "services/CacheTab.h"
#include "services/ViewState.h"
#include "services/BuiltinActions.h"
#include "services/AppearanceTab.h"
#include "services/IndexingTab.h"
#include "services/GeneralTab.h"

constexpr int   kWidth   = 750;
constexpr int   kHeight  = 480;
constexpr int   kMaxHits = 50;
constexpr float kRowH    = fizz::theme::Metrics::kRowHeight;
constexpr float kFontPx  = kBaseFontSize;

static void glfwError(int code, const char* msg) {
    fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}

static bool g_IndexReloadedOnFocus = false;

static void windowFocusCallback(GLFWwindow* window, int focused) {
    if (focused != 0) {
        if (reloadIndexIfModified()) {
            g_IndexReloadedOnFocus = true;
        }
    }
    auto* tracker = static_cast<FocusTracker*>(glfwGetWindowUserPointer(window));
    if (tracker && tracker->handleFocusChange(focused != 0)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static bool drawRow(std::string_view path, ExecState state, bool selected) {
    bool clicked = ImGui::Selectable("##row", selected,
                       ImGuiSelectableFlags_AllowDoubleClick, {0, kRowH});

    std::string_view dir;
    std::string_view name;
    if (fizz::isBuiltinActionPath(path)) {
        name = fizz::getBuiltinActionTitle(path);
        dir = path;
    } else {
        size_t slash = path.rfind('/');
        if (slash == std::string_view::npos) {
            dir = "";
            name = path;
        } else if (slash == 0) {
            dir = "/";
            name = path.substr(1);
        } else {
            dir = path.substr(0, slash);
            name = path.substr(slash + 1);
        }
    }

    ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    fizz::ui::CategoryBadge badge = fizz::ui::getCategoryBadge(path, state);
    auto layout = fizz::ui::calculateRowLayout(min, max, name, dir, badge, ImGui::GetFontSize());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) {
        auto geom = fizz::theme::calculateIndicatorGeometry(min, max);
        dl->AddRectFilled(geom.min, geom.max, ImGui::GetColorU32(fizz::theme::getActiveAccent()), geom.rounding);
    }

    fizz::ui::renderRowContent(dl, layout, name, badge);
    return clicked;
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    FizzConfig config = loadConfig();
    if (!config.binary_cache_path.empty()) {
        setBinaryCachePath(config.binary_cache_path);
    }
    FocusTracker focusTracker(config.autoCloseOnUnfocus);
    loadIndexToMemory();

    glfwSetErrorCallback(glfwError);
    if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND))
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);   
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "fizz");

    GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, "Fizz", nullptr, nullptr);
    if (!window) { fprintf(stderr, "window creation failed\n"); glfwTerminate(); return 1; }

    glfwSetWindowSizeLimits(window, kWidth, kHeight, kWidth, kHeight);
    glfwSetWindowUserPointer(window, &focusTracker);
    glfwSetWindowFocusCallback(window, windowFocusCallback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                  
    std::string fontPath = resolveSystemFont();
    if (fontPath.empty() || !io.Fonts->AddFontFromFileTTF(fontPath.c_str(), kFontPx, nullptr, fizz::ui::getUiGlyphRanges())) {
        io.Fonts->AddFontDefault();
    }
    ImGui::StyleColorsDark();
    fizz::theme::applyTheme(ImGui::GetStyle(), config.theme, config.accent);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    char buf[256] = "";
    std::string lastQuery;
    std::vector<SearchHit> results;
    results.reserve(kMaxHits);
    int selected = 0;
    bool scrollToSel = false;
    std::string warningMsg;
    auto lastInputTime = std::chrono::steady_clock::now();
    constexpr auto kIdleThreshold = std::chrono::milliseconds(500);

    ViewMode initialMode = loadViewState(config.view_state_grace_seconds);
    ViewModeStateMachine stateMachine(initialMode);
    stateMachine.setSearchStatePointers(&lastQuery, &results, buf);
    stateMachine.setOnCancelSearch([&]() {
        selected = 0;
        warningMsg.clear();
    });

    fizz::CacheTab cacheTab(config);
    fizz::IndexingTab indexingTab;
    fizz::GeneralTab generalTab;
    generalTab.setOnConfigChanged([&](const FizzConfig& updatedCfg) {
        focusTracker.setAutoCloseOnUnfocus(updatedCfg.autoCloseOnUnfocus);
    });
    generalTab.setOnReset([&](const FizzConfig& defaultCfg) {
        focusTracker.setAutoCloseOnUnfocus(defaultCfg.autoCloseOnUnfocus);
        fizz::theme::applyTheme(ImGui::GetStyle());
    });
    stateMachine.setOnModeChange([&](ViewMode oldMode, ViewMode newMode) {
        if (oldMode == ViewMode::Settings) {
            generalTab.flushPendingSave(config);
        }
    });

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEventsTimeout(0.5);

        if (g_IndexReloadedOnFocus) {
            g_IndexReloadedOnFocus = false;
            if (!lastQuery.empty() && stateMachine.getCurrentMode() == ViewMode::Search) {
                searchPaths(lastQuery, results, kMaxHits);
                fizz::rankHitsWithBuiltinActionsInPlace(lastQuery, results, kMaxHits);
                selected = 0;
                warningMsg.clear();
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("fizz", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        // Global Escape handling: Settings/About -> Search without closing; Search -> Close application
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (stateMachine.handleEscape()) {
                glfwSetWindowShouldClose(window, 1);
            }
        }

        switch (stateMachine.getCurrentMode()) {
        case ViewMode::Search: {
            // Search pill: integrated rounded pill with prompt glyph and placeholder hint
            if (fizz::ui::renderSearchPill(buf, sizeof(buf), fizz::ui::kDefaultPlaceholderText)) {
                std::string_view q(buf);
                if (q != lastQuery) {
                    auto now = std::chrono::steady_clock::now();
                    bool wasIdle = lastQuery.empty() || (now - lastInputTime >= kIdleThreshold);
                    if (wasIdle) {
                        reloadIndexIfModified();
                    }
                    lastInputTime = now;
                    searchPaths(q, results, kMaxHits);
                    fizz::rankHitsWithBuiltinActionsInPlace(q, results, kMaxHits);
                    lastQuery.assign(q);
                    selected = 0;
                    warningMsg.clear();
                }
            }

            const int n = (int)results.size();
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected + 1 < n) { ++selected; scrollToSel = true; warningMsg.clear(); }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)   && selected > 0)     { --selected; scrollToSel = true; warningMsg.clear(); }

            // Shortcuts to open Settings / About
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Comma)) {
                stateMachine.transitionTo(ViewMode::Settings);
            } else if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                stateMachine.transitionTo(ViewMode::About);
            }

            int chosen = -1;
            bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
            if (enterPressed && n > 0) {
                if (fizz::isBuiltinActionPath(results[selected].path)) {
                    fizz::executeBuiltinAction(results[selected].path, stateMachine);
                } else {
                    bool isUntrusted = isDesktopEntry(results[selected].path) && !isDesktopEntryTrusted(results[selected].path);
                    if (isUntrusted && !io.KeyCtrl) {
                        warningMsg = "⚠️ Untrusted Desktop Entry. Press Ctrl+Enter to launch anyway";
                    } else {
                        chosen = selected;
                    }
                }
            }

            auto footerLayout = fizz::ui::calculateFooterLayout(fizz::ui::Metrics::kFooterHeight, ImGui::GetStyle().ItemSpacing.y);
            ImGui::BeginChild("results", {0, footerLayout.reservedChildHeight}, ImGuiChildFlags_Borders);
            for (int i = 0; i < n; ++i) {
                ImGui::PushID(i);
                if (drawRow(results[i].path, results[i].execState, i == selected)) {
                    selected = i;
                    if (fizz::isBuiltinActionPath(results[i].path)) {
                        fizz::executeBuiltinAction(results[i].path, stateMachine);
                    } else if (ImGui::IsMouseDoubleClicked(0)) {
                        bool isUntrusted = isDesktopEntry(results[i].path) && !isDesktopEntryTrusted(results[i].path);
                        if (isUntrusted && !io.KeyCtrl) {
                            warningMsg = "⚠️ Untrusted Desktop Entry. Press Ctrl+Enter to launch anyway";
                        } else {
                            chosen = i;
                        }
                    }
                }
                if (i == selected && scrollToSel) { ImGui::SetScrollHereY(); scrollToSel = false; }
                ImGui::PopID();
            }
            if (n == 0 && !lastQuery.empty()) ImGui::TextDisabled("no matches");
            ImGui::EndChild();

            // Pinned status footer: search hit count and keyboard navigation shortcuts
            fizz::ui::renderStatusFooter(n, warningMsg);

            if (chosen >= 0) {
                launchTarget(results[chosen].path, results[chosen].execState, io.KeyShift);
                glfwSetWindowShouldClose(window, 1);
            }
            break;
        }

        case ViewMode::Settings: {
            if (fizz::ui::renderViewHeader("Settings")) {
                generalTab.flushPendingSave(config);
                stateMachine.transitionTo(ViewMode::Search);
            }

            ImGui::BeginChild("settings_container", {0, 0}, false);
            if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("Appearance")) {
                    fizz::ui::renderAppearanceTab(config);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Indexing")) {
                    indexingTab.render(config);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Cache")) {
                    cacheTab.render(config);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("General")) {
                    generalTab.render(config);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Press Escape or click '← Back to Search' to return.");
            ImGui::EndChild();
            break;
        }

        case ViewMode::About: {
            if (fizz::ui::renderViewHeader("About")) {
                stateMachine.transitionTo(ViewMode::Search);
            }

            ImGui::BeginChild("about_container", {0, 0}, false);
            ImGui::Spacing();
            ImGui::TextColored(fizz::theme::getActiveAccent(), "Fizz Application Launcher");
            ImGui::TextDisabled("v1.0.0");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("A fast, keyboard-driven desktop fuzzy file and application launcher for Linux.");
            ImGui::Spacing();
            ImGui::Text("Keyboard Navigation:");
            ImGui::BulletText("↵ (Enter)        : Open file or launch application");
            ImGui::BulletText("⇧↵ (Shift+Enter) : Reveal directory in file manager");
            ImGui::BulletText("Esc              : Return to Search / Dismiss launcher");
            ImGui::BulletText("Ctrl+,           : Open Settings view");
            ImGui::BulletText("F1               : Open About view");
            ImGui::Spacing();
            ImGui::TextDisabled("Press Escape or click '← Back to Search' to return.");
            ImGui::EndChild();
            break;
        }
        }

        ImGui::End();

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);   
        ImGui::Render();
        glViewport(0, 0, fbw, fbh);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    generalTab.flushPendingSave(config);

    ViewMode exitMode = stateMachine.getCurrentMode();
    if (exitMode == ViewMode::Settings || exitMode == ViewMode::About) {
        saveViewState(exitMode);
    } else {
        clearViewState();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
