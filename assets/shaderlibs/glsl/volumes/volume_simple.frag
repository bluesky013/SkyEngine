#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec3 pos;
layout (location = 0) out vec4 outFragColor;

#include <shaderlibs/glsl/layouts/standard_perpass.glsl>
#include <shaderlibs/glsl/shapes/shapes.glsl>
#include <shaderlibs/glsl/view/depth.glsl>

void main()
{
    Box box;
    box.center = vec3(0, -0.5, -0.5);
    box.extent = vec3(2, 1, 2);

    vec4 ndc = vec4(
    (gl_FragCoord.x / passInfo.viewport.z - 0.5) * 2,
    (gl_FragCoord.y / passInfo.viewport.w - 0.5) * 2,
    1, 1);

    vec4 pos = viewInfo.clipToWorldMatrix * ndc;
    vec3 dst = pos.xyz / pos.w;

    Ray ray;
    ray.origin = viewInfo.worldPos.xyz;
    ray.dir = normalize(dst - ray.origin);

    vec2 rayBoxDist = rayBoxDist(box, ray);
    float linearDepth = ToLinearDepth(gl_FragCoord.z, viewInfo.zParam);
    outFragColor = vec4(0, 0, 0, 0);
//    outFragColor = rayBoxDist.y > 0 && rayBoxDist.x < linearDepth ? vec4(0, 0, 0, 0) : texture(mainColor, uv);
}
