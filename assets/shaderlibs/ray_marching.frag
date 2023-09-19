#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D mainColor;
layout (set = 0, binding = 1) uniform sampler2D mainDepth;

layout (set = 0, binding = 2) uniform ViewInfo {
    mat4 viewToWorldMatrix;
    mat4 worldToViewMatrix;
    mat4 viewToClipMatrix;
    mat4 worldToClipMatrix;
    mat4 clipToWorldMatrix;
    vec4 worldPos;
    vec4 zParam;
} viewInfo;

#include <shaderlibs/shapes/shapes.glsl>
#include <shaderlibs/view/depth.glsl>

void main()
{
    Box box;
    box.center = vec3(0, -0.5, -0.5);
    box.extent = vec3(2, 1, 2);

    vec4 ndc = vec4(
        (gl_FragCoord.x / 1366.0 - 0.5) * 2,
        (gl_FragCoord.y / 768.0 - 0.5) * 2,
        1, 1);

    vec4 pos = viewInfo.clipToWorldMatrix * ndc;
    vec3 dst = pos.xyz / pos.w;

    Ray ray;
    ray.origin = viewInfo.worldPos.xyz;
    ray.dir = normalize(dst - ray.origin);

    vec2 rayBoxDist = rayBoxDist(box, ray);

    float depth = texture(mainDepth, uv).x;
    float linearDepth = ToLinearDepth(depth, viewInfo.zParam);

    outFragColor = rayBoxDist.y > 0 && rayBoxDist.x < linearDepth ? vec4(0, 0, 0, 0) : texture(mainColor, uv);
}
