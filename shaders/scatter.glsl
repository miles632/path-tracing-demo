#extension GL_EXT_nonuniform_qualifier : require

#include "raycommon.glsl"
#include "random.glsl"

hitPayload scatter(vec3 normal, vec3 rayDir, const float t, inout uint seed,
                   vec3 color, const float metallicW, const float roughness, const float dielectricW, const float eta, vec3 baseReflectance) {
    const float fuzz = clamp(roughness * roughness, 0.0, 1.0);
    vec3 rand3 = randomUnitInSphere(seed);
    //if (dot(rand3, normal) < 0.0) rand3 = -rand3; // flip to correct hemisphere

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
    vec3 attenuation = color;
    vec3 dir;
    if (r < metallicW) {
        dir = metalDir;
    } else if ( r < metallicW + dielectricW) {
        dir = dielectricDir;
    } else {
        float cosView = max(dot(-normalize(rayDir), n), 0.0);
        vec3 reflectivity = baseReflectance + (vec3(1.0) - baseReflectance) * pow(1.0 - cosView, 5.0);
        float reflectChance = clamp(dot(reflectivity, vec3(0.2126, 0.7152, 0.0722)), 0.0, 0.9);

        if (randomFloat(seed) < reflectChance) {
            dir = metalDir;
            attenuation = reflectivity / reflectChance;
        } else {
            dir = lambertDir;
            attenuation = color * (vec3(1.0) - reflectivity) / (1.0 - reflectChance);
        }
    }


    hitPayload payload;
    payload.RandomSeed = seed;
    payload.ColorAndDistance = vec4(attenuation, t);
    payload.ScatterDir = vec4(dir, 1.0);
    return payload;
}
