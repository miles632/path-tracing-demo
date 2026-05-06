#pragma once


#include <glm/ext/matrix_transform.hpp>

struct Camera {
    float yaw = -90.0f;
    float pitch = 0.0f;

    float speed = 5.0f;

    bool keys[1024] = {false};

    glm::vec3 pos = {0.0f, 0.0f, 3.0f};
    glm::vec3 front = {0.0f, 0.0f, -1.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};

    bool move(float dT); // true if the state of the camera was changed during move
    bool mouse(float xOffset, float yOffset, float sensitivity = 0.1f);
    void updateFront();
    void resetCamera();
    glm::mat4 getViewMatrix() const;
};
