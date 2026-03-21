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

hitAttributeEXT vec2 attribs;
rayPayloadInEXT hitPayload payload;

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

vec3 Mix(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}
vec2 Mix(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

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

    vec3 normal = normalize(Mix(
        v0.normal, v1.normal, v2.normal, barycentrics
        )
    );


    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 worldNormal = normalize(normalMatrix * normal);

    //not rendering any textures atm
    vec2 uv = v0.texture * barycentrics.x + v1.texture * barycentrics.y + v2.texture * barycentrics.z;



    vec3 color = vec3(1.0);
    if (mat.baseColorTexture > -1 && mat.baseColorTexture < pc.textureCount ) {
        color = texture(DiffuseTex[nonuniformEXT(mat.baseColorTexture)], uv).rgb;
    }
    if (mat.normalTexture > -1) {
        // ignore 4 now
    }
    vec4 emissive = mat.emissiveFactor;
    if (mat.emissiveTexture > -1) {

    }

    payload.ColorAndDistance = vec4(color, 1);

    //payload = scatterSpecular(worldNormal, gl_WorldRayDirectionEXT, gl_HitTEXT, payload.RandomSeed, color);
    //payload.ColorAndDistance = vec4((worldNormal * 0.5 + 0.5), 1.0);
}
