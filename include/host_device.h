#pragma once
#ifdef __cplusplus
#include <glm/glm.hpp>
    #define VEC2 glm::vec2
    #define VEC3 glm::vec3
    #define VEC4 glm::vec4
    #define MAT4 glm::mat4
    #define NS_BEGIN(name) namespace name {
    #define NS_END }
#else
    #define VEC2 vec2
    #define VEC3 vec3
    #define VEC4 vec4
    #define MAT4 mat4
    #define NS_BEGIN(name)
    #define NS_END
#endif

NS_BEGIN(vk_device)

struct Vertex {
    VEC3 pos;
    VEC3 color;
    VEC2 tex;
    VEC3 normal;
};

/*
struct UniformBufferObject {
    MAT4 model;
    MAT4 view;
    MAT4 proj;
};
*/

struct PushConstants {
    MAT4 viewInverse;
    MAT4 projInverse;
    VEC3 cameraPos;
    uint frameIndex;
    VEC4 clearColor;
    VEC3 lightPos;
    float lightIntensity;
    int lightType;
    uint frameCount;
};

NS_END