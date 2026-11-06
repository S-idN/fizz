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

constexpr int   kWidth   = 750;
constexpr int   kHeight  = 480;
constexpr int   kMaxHits = 50;
constexpr float kRowH    = fizz::theme::Metrics::kRowHeight;
constexpr float kFontPx  = kBaseFontSize;

static void glfwError(int code, const char* msg) {
    fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}

static void windowFocusCallback(GLFWwindow* window, int focused) {
    auto* tracker = static_cast<FocusTracker*>(glfwGetWindowUserPointer(window));
    if (tracker && tracker->handleFocusChange(focused != 0)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static bool drawRow(std::string_view path, ExecState state, bool selected) {
    bool clicked = ImGui::Selectable("##row", selected,
                       ImGuiSelectableFlags_AllowDoubleClick, {0, kRowH});

    size_t slash = path.rfind('/');
    std::string_view dir;
    std::string_view name;
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

    ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    fizz::ui::CategoryBadge badge = fizz::ui::getCategoryBadge(path, state);
    auto layout = fizz::ui::calculateRowLayout(min, max, name, dir, badge, ImGui::GetFontSize());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) {
        auto geom = fizz::theme::calculateIndicatorGeometry(min, max);
        dl->AddRectFilled(geom.min, geom.max, ImGui::GetColorU32(fizz::theme::Palette::kAccent), geom.rounding);
    }

    fizz::ui::renderRowContent(dl, layout, name, badge);
    return clicked;
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    FizzConfig config = loadConfig();
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
    fizz::theme::applyTheme(ImGui::GetStyle());
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    char buf[256] = "";
    std::string lastQuery;
    std::vector<SearchHit> results;
    results.reserve(kMaxHits);
    int selected = 0;
    bool scrollToSel = false;
    std::string warningMsg;

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEventsTimeout(0.5);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("fizz", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        // Search pill: integrated rounded pill with prompt glyph and placeholder hint
        if (fizz::ui::renderSearchPill(buf, sizeof(buf), fizz::ui::kDefaultPlaceholderText)) {
            std::string_view q(buf);
            if (q != lastQuery) {
                searchPaths(q, results, kMaxHits);
                lastQuery.assign(q);
                selected = 0;
                warningMsg.clear();
            }
        }

        const int n = (int)results.size();
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected + 1 < n) { ++selected; scrollToSel = true; warningMsg.clear(); }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)   && selected > 0)     { --selected; scrollToSel = true; warningMsg.clear(); }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) glfwSetWindowShouldClose(window, 1);

        int chosen = -1;
        bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
        if (enterPressed && n > 0) {
            bool isUntrusted = isDesktopEntry(results[selected].path) && !isDesktopEntryTrusted(results[selected].path);
            if (isUntrusted && !io.KeyCtrl) {
                warningMsg = "⚠️ Untrusted Desktop Entry. Press Ctrl+Enter to launch anyway";
            } else {
                chosen = selected;
            }
        }

        auto footerLayout = fizz::ui::calculateFooterLayout(fizz::ui::Metrics::kFooterHeight, ImGui::GetStyle().ItemSpacing.y);
        ImGui::BeginChild("results", {0, footerLayout.reservedChildHeight}, ImGuiChildFlags_Borders);
        for (int i = 0; i < n; ++i) {
            ImGui::PushID(i);
            if (drawRow(results[i].path, results[i].execState, i == selected)) {
                selected = i;
                if (ImGui::IsMouseDoubleClicked(0)) {
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
        ImGui::End();

        if (chosen >= 0) {
            launchTarget(results[chosen].path, results[chosen].execState, io.KeyShift);
            glfwSetWindowShouldClose(window, 1);
        }

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);   
        ImGui::Render();
        glViewport(0, 0, fbw, fbh);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
