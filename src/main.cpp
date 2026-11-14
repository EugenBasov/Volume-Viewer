// Volume Viewer: CT ray casting with ClearView and ICPVR
#define GLM_ENABLE_EXPERIMENTAL

#include "ClearView.h"
#include "ICPVR.h"
#include "Paths.h"
#include "Shader.h"
#include "Volume.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cmath>
#include <iostream>

namespace {

int winWidth = 1200;
int winHeight = 900;

// Orbit camera looking at the centre of the volume
glm::vec3 camPos = glm::vec3(0.f, 0.f, 2.f);
glm::vec3 camUp = glm::vec3(0.f, 1.f, 0.f);
glm::vec3 camRight = glm::vec3(1.f, 0.f, 0.f);
glm::vec3 camForward = glm::vec3(0.f, 0.f, 1.f);
glm::mat3 camRotationDelta = glm::mat3(1.f);   // rotation accumulated since the last frame
float radius = 2.f;

bool dragging = false;        // left mouse button: rotate the camera
bool draggingRight = false;   // right mouse button: resize the ClearView lens
double lastX = 0.0;
double lastY = 0.0;
glm::vec2 cursorPos;          // normalised cursor position = centre of the ClearView lens

glm::vec3 applyCameraRotation(glm::vec3 position)
{
    const glm::vec3 rotated = camRotationDelta * position;
    camRotationDelta = glm::mat3(1.f);
    return rotated;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            dragging = true;
            glfwGetCursorPos(window, &lastX, &lastY);
        }
        else if (action == GLFW_RELEASE) {
            dragging = false;
            camRotationDelta = glm::mat3(1.f);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            draggingRight = true;
            glfwGetCursorPos(window, &lastX, &lastY);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE) {
            draggingRight = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void cursorPositionCallback(GLFWwindow* /*window*/, double xpos, double ypos)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;

    // Right drag: the lens size changes exponentially with the horizontal offset
    static float oldLensSize = 0;
    if (draggingRight) {
        if (oldLensSize == 0)
            oldLensSize = ClearView::getLensSize();
        const float mult = std::pow(10.f, float(xpos / winWidth) - cursorPos.x);
        ClearView::setLensSize(oldLensSize * mult);
        return;
    }
    oldLensSize = 0;

    cursorPos = glm::vec2(float(xpos / winWidth), float(1.f - (ypos / winHeight)));
    if (!dragging)
        return;

    // Left drag: rotate the camera around the volume
    const float sensitivity = 0.005f;
    float dx = static_cast<float>(xpos - lastX);
    float dy = static_cast<float>(ypos - lastY);
    if (dx == 0 && dy == 0) {
        camRotationDelta = glm::mat3(1.f);
        return;
    }

    // Snap to a purely horizontal / vertical rotation when the mouse moves mostly along one axis
    if (std::abs(dy / dx) >= 3.f) {
        dy = glm::sign(dy) * std::sqrt(dy * dy + dx * dx);
        dx = 0;
    }
    if (std::abs(dx / dy) >= 3.f) {
        dx = glm::sign(dx) * std::sqrt(dy * dy + dx * dx);
        dy = 0;
    }

    camRotationDelta = glm::mat3(glm::rotate(-dy * sensitivity, camRight));
    camForward = -glm::normalize(camRotationDelta * camPos);
    camUp = camRotationDelta * camUp;
    camUp = glm::normalize(camUp - (glm::dot(camUp, camForward) * camForward));

    camRotationDelta = glm::mat3(glm::rotate(-dx * sensitivity, camUp)) * camRotationDelta;
    camForward = -glm::normalize(camRotationDelta * camPos);
    camRight = glm::normalize(glm::cross(camForward, camUp));

    lastX = xpos;
    lastY = ypos;
}

void scrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;
    radius -= radius * (1.f - 1.f / (1.f + float(yoffset) * 0.15f)) / 2.2f;
    camPos = glm::normalize(camPos) * radius;
}

void windowSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    winWidth = width;
    winHeight = height;
}

} // namespace

int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialise GLFW" << std::endl;
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(winWidth, winHeight, "Volume Viewer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create an OpenGL 3.3 window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetWindowSizeCallback(window, windowSizeCallback);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to load OpenGL functions" << std::endl;
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    const GLuint program = loadShaderProgram(assetPath("shaders/fullscreen.vert"),
                                             assetPath("shaders/volume.frag"));
    if (!program)
        return 1;

    // Full-screen quad - every pixel casts one ray
    const float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    const unsigned int quadIndices[] = { 0, 1, 2, 2, 3, 0 };

    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "volumeTexture"), 0);

    // Initial layers (densities of the tissues in the dataset, normalised to [0, 1])
    const float densitySkin = 1056.f / 3273.f;
    const float densitySoftBone = 1428.f / 3273.f;

    ClearView clearView(program);
    clearView.addISO("bone", densitySoftBone, glm::vec3(0.8f, 0.8f, 0.8f));
    clearView.addISO("skin", densitySkin / 2.f, glm::vec3(0.2f, 0.2f, 1.0f));

    ICPVR icpvr(program);
    icpvr.addISO("bone", 1.f, glm::vec3(0.387f, 0.387f, 0.387f));
    icpvr.addISO("skin", 0.189f, glm::vec3(0.104f, 0.104f, 0.760f));
    icpvr.addISO("muscle1", 0.325f, glm::vec3(0.775f, 0.163f, 0.163f));
    icpvr.addISO("brain", 0.330f, glm::vec3(0.000f, 1.000f, 0.912f));
    icpvr.addISO("other", 0.525f, glm::vec3(0.760f, 0.709f, 0.179f));
    icpvr.addISO("muscle2", 0.385f, glm::vec3(0.770f, 0.162f, 0.162f));
    icpvr.lowerThreshold = 0.074f;
    icpvr.kt = 7.559f;
    icpvr.ks = 0.553f;

    // CT scan of a head: 113 slices of 256 x 256 16-bit samples
    Volume volume;
    if (!loadVolume(assetPath("data/cthead") + "/cthead-16bit", 113, volume))
        return 1;
    const GLuint volumeTexture = createVolumeTexture(volume);

    const glm::vec3 cellSize = 1.f / glm::vec3(volume.width, volume.height, volume.depth);
    glUniform3fv(glGetUniformLocation(program, "cellSize"), 1, glm::value_ptr(cellSize));
    glUniform1f(glGetUniformLocation(program, "gradientMagnitude_min"), volume.gradientMin);
    glUniform1f(glGetUniformLocation(program, "gradientMagnitude_max"), volume.gradientMax);

    const GLint cameraPosLoc = glGetUniformLocation(program, "cameraPos");
    const GLint cameraDirLoc = glGetUniformLocation(program, "cameraDir");
    const GLint cameraRightLoc = glGetUniformLocation(program, "cameraRight");
    const GLint cameraRadLoc = glGetUniformLocation(program, "cameraRad");
    const GLint cursorPosLoc = glGetUniformLocation(program, "cursorPos");
    const GLint aspectRatioLoc = glGetUniformLocation(program, "aspectRatio");
    const GLint resolutionLoc = glGetUniformLocation(program, "resolution");
    const GLint techniqueLoc = glGetUniformLocation(program, "technique");

    bool useClearView = true;

    while (!glfwWindowShouldClose(window)) {
        // Per-frame uniforms
        camPos = applyCameraRotation(camPos);
        const glm::mat3 camDir = glm::inverse(glm::lookAt(camPos, glm::vec3(0.0f), camUp));
        glUseProgram(program);
        glUniform3fv(cameraPosLoc, 1, glm::value_ptr(camPos));
        glUniformMatrix3fv(cameraDirLoc, 1, GL_FALSE, glm::value_ptr(camDir));
        glUniform3fv(cameraRightLoc, 1, glm::value_ptr(camRight));
        glUniform1f(cameraRadLoc, radius);
        glUniform2fv(cursorPosLoc, 1, glm::value_ptr(cursorPos));
        glUniform1f(aspectRatioLoc, winHeight / float(winWidth));
        glUniform2fv(resolutionLoc, 1, glm::value_ptr(glm::vec2(winWidth, winHeight)));
        glUniform1i(techniqueLoc, useClearView);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, volumeTexture);
        glBindVertexArray(vao);

        glClear(GL_COLOR_BUFFER_BIT);
        if (useClearView)
            clearView.renderImage(winWidth, winHeight);
        else
            icpvr.renderImage(winWidth, winHeight);

        // User interface
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Rendering settings");
        ImGui::Text("Visualization technique:");
        if (ImGui::Button(useClearView ? "Clear-View" : "ICPVR", ImVec2(24 * 7, 2 * 14)))
            useClearView = !useClearView;
        ImGui::NewLine();
        ImGui::NewLine();
        if (useClearView)
            clearView.renderInterface();
        else
            icpvr.renderInterface();
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
