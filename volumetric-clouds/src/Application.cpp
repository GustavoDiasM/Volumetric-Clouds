#include "Application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <stdexcept>

// ── Construtor / Destrutor ────────────────────────────────────────────────────

Application::Application(int w, int h, const char* title)
    : width(w), height(h),
      camera(glm::vec3(0.0f, 2500.0f, 0.0f))   // câmara começa dentro da camada de nuvens
{
    initOpenGL();
    initImGui();

    // Gerar ruído na GPU (uma vez)
    noiseGen = std::make_unique<NoiseGenerator>();
    noiseGen->generate();

    // Inicializar renderer
    renderer = std::make_unique<CloudRenderer>();
    renderer->init(width, height, noiseGen->getShapeNoise(), noiseGen->getDetailNoise());
}

Application::~Application()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

// ── Loop principal ────────────────────────────────────────────────────────────

void Application::run()
{
    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;
        time      = now;

        glfwPollEvents();
        processInput(dt);

        // Render frame
        renderer->render(camera, time, width, height);

        // ImGui overlay
        renderImGui();

        glfwSwapBuffers(window);
    }
}

// ── Inicialização ─────────────────────────────────────────────────────────────

void Application::initOpenGL()
{
    if (!glfwInit())
        throw std::runtime_error("glfwInit falhou");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(width, height, "Volumetric Clouds — Real-Time Rendering", nullptr, nullptr);
    if (!window)
        throw std::runtime_error("glfwCreateWindow falhou");

    glfwSetWindowUserPointer(window, this);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Carregar OpenGL com GLAD
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
        throw std::runtime_error("gladLoadGL falhou");

    std::cout << "[OpenGL] " << glGetString(GL_VERSION) << "\n";
    std::cout << "[GPU]    " << glGetString(GL_RENDERER) << "\n";

    // Callbacks
    glfwSetFramebufferSizeCallback(window, onFramebufferResize);
    glfwSetCursorPosCallback      (window, onMouseMove);
    glfwSetMouseButtonCallback    (window, onMouseButton);
    glfwSetScrollCallback         (window, onScroll);
    glfwSetKeyCallback            (window, onKey);

    glEnable(GL_DEPTH_TEST);
}

void Application::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
}

// ── Input ─────────────────────────────────────────────────────────────────────

void Application::processInput(float dt)
{
    if (!mouseCaptured) return; // só move câmara com o rato capturado (botão direito)

    auto* w = window;
    if (glfwGetKey(w, GLFW_KEY_W)     == GLFW_PRESS) camera.processKeyboard(Camera::FORWARD,  dt);
    if (glfwGetKey(w, GLFW_KEY_S)     == GLFW_PRESS) camera.processKeyboard(Camera::BACKWARD, dt);
    if (glfwGetKey(w, GLFW_KEY_A)     == GLFW_PRESS) camera.processKeyboard(Camera::LEFT,     dt);
    if (glfwGetKey(w, GLFW_KEY_D)     == GLFW_PRESS) camera.processKeyboard(Camera::RIGHT,    dt);
    if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) camera.processKeyboard(Camera::UP_DIR,   dt);
    if (glfwGetKey(w, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera.processKeyboard(Camera::DOWN_DIR, dt);
}

// ── ImGui UI ──────────────────────────────────────────────────────────────────

void Application::renderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawUI();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::drawUI()
{
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({350, 600}, ImGuiCond_Once);
    ImGui::Begin("Volumetric Clouds — Controlo");

    auto& p = renderer->params;

    if (ImGui::CollapsingHeader("Camada de Nuvens", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Altitude Mínima (m)",  &p.cloudBottomAltitude, 500.0f,  3000.0f);
        ImGui::SliderFloat("Altitude Máxima (m)",  &p.cloudTopAltitude,    2000.0f, 8000.0f);
        ImGui::SliderFloat("Cobertura",             &p.cloudCoverage,       0.0f,    1.0f);
        ImGui::SliderFloat("Densidade",             &p.cloudDensity,        0.01f,   1.0f);
    }

    if (ImGui::CollapsingHeader("Ruído / Forma"))
    {
        ImGui::SliderFloat("Escala Shape",          &p.shapeNoiseScale,    0.00001f, 0.0005f, "%.6f");
        ImGui::SliderFloat("Escala Detail",         &p.detailNoiseScale,   0.0001f,  0.002f,  "%.5f");
        ImGui::SliderFloat("Força Detail",          &p.detailNoiseStrength, 0.0f,    0.5f);
    }

    if (ImGui::CollapsingHeader("Vento"))
    {
        ImGui::SliderFloat3("Direcção",  &p.windDirection.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Velocidade", &p.windSpeed, 0.0f, 200.0f);
    }

    if (ImGui::CollapsingHeader("Iluminação", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat3("Direcção do Sol", &p.sunDirection.x, -1.0f, 1.0f);
        ImGui::ColorEdit3("Cor do Sol",        &p.sunColor.x);
        ImGui::SliderFloat("Intensidade Solar",&p.sunIntensity,    0.0f, 5.0f);
        ImGui::SliderFloat("Absorção (luz)",   &p.lightAbsorption, 0.0f, 2.0f);
        ImGui::SliderFloat("Absorção (câmara)",&p.cloudAbsorption, 0.0f, 2.0f);
        ImGui::SliderFloat("Dark Edge",        &p.darkEdgeFactor,  0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Scattering (Henyey-Greenstein)"))
    {
        ImGui::SliderFloat("Forward (g+)",  &p.hgForward,   0.0f,  0.99f);
        ImGui::SliderFloat("Backward (g-)", &p.hgBackward, -0.99f, 0.0f);
        ImGui::SliderFloat("Mistura",       &p.hgBlend,     0.0f,  1.0f);
    }

    if (ImGui::CollapsingHeader("Ray March"))
    {
        ImGui::SliderInt("Passos primários", &p.primarySteps, 16, 128);
        ImGui::SliderInt("Passos de luz",    &p.lightSteps,    2,  16);
    }

    if (ImGui::CollapsingHeader("Céu"))
    {
        ImGui::ColorEdit3("Zenith",   &p.skyColorZenith.x);
        ImGui::ColorEdit3("Horizonte",&p.skyColorHorizon.x);
        ImGui::ColorEdit3("Névoa",    &p.fogColor.x);
    }

    ImGui::Separator();
    ImGui::Text("FPS: %.1f  (%.2f ms)", ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);
    ImGui::Text("Câmara: (%.0f, %.0f, %.0f)",
                camera.position.x, camera.position.y, camera.position.z);
    ImGui::Text("Rato direito + WASD para mover");
    ImGui::Text("Space/Ctrl = subir/descer");

    ImGui::End();
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void Application::onFramebufferResize(GLFWwindow* w, int width, int height)
{
    auto* app = (Application*)glfwGetWindowUserPointer(w);
    app->width  = width;
    app->height = height;
    app->renderer->resize(width, height);
}

void Application::onMouseMove(GLFWwindow* w, double x, double y)
{
    auto* app = (Application*)glfwGetWindowUserPointer(w);
    if (!app->mouseCaptured) { app->firstMouse = true; return; }

    if (app->firstMouse) {
        app->lastMouseX = (float)x;
        app->lastMouseY = (float)y;
        app->firstMouse = false;
    }
    float dx =  (float)x - app->lastMouseX;
    float dy = -(float)y + app->lastMouseY; // y invertido
    app->lastMouseX = (float)x;
    app->lastMouseY = (float)y;
    app->camera.processMouse(dx, dy);
}

void Application::onMouseButton(GLFWwindow* w, int button, int action, int /*mods*/)
{
    // ImGui quer este evento
    if (ImGui::GetIO().WantCaptureMouse) return;

    auto* app = (Application*)glfwGetWindowUserPointer(w);
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        app->mouseCaptured = (action == GLFW_PRESS);
        glfwSetInputMode(w, GLFW_CURSOR,
            app->mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        app->firstMouse = true;
    }
}

void Application::onScroll(GLFWwindow* w, double /*x*/, double y)
{
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* app = (Application*)glfwGetWindowUserPointer(w);
    app->camera.processScroll((float)y);
}

void Application::onKey(GLFWwindow* w, int key, int /*sc*/, int action, int /*mods*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(w, true);
}
