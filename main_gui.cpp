#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <csignal>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>

#include "services/QueryOps.h"
#include "services/LoadFile.h"

static void glfw_error_callback(int error, const char* desc) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

static void openPath(std::string_view path) {
    std::string p(path);
    if (fork() == 0) {
        setsid();
        execlp("xdg-open", "xdg-open", p.c_str(), (char*)nullptr);
        _exit(127);
    }
}

int main(int, char**) {
signal(SIGCHLD, SIG_IGN);                 
    loadIndexToMemory();                     
    glfwSetErrorCallback(glfw_error_callback);
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);   
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "fizz");    
    
    // 1. Wayland-specific hint to mark it as a transient pop-up/utility panel
    // This tells compositors it shouldn't tile with normal application windows
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(650, 400, "Fizz Search", nullptr, nullptr);
    if (!window) return 1;

    // 2. Query monitor sizes to physically place it in the center of the viewport
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode) {
            int xpos = (mode->width - 650) / 2;
            int ypos = (mode->height - 400) / 2;
            glfwSetWindowPos(window, xpos, ypos);
        }
    }

    // 3. Re-assert floating window state post-instantiation
    glfwSetWindowAttrib(window, GLFW_FLOATING, GLFW_TRUE); 

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                 
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    char search_buffer[256] = "";
    std::string last_query;
    std::vector<SearchHit> results;
    results.reserve(50);                      
    int selected = 0;
    bool scroll_to_selected = false;

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEventsTimeout(0.5);           

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowBgAlpha(0.6f); 
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::Begin("Spotlight Canvas", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##Search", "Search files...",
                                     search_buffer, sizeof(search_buffer))) {
            std::string_view q(search_buffer);
            if (q != last_query) {
                searchPaths(q, results, 50);  
                last_query.assign(q);
                selected = 0;
            }
        }

        const int n = (int)results.size();
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected + 1 < n) { ++selected; scroll_to_selected = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)   && selected > 0)     { --selected; scroll_to_selected = true; }

        int chosen = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && n > 0) chosen = selected;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) glfwSetWindowShouldClose(window, 1);

        ImGui::Separator();
        ImGui::Spacing();

        if (n > 0) {
           ImGui::BeginChild("Results", ImVec2(0, 0),
                  ImGuiChildFlags_NavFlattened,   
                  ImGuiWindowFlags_None);        
            for (int i = 0; i < n; ++i) {
                ImGui::PushID(i);
                const bool is_sel = (i == selected);
                if (ImGui::Selectable(results[i].path.data(), is_sel,
                                      ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 24.0f))) {
                    selected = i;
                    if (ImGui::IsMouseDoubleClicked(0)) chosen = i;
                }
                if (is_sel && scroll_to_selected) { ImGui::SetScrollHereY(); scroll_to_selected = false; }
                ImGui::PopID();
            }
            ImGui::EndChild();
        } else if (!last_query.empty()) {
            ImGui::TextDisabled("No matches");
        }

        ImGui::End();

        if (chosen >= 0) {
            openPath(results[chosen].path);
            glfwSetWindowShouldClose(window, 1);
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.f, 0.f, 0.f, 0.f);
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
