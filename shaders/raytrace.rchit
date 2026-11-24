#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "raycommon.glsl"
#include "host_device.h"
#include "scatter.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload payload;
layout(location = 1) rayPayloadEXT shadowPayload sPayload;


layout(push_constant) uniform _PushConstants { PushConstants pc; };

layout(binding = 4) readonly buffer VertexArray {
    float v[];
} vertices;
layout(binding = 5) readonly buffer IndexArray {
    uint i[];
} indices;
layout(binding = 6) readonly buffer OffsetArray {
    uvec4 o[]; // last element is not needed
                // z is for material index per primitive
} offsets;
layout(binding = 7) uniform sampler2D[] DiffuseTex;
layout(binding = 8, scalar) readonly buffer MaterialArray {
    Material m[];
} materials;
layout(binding = 9, scalar) readonly buffer EmissiveTriangleArray {
    EmissiveTriangle e[];
} emissiveTriangles;

Vertex UnpackVertex(uint baseIndex) {
    const uint VERTEX_STRIDE = 11; // 11 floats are stored per vertex
    const uint base = baseIndex * VERTEX_STRIDE;
    Vertex v;
    v.pos   = vec3(vertices.v[base + 0], vertices.v[base + 1], vertices.v[base + 2]);
    v.color = vec3(vertices.v[base + 3], vertices.v[base + 4], vertices.v[base + 5]);
    v.texture   = vec2(vertices.v[base + 6], vertices.v[base + 7]);
    v.normal = vec3(vertices.v[base + 8], vertices.v[base+9], vertices.v[base + 10]);

    return v;
}

void main() {
    const uvec3 offs = offsets.o[gl_InstanceCustomIndexEXT].xyz;

    const uint indexOffset = offs.y;
    const uint vertexOffset = offs.x;

    const Vertex v0 = UnpackVertex(vertexOffset + indices.i[indexOffset + gl_PrimitiveID * 3 + 0]);
    const Vertex v1 = UnpackVertex(vertexOffset + indices.i[indexOffset + gl_PrimitiveID * 3 + 1]);
    const Vertex v2 = UnpackVertex(vertexOffset + indices.i[indexOffset + gl_PrimitiveID * 3 + 2]);

    const Material mat = materials.m[offs.z];

    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 normal = normalize(mix(
        v0.normal, v1.normal, v2.normal, barycentrics
        )
    );

    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 worldNormal = normalize(normalMatrix * normal);
    vec2 uv = v0.texture * barycentrics.x + v1.texture * barycentrics.y + v2.texture * barycentrics.z;

    vec3 color = mat.baseColorFactor.xyz;
    //emissiveTriangleColor = texture(DiffuseTex[nonuniformEXT(emissiveTriMat.emissiveTexture)], emissiveUV).rgb;
    //vec3 emTextureColor = texture(DiffuseTex[nonuniformEXT(mat.emissiveTexture)], uv).rgb;
    if (mat.baseColorTexture > -1 && mat.baseColorTexture < pc.textureCount ) {
        color *= texture(DiffuseTex[nonuniformEXT(mat.baseColorTexture)], uv).rgb;
    }

    vec3 specularColor = mat.specularColorFactor.rgb;
    if (mat.specularColorTexture > -1) {
        specularColor *= texture(DiffuseTex[nonuniformEXT(mat.specularColorTexture)], uv).rgb;
    }

    vec4 emissive = mat.emissiveFactor;
    if (length(emissive.rgb) > 0.0) {
        vec3 emittedRadiance = emissive.rgb;
        if (mat.emissiveTexture > -1) {
            emittedRadiance *= textureLod(DiffuseTex[nonuniformEXT(mat.emissiveTexture)], uv, 0.0).rgb;
        }
        payload.ColorAndDistance = vec4(emittedRadiance, gl_HitTEXT);
        payload.ScatterDir = vec4(0.0);
        return;
    }

    if (mat.emissiveTexture > -1) {
        // skip 4 now
    }

    vec3 directLight = vec3(0.0); // the final light that will be added to the payload contribution
    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    hitPos += worldNormal * 0.01;
    sPayload.shadow = true;
    bool weighingSun = true;

    // this branch weighs any of the emissive triangles present in the scene,
    // based on random probability applied on a CDF of the triangle areas
    if (!weighingSun && emissiveTriangles.e.length() > 0) {
        // binary search to find the apppropriate triangle
        int high = int(emissiveTriangles.e.length()) - 1;
        int low = 0;
        float randomNormalizedF = randomFloat(payload.RandomSeed);
        while (low < high) {
            int mid = (low + high) / 2;
            EmissiveTriangle tri = emissiveTriangles.e[mid];
            if (tri.cdfWeight >= randomNormalizedF) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }
        int selectedTriangleIdx = low;
        EmissiveTriangle emissiveTri = emissiveTriangles.e[selectedTriangleIdx];

        // barycentric cords for the emissive triangle
        float u = randomFloat(payload.RandomSeed);
        float v = randomFloat(payload.RandomSeed);

        vec3 edge1 = emissiveTri.v1 - emissiveTri.v0;
        vec3 edge2 = emissiveTri.v2 - emissiveTri.v0;
        float sqrtU = sqrt(u);

        // sqrt(u) is needed here because otherwise the random samples would cluster near the top of the triangle
        vec3 emissivePoint = emissiveTri.v0 + sqrtU * (1.0 - v) * edge1 + sqrtU * v * edge2;

        // if a scene contains no other emissive sources just set the dir as the sunDir
        vec3 toLight = emissivePoint - hitPos;
        vec3 dirToLight = normalize(toLight);
        //bool weighingSun = emissiveTriangles.e.length() != 0 ? randomFloat(payload.RandomSeed) > 0.8 : true;
        //dirToLight = weighingSun ? sunDir.xyz : normalize(toLight);

        float shadowTmax = length(toLight) - 0.01;

        traceRayEXT(
            tlas,
            gl_RayFlagsSkipClosestHitShaderEXT,
            0xFF,
            1u,
            0u,
            1u,
            hitPos, 0.01, dirToLight, shadowTmax, 1
        );

        Material emissiveTriMat = materials.m[emissiveTri.materialIndex];
        vec2 emissiveUV =
        (1.0 - sqrtU) * emissiveTri.u0 +
        sqrtU * (1.0 - v) * emissiveTri.u1 +
        sqrtU * v * emissiveTri.u2;

        vec3 emissiveTriangleColor = vec3(1.0);
        if (emissiveTriMat.emissiveTexture > -1) {
            emissiveTriangleColor = texture(DiffuseTex[nonuniformEXT(emissiveTriMat.emissiveTexture)], emissiveUV).rgb;
        }

        if (!sPayload.shadow) {
            float cosRecipientSurface = max(dot(worldNormal, dirToLight), 0.0);
            vec3 emitterNormal = normalize(cross(emissiveTri.v1 - emissiveTri.v0, emissiveTri.v2 - emissiveTri.v0));
            float cosEmitterSurface = max(dot(emitterNormal, -dirToLight), 0.0);

            float dist2 = dot(toLight, toLight);
            float area = 0.5 * length(cross(emissiveTri.v1 - emissiveTri.v0, emissiveTri.v2 - emissiveTri.v0));

            float probabilityOfPickingTriangle = (selectedTriangleIdx > 0)
            ? emissiveTriangles.e[selectedTriangleIdx].cdfWeight - emissiveTriangles.e[selectedTriangleIdx - 1].cdfWeight
            : emissiveTriangles.e[selectedTriangleIdx].cdfWeight;
            float probabilityDensityTriangle = probabilityOfPickingTriangle / area;

            //directLight = emissiveTriangleMat.emissiveFactor.xyz * color * cosEmitterSurface * cosRecipientSurface / (probabilityDensityTriangle * dist2);
            vec3 lightColor = emissiveTriangleColor * emissiveTriMat.emissiveFactor.xyz;
            directLight = lightColor * (color/PI)* cosEmitterSurface * cosRecipientSurface / (probabilityDensityTriangle * dist2);
        }
    } else { // we weigh the sun instead
         float tmin = 0.001;
         float tmax = 1e4;
         vec3 sunDir = normalize(vec3(0.2, 0.95, 0.1));

         traceRayEXT(
            tlas,
            gl_RayFlagsSkipClosestHitShaderEXT,
            0xFF,
            1u,
            0u,
            1u,
            hitPos, 0.01, sunDir.xyz, tmax, 1
        );

        if (!sPayload.shadow) {
            float cosTheta = max(dot(worldNormal, sunDir), 0.0);
            directLight = cosTheta * sunColor * pc.lightIntensity * color;
        }
    }

    vec3 baseReflectance = mix(vec3(0.04) * specularColor, color, mat.metallicFactor);

    payload = scatter(worldNormal, gl_WorldRayDirectionEXT, gl_HitTEXT,
        payload.RandomSeed, color, mat.metallicFactor, mat.roughnessFactor, mat.transmissionFactor, 1.5, baseReflectance);
    payload.Radiance = directLight;
}