#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  Camera  — câmara FPS livre (WASD + rato)
//
//  A câmara guarda a sua posição e orientação e fornece:
//    • getViewMatrix()         — matriz view (world→camera)
//    • getProjectionMatrix()   — perspectiva
//    • position / front        — para usar nos shaders de nuvens
// ─────────────────────────────────────────────────────────────────────────────
class Camera
{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw   = -90.0f;   // graus
    float pitch =   0.0f;

    float speed       = 500.0f;   // unidades/segundo
    float sensitivity = 0.1f;
    float fov         = 60.0f;

    Camera(glm::vec3 pos = glm::vec3(0.0f, 1500.0f, 0.0f));

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect, float near_ = 1.0f, float far_ = 100000.0f) const;

    // Processamento de input
    enum Direction { FORWARD, BACKWARD, LEFT, RIGHT, UP_DIR, DOWN_DIR };
    void processKeyboard(Direction dir, float dt);
    void processMouse(float dx, float dy);
    void processScroll(float dy);

private:
    void updateVectors();
};
