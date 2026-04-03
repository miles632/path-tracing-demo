#ifndef COMMON_GLSL
#define COMMON_GLSL

struct hitPayload {
    vec4 ColorAndDistance;
    vec4 ScatterDir;
    uint RandomSeed;
};

vec3 mix(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}
vec2 mix(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

#endif