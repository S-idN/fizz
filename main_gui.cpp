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

struct Tag { const char* label; ImVec4 color; };

static const ImVec4 kAccent = {0.20f,  0.83f,  0.75f,  1.00f};

static Tag classify(std::string_view path, ExecState state) {
    std::string_view cat = classifyPath(path, state);
    if (cat.empty()) return {nullptr, {}};

    static const ImVec4 kOrange = {0.95f, 0.65f, 0.25f, 1.f};
    static const ImVec4 kBlue   = {0.45f, 0.65f, 0.95f, 1.f};
    static const ImVec4 kPurple = {0.70f, 0.55f, 0.95f, 1.f};
    static const ImVec4 kPink   = {0.95f, 0.50f, 0.65f, 1.f};
    static const ImVec4 kGreen  = {0.55f, 0.85f, 0.45f, 1.f};
    static const ImVec4 kGrey   = {0.60f, 0.65f, 0.67f, 1.f};

    if (cat == "app")     return {"app",     kAccent};
    if (cat == "exec")    return {"exec",    kOrange};
    if (cat == "text")    return {"text",    kBlue};
    if (cat == "code")    return {"code",    kPurple};
    if (cat == "image")   return {"image",   kPink};
    if (cat == "media")   return {"media",   kGreen};
    if (cat == "doc")     return {"doc",     kBlue};
    if (cat == "archive") return {"archive", kGrey};

    return {nullptr, {}};
}

constexpr int   kWidth   = 1650;
constexpr int   kHeight  = 1400;
constexpr int   kMaxHits = 50;
constexpr float kRowH    = 24.f;
constexpr float kFontPx  = 18.f;
constexpr const char* kFont = "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf";

static const ImVec4 kBg     = {0.055f, 0.070f, 0.078f, 0.95f};
static const ImVec4 kBorder = {0.16f,  0.22f,  0.24f,  1.00f};
static const ImVec4 kText   = {0.86f,  0.90f,  0.90f,  1.00f};
static const ImVec4 kDim    = {0.45f,  0.52f,  0.54f,  1.00f};

static ImVec4 withAlpha(ImVec4 c, float a) { c.w = a; return c; }

static void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = s.FrameRounding = s.ChildRounding = 0.f;
    s.WindowBorderSize = 1.f;
    s.FrameBorderSize  = 0.f;
    s.WindowPadding    = {14, 12};
    s.FramePadding     = {4, 6};
    s.ItemSpacing      = {0, 2};

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]       = kBg;
    c[ImGuiCol_ChildBg]        = {0, 0, 0, 0};
    c[ImGuiCol_Border]         = kBorder;
    c[ImGuiCol_Text]           = kText;
    c[ImGuiCol_TextDisabled]   = kDim;
    c[ImGuiCol_FrameBg]        = {0, 0, 0, 0};
    c[ImGuiCol_FrameBgHovered] = {0, 0, 0, 0};
    c[ImGuiCol_FrameBgActive]  = {0, 0, 0, 0};
    c[ImGuiCol_Separator]      = kBorder;
    c[ImGuiCol_Header]         = withAlpha(kAccent, 0.14f);   
    c[ImGuiCol_HeaderHovered]  = withAlpha(kAccent, 0.08f);
    c[ImGuiCol_HeaderActive]   = withAlpha(kAccent, 0.20f);
}

static void glfwError(int code, const char* msg) {
    fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}

static bool drawRow(std::string_view path, ExecState state, bool selected) {
    bool clicked = ImGui::Selectable("##row", selected,
                       ImGuiSelectableFlags_AllowDoubleClick, {0, kRowH});

    size_t slash = path.rfind('/');
    std::string_view dir  = slash == std::string_view::npos ? "" : path.substr(0, slash);
    std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);

    ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImVec2 pos = {min.x + 10, min.y + (kRowH - ImGui::GetFontSize()) * 0.5f};
    float nameW = ImGui::CalcTextSize(name.data(), name.data() + name.size()).x;

    Tag tag = classify(path, state);
    static const float badgeW = ImGui::CalcTextSize("archive").x + 12;   
    const float rightPad = 10.f;
    const float textRight = tag.label ? max.x - rightPad - badgeW - 8 : max.x;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) dl->AddRectFilled(min, {min.x + 2, max.y}, ImGui::GetColorU32(kAccent));

    dl->PushClipRect(min, {textRight, max.y}, true);   
    dl->AddText(pos, ImGui::GetColorU32(kText), name.data(), name.data() + name.size());
    dl->AddText({pos.x + nameW + 16, pos.y}, ImGui::GetColorU32(kDim), dir.data(), dir.data() + dir.size());
    dl->PopClipRect();

    if (tag.label) {
        ImVec2 bmin = {max.x - rightPad - badgeW, min.y + 3};
        ImVec2 bmax = {max.x - rightPad,          max.y - 3};
        float tw = ImGui::CalcTextSize(tag.label).x;
        dl->AddRect(bmin, bmax, ImGui::GetColorU32(withAlpha(tag.color, 0.45f)));
        dl->AddText({bmin.x + (badgeW - tw) * 0.5f, pos.y}, ImGui::GetColorU32(tag.color), tag.label);
    }
    return clicked;
}

int main() {
    signal(SIGCHLD, SIG_IGN);
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

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                  
    if (access(kFont, R_OK) == 0) io.Fonts->AddFontFromFileTTF(kFont, kFontPx);
    ImGui::StyleColorsDark();
    applyTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    char buf[256] = "";
    std::string lastQuery;
    std::vector<SearchHit> results;
    results.reserve(kMaxHits);
    int selected = 0;
    bool scrollToSel = false;

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEventsTimeout(0.5);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("fizz", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        // prompt line: "> query"
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kAccent, ">");
        ImGui::SameLine(0, 8);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SetKeyboardFocusHere();         
        if (ImGui::InputText("##q", buf, sizeof(buf))) {
            std::string_view q(buf);
            if (q != lastQuery) {
                searchPaths(q, results, kMaxHits);
                lastQuery.assign(q);
                selected = 0;
            }
        }

        const int n = (int)results.size();
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected + 1 < n) { ++selected; scrollToSel = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)   && selected > 0)     { --selected; scrollToSel = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) glfwSetWindowShouldClose(window, 1);

        int chosen = -1;
        if ((ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) && n > 0)
            chosen = selected;

        ImGui::Separator();

	ImGui::BeginChild("results", {0, -ImGui::GetFrameHeightWithSpacing()});
	for (int i = 0; i < n; ++i) {
	    ImGui::PushID(i);
	    if (drawRow(results[i].path, results[i].execState, i == selected)) {
		selected = i;
		if (ImGui::IsMouseDoubleClicked(0)) chosen = i;
	    }
	    if (i == selected && scrollToSel) { ImGui::SetScrollHereY(); scrollToSel = false; }
	    ImGui::PopID();
	}
	if (n == 0 && !lastQuery.empty()) ImGui::TextDisabled("no matches");
	ImGui::EndChild();
	ImGui::TextDisabled("%d hits   enter open   shift+enter open folder   esc quit", n);
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
