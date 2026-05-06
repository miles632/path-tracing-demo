#include "camera.h"

#include <GLFW/glfw3.h>

bool Camera::move(float dT) {
    bool stateChanged = false;
    auto changeS = [&stateChanged]() {
        stateChanged = true;
    };

    if (keys[GLFW_KEY_W]) {
        pos += speed * front * dT; changeS();
    }
    if (keys[GLFW_KEY_S]) {
        pos -= speed * front * dT; changeS();
    }
    if (keys[GLFW_KEY_A]) {
        pos -= glm::normalize(glm::cross(front, up)) * speed * dT; changeS();
    }
    if (keys[GLFW_KEY_D]) {
        pos += glm::normalize(glm::cross(front, up)) * speed * dT; changeS();
    }

    if (keys[GLFW_KEY_SPACE]) {
        pos += up * speed * dT; changeS();
    }
    if (keys[GLFW_KEY_LEFT_SHIFT]) {
        pos -= up * speed * dT; changeS();
    }

    return stateChanged;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(pos, pos + front, up);
}

bool Camera::mouse(float xOffset, float yOffset, float sensitivity) {
    if (xOffset == 0 && yOffset == 0) {
        return false;
    }

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

    return true;
}

void Camera::updateFront() {
    glm::vec3 dir;

    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));

    front = glm::normalize(dir);
}

void Camera::resetCamera() {
    yaw = -90.0f;
    pitch = 0.0f;

    pos = {0.0f, 0.0f, 3.0f};
    front = {0.0f, 0.0f, -1.0f};
    up = {0.0f, 1.0f, 0.0f};
}