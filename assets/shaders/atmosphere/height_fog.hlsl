
const float2 FT_POSITIONS[3] = {
    float2(-1.0,  3.0),
    float2(-1.0, -1.0),
    float2( 3.0, -1.0)
};

const float2 FT_UVS[3] = {
    float2(0.0, 2.0),
    float2(0.0, 0.0),
    float2(2.0, 0.0)
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD;
};

VSOutput VSMain(uint vid : SV_VertexID)
{
    VSOutput res;
    res.Pos = float4(FT_POSITIONS[vid], 0.0, 1.0);
    res.UV = FT_UVS[vid];
    return res;
}

struct ViewInfo {
    float4x4 World;
    float4x4 View;
    float4x4 ViewProj;
    float4x4 LastViewProj;
    float4x4 InvViewProj;
};

[[vk::binding(0, 0)]] cbuffer viewInfo : register(b0, space0)
{
    ViewInfo View;
}

[[vk::binding(1, 0)]] cbuffer heightFogCB : register(b1, space0)
{
    float4 FogColor;           // rgb: fog color,       a: unused
    float4 InscatterColor;     // rgb: inscatter color,  a: unused
    float  FogDensity;         // global fog density
    float  HeightFalloff;      // fog density falloff per unit height
    float  BaseHeight;         // height where full fog starts
    float  MaxHeight;          // height above which there is no fog
    float  StartDistance;      // view distance before fog starts
    float  InscatterExponent;  // exponent for directional inscattering
    float  ClipYSign;          // +1 Vulkan (NDC Y=-1 at top), -1 DX12 (NDC Y=+1 at top)
    float  Pad0;
}

[[vk::binding(2, 0)]] Texture2D    InColor        : register(t0, space0);
[[vk::binding(3, 0)]] SamplerState InColorSampler : register(s0, space0);

[[vk::binding(4, 0)]] Texture2D    InDepth        : register(t1, space0);
[[vk::binding(5, 0)]] SamplerState InDepthSampler : register(s1, space0);

// Reconstruct world position from a depth value and screen UV.
float3 ReconstructWorldPos(float2 uv, float depth)
{
    // ClipYSign handles API differences: +1 for Vulkan (NDC Y=-1 at top),
    // -1 for DX12/Metal (NDC Y=+1 at top).
    float2 ndc    = float2(uv.x * 2.0 - 1.0, ClipYSign * (uv.y * 2.0 - 1.0));
    float4 clipPos = float4(ndc.x, ndc.y, depth, 1.0);
    float4 worldPos = mul(View.InvViewProj, clipPos);
    return worldPos.xyz / worldPos.w;
}

// Analytical integration of exponential height fog density along a view ray.
// density(h) = FogDensity * exp(-HeightFalloff * max(h - BaseHeight, 0))
float ComputeHeightFogFactor(float3 worldPos)
{
    float3 camPos = float3(View.World[0][3], View.World[1][3], View.World[2][3]);
    float3 ray    = worldPos - camPos;
    float  dist   = length(ray);

    if (dist <= StartDistance) {
        return 0.0;
    }
    float effectiveDist = dist - StartDistance;

    float camH  = camPos.y;
    float fragH = worldPos.y;
    float deltaH = fragH - camH;

    float fogIntegral;
    if (abs(deltaH) < 0.001) {
        // Horizontal ray: constant density
        float h = max(camH - BaseHeight, 0.0);
        fogIntegral = FogDensity * exp(-HeightFalloff * h) * effectiveDist;
    } else {
        // Integrate density along the ray analytically
        float h0 = max(camH  - BaseHeight, 0.0);
        float h1 = max(fragH - BaseHeight, 0.0);
        fogIntegral = FogDensity * effectiveDist *
            (exp(-HeightFalloff * h0) - exp(-HeightFalloff * h1)) /
            (HeightFalloff * abs(deltaH) + 0.0001);
    }

    // Suppress fog above MaxHeight
    float heightMask = 1.0 - saturate((fragH - MaxHeight) / (max(MaxHeight - BaseHeight, 0.001)));
    fogIntegral *= heightMask;

    return 1.0 - exp(-max(fogIntegral, 0.0));
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    float4 sceneColor = InColor.Sample(InColorSampler, input.UV);
    float  depth      = InDepth.Sample(InDepthSampler, input.UV).r;

    // Skip sky pixels (depth == 1.0 means far plane / no geometry)
    if (depth >= 1.0) {
        return sceneColor;
    }

    float3 worldPos  = ReconstructWorldPos(input.UV, depth);
    float  fogFactor = ComputeHeightFogFactor(worldPos);

    float3 finalColor = lerp(sceneColor.rgb, FogColor.rgb, fogFactor);
    return float4(finalColor, sceneColor.a);
}
