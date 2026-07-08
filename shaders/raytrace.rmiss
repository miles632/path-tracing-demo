#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "raycommon.glsl"
#include "host_device.h"

layout(location = 0) rayPayloadInEXT hitPayload payload;

layout(push_constant) uniform _PushConstants { PushConstants pc; };

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);

    vec3 sunDir = normalize(vec3(0.2, 0.95, 0.1));

    float t = max(dir.y, 0.0);
    vec3 horizon = pc.clearColor.rgb;
    vec3 zenith  = vec3(0.8, 1.2, 2.5);
    vec3 skyColor = mix(horizon, zenith, t);


    float sunDot = dot(dir, sunDir);
    if (sunDot > 0.9995) {
        // the sun color
        skyColor = sunColor;
    }

    payload.ColorAndDistance = vec4(skyColor, -1);
    payload.ScatterDir = vec4(0.0);
}