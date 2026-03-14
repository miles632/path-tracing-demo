#ifndef HOST_DEVICE
#define HOST_DEVICE

#ifdef __cplusplus
#include <glm/glm.hpp>
    #define VEC2 glm::vec2
    #define VEC3 glm::vec3
    #define VEC4 glm::vec4
    #define MAT4 glm::mat4
    #define UINT32 uint32_t
    #define NS_BEGIN(name) namespace name {
    #define NS_END }
#else
    #define VEC2 vec2
    #define VEC3 vec3
    #define VEC4 vec4
    #define MAT4 mat4
    #define UINT32 uint
    //#define NS_BEGIN(name)
    //#define NS_END
#endif

struct Vertex {
    VEC3 pos;
    VEC3 color;
    VEC2 texture;
    VEC3 normal;
};

struct Material {
    UINT32 baseColorTexture;
    UINT32 normalTexture;
    UINT32 metallicRoughnessTexture;
    UINT32 occlusionTexture;
    UINT32 emissiveTexture;

    VEC4 baseColorFactor;
    UINT32 emissiveFactor;
    UINT32 metallicFactor;
    UINT32 roughnessFactor;
};

struct PushConstants {
    MAT4 viewInverse;
    MAT4 projInverse;
    VEC3 cameraPos;
    UINT32 frameIndex;
    VEC4 clearColor;
    VEC3 lightPos;
    float lightIntensity;
    int lightType;
    UINT32 frameCount;
};

struct UniformBufferObject {
    MAT4 model;
    MAT4 view;
    MAT4 proj;
};


#endif