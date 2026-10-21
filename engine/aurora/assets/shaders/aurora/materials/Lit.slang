#include <aurora/pipeline/GlobalBlock.slangh>
#include <aurora/pipeline/OpaquePass.slangh>
#include <aurora/pipeline/SceneBatch.slangh>
#include <aurora/pipeline/MaterialBatch.slangh>
#include <aurora/pipeline/SceneLightingPass.slangh>
#include <aurora/pipeline/SceneSurfaceBatch.slangh>
#include <aurora/pipeline/SceneEnvironmentPass.slangh>

struct SceneVarying
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
};
SceneVarying VSMain(float3 position : POSITION)
{
    SceneVarying result;
    float4 world = mul(gSceneBatch.world, float4(position, 1));
    result.position = mul(gOpaquePass.viewProjection, world);
    result.worldPosition = world.xyz;
    return result;
}
float ShadowVisibility(float3 worldPosition)
{
    if (gSceneSurface.surface.x < 0.5 || gSceneLighting.shadowControl.x < 0.5)
        return 1;
    float4 projected = mul(gSceneLighting.shadowViewProjection, float4(worldPosition, 1));
    float3 p = projected.xyz / projected.w;
    float2 uv = p.xy * float2(0.5, gSceneLighting.shadowControl.z * 0.5) + 0.5;
    if (any(uv < 0) || any(uv > 1) || p.z < 0 || p.z > 1)
        return 1;
    float depth = gShadowImage.SampleLevel(gShadowSampler, uv, 0);
    return p.z - gSceneLighting.shadowControl.y <= depth ? 1 : 0;
}
float2 EnvironmentUv(float3 direction)
{
    return float2(atan2(direction.z, direction.x) / (2 * 3.14159265) + .5, acos(clamp(direction.y, -1, 1)) / 3.14159265);
}
float4 FSMain(SceneVarying input) : SV_Target
{
    float3 toEye = gOpaquePass.cameraPosition.xyz - input.worldPosition;
    float3 normal = normalize(cross(ddx(input.worldPosition), ddy(input.worldPosition)));
    if (dot(normal, toEye) < 0)
        normal = -normal;
    float3 radiance = gDiffuseIrradiance.SampleLevel(gEnvironmentSampler, EnvironmentUv(normal), 0).rgb * gSceneEnvironment.environment.x;
    for (uint i = 0; i < (uint)gSceneLighting.lightCount.x; ++i) {
        float kind = gSceneLighting.directions[i].w;
        float3 delta = gSceneLighting.positions[i].xyz - input.worldPosition;
        float distance = length(delta);
        float3 toLight = kind == 1 ? -gSceneLighting.directions[i].xyz : delta / max(distance, 0.0001);
        float attenuation = kind == 1 ? 1 : pow(saturate(1 - distance / max(gSceneLighting.positions[i].w, 0.0001)), 2);
        if (kind == 2) {
            float cone = dot(-toLight, gSceneLighting.directions[i].xyz);
            attenuation *= saturate((cone - gSceneLighting.cones[i].y) / max(gSceneLighting.cones[i].x - gSceneLighting.cones[i].y, 0.0001));
        }
        float shadow = gSceneLighting.shadowControl.x == i + 1 ? ShadowVisibility(input.worldPosition) : 1;
        radiance += gSceneLighting.colors[i].rgb * gSceneLighting.colors[i].w * saturate(dot(normal, toLight)) * attenuation * shadow;
    }
    float3 eye = toEye / max(length(toEye), .0001);
    float roughness = saturate(gMaterial.surface.x);
    uint width, height, levels;
    gSpecularEnvironment.GetDimensions(0, width, height, levels);
    float3 prefiltered = gSpecularEnvironment.SampleLevel(gEnvironmentSampler, EnvironmentUv(reflect(-eye, normal)), roughness * (levels - 1)).rgb;
    float2 brdf = gBrdfIntegration.SampleLevel(gBrdfSampler, float2(saturate(dot(normal, eye)), roughness), 0).rg;
    float3 specular = prefiltered * (.04 * brdf.x + brdf.y) * gSceneEnvironment.environment.x;
    return float4(gMaterial.tint.rgb * radiance + specular, gMaterial.tint.a);
}
