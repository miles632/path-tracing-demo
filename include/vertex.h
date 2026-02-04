#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex;
    glm::vec3 normal;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

inline VkTransformMatrixKHR convert3x4GlmToVulkan(glm::mat3x4 mat) {
    VkTransformMatrixKHR result{};
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 4; col++) {
            result.matrix[row][col] = mat[row][col];
        }
    }

    return result;
}