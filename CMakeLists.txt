cmake_minimum_required(VERSION 3.16)
project(fizz VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to an optimized build (the search loop is very slow at -O0)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)   # LTO

# 1. System packages
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# 2. Core logic
set(SERVICES_SOURCES
    services/FileSearch.cpp
    services/InvertedSearchIndex.cpp
    services/LoadFile.cpp
    services/QueryOps.cpp
)

# 3. Dear ImGui core + backends
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
)

# 4. GUI executable
add_executable(main_gui main_gui.cpp ${SERVICES_SOURCES} ${IMGUI_SOURCES})

# 5. Include paths
target_include_directories(main_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

# 6. Link libraries
target_link_libraries(main_gui PRIVATE
    glfw
    OpenGL::GL
    Threads::Threads
)

# 7. Optional size/speed tweaks (remove if they cause problems)
target_compile_options(main_gui PRIVATE -O2)
target_link_options(main_gui PRIVATE -s)   # strip symbols; drop this while debugging
