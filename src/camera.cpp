#include "camera.h"

#include <GLFW/glfw3.h>

void Camera::move(float dT) {
    if (keys[GLFW_KEY_W])
        pos += speed * front * dT;
    if (keys[GLFW_KEY_S])
        pos -= speed * front * dT;
    if (keys[GLFW_KEY_A])
        pos -= glm::normalize(glm::cross(front, up)) * speed * dT;
    if (keys[GLFW_KEY_D])
        pos += glm::normalize(glm::cross(front, up)) * speed * dT;

    if (keys[GLFW_KEY_SPACE])
        pos += up * speed * dT;
    if (keys[GLFW_KEY_LEFT_SHIFT])
        pos -= up * speed * dT;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(pos, pos + front, up);
}

void Camera::mouse(float xOffset, float yOffset, float sensitivity) {
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    yaw += xOffset;
    pitch += yOffset;

    // clamp
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    // recompute the front vector
    updateFront();
}

void Camera::updateFront() {
    glm::vec3 dir;

    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));

    front = glm::normalize(dir);
}

