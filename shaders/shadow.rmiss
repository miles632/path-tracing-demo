#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "raycommon.glsl"

layout(location = 1) rayPayloadInEXT shadowPayload sPayload;

void main() {
    sPayload.shadow = false;
}