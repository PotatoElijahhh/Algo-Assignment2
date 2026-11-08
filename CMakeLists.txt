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
)
target_include_directories(seamcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(seamcore PRIVATE stb)

# Command-line tool
add_executable(seamcarve src/main.cpp)
target_link_libraries(seamcarve PRIVATE seamcore)

# Unit tests (run with: ctest --test-dir build -C Release)
enable_testing()
add_executable(seam_tests tests/test_main.cpp)
target_link_libraries(seam_tests PRIVATE seamcore)
add_test(NAME seam_tests COMMAND seam_tests)

foreach(target seamcore seamcarve seam_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
