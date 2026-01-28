#extension GL_EXT_nonuniform_qualifier : require

#include "raycommon.glsl"
#include "random.glsl"

hitPayload scatterLambertian(vec3 normal, vec3 rayDir, const float distance, inout uint seed)
{
    vec4 scatterDir = vec4(normalize(normal + randomUnitInSphere(seed)), 1);
    if (length(scatterDir) < 1e-3)
        scatterDir = vec4(normal, 1);

    vec3 baseColor = vec3(0.3, 0.1, 0.7);

    hitPayload result;
    result.ScatterDir = scatterDir;
    result.ColorAndDistance = vec4(baseColor, distance);
    result.RandomSeed = seed;

    return result;
}


hitPayload scatterSpecular(vec3 normal, vec3 rayDir, const float distance, inout uint seed, vec3 color) {
    const vec3 reflected = reflect(normalize(rayDir), normal);
    const bool isScattered = dot(reflected, normal) > 0;

    hitPayload p;
    p.ColorAndDistance = vec4(color ,distance);
    p.ScatterDir = vec4(reflected + 0.000001 * randomUnitInSphere(seed), isScattered ? 1.0 : 0.0);
    p.RandomSeed = seed;
    return p;
}