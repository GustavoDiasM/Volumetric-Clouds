cmake_minimum_required(VERSION 3.20)
project(VolumetricClouds VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────
#  Fetch external dependencies
# ─────────────────────────────────────────────
include(FetchContent)

# GLFW
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM (header-only math)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# GLAD v2 — has proper CMake integration
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.6
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake
)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl REPRODUCIBLE API gl:core=4.3)

# ImGui
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# ─────────────────────────────────────────────
#  Main executable
# ─────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/Camera.cpp
    src/Shader.cpp
    src/CloudRenderer.cpp
    src/NoiseGenerator.cpp
)

add_executable(VolumetricClouds ${SOURCES})

target_include_directories(VolumetricClouds PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(VolumetricClouds PRIVATE
    glfw
    glad_gl
    glm::glm
    imgui_lib
    ${CMAKE_DL_LIBS}
)

# On Linux we also need X11/Xrandr/etc. (GLFW depends on them)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(VolumetricClouds PRIVATE ${X11_LIBRARIES} pthread dl)
endif()

# Copy shaders next to the binary
add_custom_command(TARGET VolumetricClouds POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:VolumetricClouds>/shaders"
    COMMENT "Copying shaders…"
)
