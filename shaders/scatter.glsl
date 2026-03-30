#extension GL_EXT_nonuniform_qualifier : require

#include "raycommon.glsl"
#include "random.glsl"

hitPayload scatter(vec3 normal, vec3 rayDir, const float t, inout uint seed, vec3 color, const float metallicW, const float roughness, vec4 clearColor) {
    const float fuzz = roughness*roughness;

    const float lambertW = 1.0 - metallicW;
    const float dielectricW = 0.0;

    const float sumW = metallicW + lambertW + dielectricW + 1e-6f;
    const vec3 normW = vec3(metallicW / sumW, lambertW / sumW, dielectricW / sumW);

    const vec3 rand3 = randomUnitInSphere(seed);

    const vec3 lambertDir = normalize(normal + rand3);
    const vec3 metalReflection = reflect(rayDir, normal);
    const vec3 metalDir = normalize(metalReflection + fuzz * rand3);

    // skipping dielectric
    const vec3 dir = metalDir * normW.x + lambertDir * normW.y + vec3(0) * normW.z;

    hitPayload payload;
    payload.ColorAndDistance = vec4(color, t);
    payload.ScatterDir = vec4(normalize(dir), 1.0);
    return payload;
}