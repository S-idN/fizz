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
    services/AppLauncher.cpp
    services/Config.cpp
    services/FileSearch.cpp
    services/FocusTracker.cpp
    services/FontDiscovery.cpp
    services/LoadFile.cpp
    services/QueryOps.cpp
    services/Theme.cpp
    services/SearchUi.cpp
)

set(IMGUI_CORE_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

set(IMGUI_SOURCES
    ${IMGUI_CORE_SOURCES}
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
    services/AppLauncher.cpp
    services/Config.cpp
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

add_executable(fizz_tests
    tests/test_main.cpp
    tests/test_binary_cache.cpp
    tests/test_config.cpp
    tests/test_app_launcher.cpp
    tests/test_focus_tracker.cpp
    tests/test_font_discovery.cpp
    tests/test_theme.cpp
    tests/test_search_ui.cpp
    tests/test_file_cache_builder.cpp
    services/Config.cpp
    services/FileCacheBuilder.cpp
    services/FileSearch.cpp
    services/FocusTracker.cpp
    services/FontDiscovery.cpp
    services/LoadFile.cpp
    services/AppLauncher.cpp
    services/QueryOps.cpp
    services/Theme.cpp
    services/SearchUi.cpp
    ${IMGUI_CORE_SOURCES}
)

target_include_directories(fizz_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

target_link_libraries(fizz_tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME fizz_tests COMMAND fizz_tests)

# Hermetic unit tests for installer logic (GVariant parsing, WM sentinels, package maps, flag parsing)
# These are fast and 100% hermetic (run strictly in temporary directories without touching the host).
add_test(NAME installer_unit_tests COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_installer.sh --unit)

# Installer live lifecycle tests execute install.sh / uninstall.sh.
# Gated behind an option to preserve hermeticity during default CTest execution.
# Enable with:  cmake -DFIZZ_INSTALLER_TESTS=ON ..
option(FIZZ_INSTALLER_TESTS "Include installer live lifecycle tests in CTest" OFF)
if(FIZZ_INSTALLER_TESTS)
    add_test(NAME installer_live_tests COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_installer.sh --live)
endif()

install(TARGETS fizzd main_gui RUNTIME DESTINATION bin)
