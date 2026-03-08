// Screen-space decal pass.
//
// Reads the scene depth buffer to reconstruct world-space positions, then for
// each active decal tests whether the position lies inside the decal OBB
// (represented as a unit cube in decal-local space).  Matching pixels receive
// the decal colour blended over the existing scene colour via src-alpha blending
// in the pipeline blend state.

#define MAX_DECALS 16

struct DecalData {
    float4x4 WorldToDecal; // Inverse world transform of the decal OBB
    float4   DecalColor;   // RGB tint + alpha
    float4   Params;       // x: blend factor, yzw: unused
};

[[vk::binding(0, 0)]] cbuffer DecalPassInfo : register(b0, space0)
{
    uint      DecalCount;
    uint3     _pad;
    DecalData Decals[MAX_DECALS];
}

// ViewInfo bound separately so this pass is independent of the default pass layout.
// Mirrors the SceneViewInfo C++ struct (world / view / viewProject / lastViewProject / invViewProject).
struct ViewInfo {
    float4x4 World;
    float4x4 View;
    float4x4 ViewProj;
    float4x4 LastViewProj;
    float4x4 InvViewProj;
};

[[vk::binding(1, 0)]] cbuffer view : register(b1, space0)
{
    ViewInfo View;
}

[[vk::binding(2, 0)]] Texture2D    DepthBuffer  : register(t0, space0);
[[vk::binding(3, 0)]] SamplerState DepthSampler : register(s0, space0);

// Full-screen triangle
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

struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD;
};

VSOutput VSMain(uint vid : SV_VertexID)
{
    VSOutput res;
    res.Pos = float4(FT_POSITIONS[vid], 0.0, 1.0);
    res.UV  = FT_UVS[vid];
    return res;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    float depth = DepthBuffer.Sample(DepthSampler, input.UV).r;

    // Discard sky / far-plane pixels (depth == 1 in non-reversed-z)
    if (depth >= 1.0) {
        discard;
    }

    // Reconstruct clip-space position and unproject to world space.
    // NDC XY maps linearly from UV: (0,0) -> (-1,-1), (1,1) -> (1,1)
    float2 ndcXY   = input.UV * 2.0 - 1.0;
    float4 clipPos = float4(ndcXY, depth, 1.0);

    float4 worldPos4 = mul(View.InvViewProj, clipPos);
    float3 worldPos  = worldPos4.xyz / worldPos4.w;

    float4 outColor = float4(0.0, 0.0, 0.0, 0.0);

    for (uint i = 0; i < DecalCount; ++i)
    {
        // Transform world position into decal local space
        float4 localPos = mul(Decals[i].WorldToDecal, float4(worldPos, 1.0));

        // The decal OBB is a unit cube [-0.5, 0.5]^3 in local space
        if (all(abs(localPos.xyz) <= 0.5))
        {
            // Map local XZ to UV [0,1]
            float2 decalUV = localPos.xz + 0.5;

            float4 decalColor = Decals[i].DecalColor;
            float  blend      = decalColor.a * Decals[i].Params.x;

            // Accumulate: later decals paint over earlier ones (painter's algorithm)
            outColor = lerp(outColor, float4(decalColor.rgb, 1.0), blend);
        }
    }

    return outColor;
}
