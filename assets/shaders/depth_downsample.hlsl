#include "vertex/full_screen.hlslh"

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

#include "depth/depth.hlslh"
[[vk::binding(0, 0)]] cbuffer global : register(b0, space0) {
    float2 InvSize;
    float2 InputViewportMaxBound;
}

[[vk::binding(1, 0)]] Texture2D InDepth : register(t0, space0);
[[vk::binding(2, 0)]] SamplerState InDepthSampler : register(s0, space0);

float4 FSMain(VSOutput input) : SV_TARGET
{
    float2 baseUV = input.UV;

    float4 depth;
    depth.x = InDepth.Sample(InDepthSampler, baseUV + InvSize * float2(-0.25, -0.25)).r;
    depth.y = InDepth.Sample(InDepthSampler, baseUV + InvSize * float2( 0.25, -0.25)).r;
    depth.z = InDepth.Sample(InDepthSampler, baseUV + InvSize * float2(-0.25,  0.25)).r;
    depth.w = InDepth.Sample(InDepthSampler, baseUV + InvSize * float2( 0.25,  0.25)).r;

	float fFurthestZ = max(max(depth.x, depth.y), max(depth.z, depth.w));
	return float4(fFurthestZ, 0, 0, 0);
}