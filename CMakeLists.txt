cmake_minimum_required(VERSION 3.20)
project(SeamCarving LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# stb_image / stb_image_write (public domain) are only used to read and write image files.
include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        4e9b368bfc071e97206ea5c6adba68dd0cbf8897
)
FetchContent_MakeAvailable(stb)

add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

# Core seam carving library
add_library(seamcore
    src/image.cpp
    src/energy.cpp
    src/seam_dp.cpp
    src/seam_dp_improved.cpp
    src/seam_greedy.cpp
    src/seam_graphcut.cpp
    src/seam_method.cpp
    src/carver.cpp
    src/parallel.cpp
    src/visualise.cpp
    src/interactive.cpp
    src/seam_order.cpp
)
target_include_directories(seamcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(seamcore PRIVATE stb)

# Multithreading (assignment item 5d). If the compiler has no OpenMP, the code still
# builds and simply runs on one thread.
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(seamcore PUBLIC OpenMP::OpenMP_CXX)
endif()

# Command-line tool
add_executable(seamcarve src/main.cpp)
target_link_libraries(seamcarve PRIVATE seamcore)

# Benchmark tool for the DP improvements (assignment item 5d)
add_executable(seambench src/bench.cpp)
target_link_libraries(seambench PRIVATE seamcore)

# Unit tests (run with: ctest --test-dir build -C Release)
enable_testing()
add_executable(seam_tests tests/test_main.cpp)
target_link_libraries(seam_tests PRIVATE seamcore)
add_test(NAME seam_tests COMMAND seam_tests)

# Optional slider window (assignment bonus 7b). OFF by default, so normal builds and CI
# don't download or need anything extra. Turn on with: cmake -S . -B build -DSEAM_BUILD_GUI=ON
option(SEAM_BUILD_GUI "Build the seamgui slider window (downloads Dear ImGui and GLFW)" OFF)
set(SEAM_TARGETS seamcore seamcarve seambench seam_tests)

if(SEAM_BUILD_GUI)
    # GLFW opens the window and handles input. Only build the library itself.
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE) # Linux: X11 is enough
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    # Dear ImGui draws the sliders and buttons (MIT licence). It has no CMake file of its own,
    # so we list its source files here.
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.9
    )
    FetchContent_MakeAvailable(glfw imgui)
    find_package(OpenGL REQUIRED)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    )
    target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

    add_executable(seamgui src/gui_main.cpp)
    target_link_libraries(seamgui PRIVATE seamcore imgui)
    list(APPEND SEAM_TARGETS seamgui)
endif()

foreach(target ${SEAM_TARGETS})
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
