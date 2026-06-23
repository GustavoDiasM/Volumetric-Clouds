#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

Camera::Camera(glm::vec3 pos)
    : position(pos), front(0.0f, 0.0f, -1.0f), up(0.0f, 1.0f, 0.0f)
{
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspect, float near_, float far_) const
{
    return glm::perspective(glm::radians(fov), aspect, near_, far_);
}

void Camera::processKeyboard(Direction dir, float dt)
{
    float v = speed * dt;
    glm::vec3 right = glm::normalize(glm::cross(front, up));
    switch (dir) {
        case FORWARD:  position += front * v; break;
        case BACKWARD: position -= front * v; break;
        case LEFT:     position -= right * v; break;
        case RIGHT:    position += right * v; break;
        case UP_DIR:   position += up    * v; break;
        case DOWN_DIR: position -= up    * v; break;
    }
}

void Camera::processMouse(float dx, float dy)
{
    yaw   += dx * sensitivity;
    pitch += dy * sensitivity;
    pitch  = std::clamp(pitch, -89.0f, 89.0f);
    updateVectors();
}

void Camera::processScroll(float dy)
{
    fov -= dy;
    fov  = std::clamp(fov, 10.0f, 120.0f);
}

void Camera::updateVectors()
{
    glm::vec3 f;
    f.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
    f.y = sinf(glm::radians(pitch));
    f.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
    front = glm::normalize(f);
}
