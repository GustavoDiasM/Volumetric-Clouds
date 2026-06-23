#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <memory>
#include "Camera.h"
#include "CloudRenderer.h"
#include "NoiseGenerator.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Application — gere a janela GLFW, o loop principal e o ImGui
// ─────────────────────────────────────────────────────────────────────────────
class Application
{
public:
    Application(int width = 1280, int height = 720, const char* title = "Volumetric Clouds");
    ~Application();

    void run();

private:
    GLFWwindow* window = nullptr;
    int  width, height;

    Camera                      camera;
    std::unique_ptr<CloudRenderer>  renderer;
    std::unique_ptr<NoiseGenerator> noiseGen;

    // Estado do rato
    bool  firstMouse = true;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    bool  mouseCaptured = false;

    // Timing
    float lastTime = 0.0f;
    float time     = 0.0f;

    void initOpenGL();
    void initImGui();
    void processInput(float dt);
    void renderImGui();
    void drawUI();

    // Callbacks GLFW (estáticos → reencaminham para a instância)
    static void onFramebufferResize(GLFWwindow* w, int width, int height);
    static void onMouseMove(GLFWwindow* w, double x, double y);
    static void onMouseButton(GLFWwindow* w, int button, int action, int mods);
    static void onScroll(GLFWwindow* w, double x, double y);
    static void onKey(GLFWwindow* w, int key, int scancode, int action, int mods);
};
