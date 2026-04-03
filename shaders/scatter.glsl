#extension GL_EXT_nonuniform_qualifier : require

#include "raycommon.glsl"
#include "random.glsl"

hitPayload scatter(vec3 normal, vec3 rayDir, const float t, inout uint seed,
                   vec3 color, const float metallicW, const float roughness, const float dielectricW, const float eta) {
    const float fuzz = clamp(roughness * roughness, 0.0, 1.0);
    const vec3 rand3 = randomUnitInSphere(seed);

    vec3 raw = normal + rand3;
    const vec3 lambertDir = (dot(raw, raw) < 1e-6) ? normal : normalize(raw);
    const vec3 metalDir = normalize(reflect(rayDir, normal) + fuzz * rand3);

    bool frontFace = dot(rayDir, normal) < 0.0;
    vec3 n = frontFace ? normal : -normal;
    float etaRatio = frontFace ? (1.0 / eta) : eta;
    vec3 refracted = refract(normalize(rayDir), n, etaRatio);
    float cosTheta = abs(dot(normalize(rayDir), n));
    vec3 dielectricDir = (length(refracted) < 0.001)
        ? reflect(rayDir, n)
        : refracted;

    float r = randomFloat(seed);
    vec3 dir;
    if (r < metallicW)
        dir = metalDir;
    else if ( r < metallicW + dielectricW)
        dir = dielectricDir;
    else
        dir = lambertDir;

    hitPayload payload;
    payload.ColorAndDistance = vec4(color, t);
    payload.ScatterDir = vec4(dir, 1.0);
    return payload;
}
