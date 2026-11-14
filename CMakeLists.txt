cmake_minimum_required(VERSION 3.16)
project(VolumeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Dependencies downloaded at configure time ------------------------------

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm)

# --- Bundled libraries -------------------------------------------------------

add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_library(lodepng STATIC external/lodepng/lodepng.cpp)
target_include_directories(lodepng PUBLIC external/lodepng)

add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_impl_glfw.cpp
    external/imgui/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC external/imgui)
target_compile_definitions(imgui PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(imgui PUBLIC glfw)

# --- Application -------------------------------------------------------------

add_executable(VolumeViewer
    src/main.cpp
    src/ClearView.cpp
    src/ICPVR.cpp
    src/IsoLayers.cpp
    src/Shader.cpp
    src/Volume.cpp)
target_link_libraries(VolumeViewer PRIVATE glad glfw glm::glm imgui lodepng)

# Fallback location of shaders/ and data/ when the program is not started from its own folder
target_compile_definitions(VolumeViewer PRIVATE ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Copy shaders and the dataset next to the executable
add_custom_command(TARGET VolumeViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:VolumeViewer>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data $<TARGET_FILE_DIR:VolumeViewer>/data)

if(MSVC)
    target_compile_options(VolumeViewer PRIVATE /W3 /utf-8)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT VolumeViewer)
    set_property(TARGET VolumeViewer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:VolumeViewer>)
else()
    target_compile_options(VolumeViewer PRIVATE -Wall -Wextra)
endif()
