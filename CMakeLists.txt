cmake_minimum_required(VERSION 3.16)
project(fizz VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)   

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

set(SERVICES_SOURCES
    services/FileCacheBuilder.cpp
    services/FileSearch.cpp
    services/LoadFile.cpp
    services/QueryOps.cpp
)

set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
)

add_executable(main_gui main_gui.cpp ${SERVICES_SOURCES} ${IMGUI_SOURCES})

target_include_directories(main_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

target_link_libraries(main_gui PRIVATE
    glfw
    OpenGL::GL
    Threads::Threads
)

target_compile_options(main_gui PRIVATE -O2)
target_link_options(main_gui PRIVATE -s)   

add_executable(fizzd
    services/FileCacheBuilder.cpp
    services/FileSearch.cpp
)

target_compile_definitions(fizzd PRIVATE FIZZD_MAIN)

target_include_directories(fizzd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(fizzd PRIVATE Threads::Threads)
target_link_options(fizzd PRIVATE -s)

install(TARGETS fizzd main_gui RUNTIME DESTINATION bin)
