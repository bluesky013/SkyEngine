// TSAA (Temporal Specular Anti-Aliasing) Shader
//
// Performs temporal anti-aliasing by reprojecting the previous frame's result
// using motion vectors derived from depth and view-projection matrices.
// Uses variance-based neighborhood clamping (Karis 2014) to reduce ghosting,
// and Reinhard-based tonemap weighting to suppress firefly artifacts.

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

// ---- Resource bindings ----

struct ViewInfo {
    float4x4 World;
    float4x4 View;
    float4x4 ViewProj;
    float4x4 LastViewProj;
};

[[vk::binding(0, 0)]] Texture2D InColor : register(t0, space0);
[[vk::binding(1, 0)]] SamplerState InColorSampler : register(s0, space0);
[[vk::binding(2, 0)]] Texture2D HistoryColor : register(t1, space0);
[[vk::binding(3, 0)]] SamplerState HistorySampler : register(s1, space0);
[[vk::binding(4, 0)]] Texture2D DepthTex : register(t2, space0);
[[vk::binding(5, 0)]] SamplerState DepthSampler : register(s2, space0);
[[vk::binding(6, 0)]] cbuffer view : register(b0, space0)
{
    ViewInfo View;
}
[[vk::binding(7, 0)]] cbuffer global : register(b1, space0)
{
    float4x4 LightMatrix;
    float4 MainLightColor;
    float4 MainLightDirection;
    float4 Viewport;
    float4 Jitter; // xy: jitter in UV space, zw: reserved
}

// ---- Constants ----

static const float TSAA_BLEND_MIN = 0.05;
static const float TSAA_BLEND_MAX = 0.12;
static const float TSAA_VELOCITY_WEIGHT = 20.0;

// ---- Utility functions ----

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 TonemapColor(float3 c)
{
    return c / (1.0 + Luminance(c));
}

float3 UntonemapColor(float3 c)
{
    return c / max(1.0 - Luminance(c), 1e-5);
}

// 4x4 matrix inverse using cofactor expansion
float4x4 InverseMatrix(float4x4 m)
{
    float n11 = m[0][0], n12 = m[0][1], n13 = m[0][2], n14 = m[0][3];
    float n21 = m[1][0], n22 = m[1][1], n23 = m[1][2], n24 = m[1][3];
    float n31 = m[2][0], n32 = m[2][1], n33 = m[2][2], n34 = m[2][3];
    float n41 = m[3][0], n42 = m[3][1], n43 = m[3][2], n44 = m[3][3];

    float t11 = n23*n34*n42 - n24*n33*n42 + n24*n32*n43 - n22*n34*n43 - n23*n32*n44 + n22*n33*n44;
    float t12 = n14*n33*n42 - n13*n34*n42 - n14*n32*n43 + n12*n34*n43 + n13*n32*n44 - n12*n33*n44;
    float t13 = n13*n24*n42 - n14*n23*n42 + n14*n22*n43 - n12*n24*n43 - n13*n22*n44 + n12*n23*n44;
    float t14 = n14*n23*n32 - n13*n24*n32 - n14*n22*n33 + n12*n24*n33 + n13*n22*n34 - n12*n23*n34;

    float det = n11*t11 + n21*t12 + n31*t13 + n41*t14;
    float idet = 1.0 / det;

    float4x4 ret;
    ret[0][0] = t11 * idet;
    ret[0][1] = t12 * idet;
    ret[0][2] = t13 * idet;
    ret[0][3] = t14 * idet;
    ret[1][0] = (n24*n33*n41 - n23*n34*n41 - n24*n31*n43 + n21*n34*n43 + n23*n31*n44 - n21*n33*n44) * idet;
    ret[1][1] = (n13*n34*n41 - n14*n33*n41 + n14*n31*n43 - n11*n34*n43 - n13*n31*n44 + n11*n33*n44) * idet;
    ret[1][2] = (n14*n23*n41 - n13*n24*n41 - n14*n21*n43 + n11*n24*n43 + n13*n21*n44 - n11*n23*n44) * idet;
    ret[1][3] = (n13*n24*n31 - n14*n23*n31 + n14*n21*n33 - n11*n24*n33 - n13*n21*n34 + n11*n23*n34) * idet;
    ret[2][0] = (n22*n34*n41 - n24*n32*n41 + n24*n31*n42 - n21*n34*n42 - n22*n31*n44 + n21*n32*n44) * idet;
    ret[2][1] = (n14*n32*n41 - n12*n34*n41 - n14*n31*n42 + n11*n34*n42 + n12*n31*n44 - n11*n32*n44) * idet;
    ret[2][2] = (n12*n24*n41 - n14*n22*n41 + n14*n21*n42 - n11*n24*n42 - n12*n21*n44 + n11*n22*n44) * idet;
    ret[2][3] = (n14*n22*n31 - n12*n24*n31 - n14*n21*n32 + n11*n24*n32 + n12*n21*n34 - n11*n22*n34) * idet;
    ret[3][0] = (n23*n32*n41 - n22*n33*n41 - n23*n31*n42 + n21*n33*n42 + n22*n31*n43 - n21*n32*n43) * idet;
    ret[3][1] = (n12*n33*n41 - n13*n32*n41 + n13*n31*n42 - n11*n33*n42 - n12*n31*n43 + n11*n32*n43) * idet;
    ret[3][2] = (n13*n22*n41 - n12*n23*n41 - n13*n21*n42 + n11*n23*n42 + n12*n21*n43 - n11*n22*n43) * idet;
    ret[3][3] = (n12*n23*n31 - n13*n22*n31 + n13*n21*n32 - n11*n23*n32 - n12*n21*n33 + n11*n22*n33) * idet;
    return ret;
}

// Find the closest depth in a 3x3 neighborhood (dilated motion vectors)
float2 GetClosestUV(float2 uv, float2 texelSize)
{
    float closestDepth = 0.0;
    float2 closestUV = uv;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float2 sampleUV = uv + float2(x, y) * texelSize;
            float d = DepthTex.SampleLevel(DepthSampler, sampleUV, 0).r;
            if (d > closestDepth)
            {
                closestDepth = d;
                closestUV = sampleUV;
            }
        }
    }

    return closestUV;
}

// ---- Main fragment shader ----

float4 FSMain(VSOutput input) : SV_TARGET
{
    float2 uv = input.UV;
    float2 screenSize = Viewport.zw;
    float2 texelSize = 1.0 / screenSize;

    // 1. Sample current frame color
    float3 currentColor = InColor.SampleLevel(InColorSampler, uv, 0).rgb;

    // 2. Find closest depth in 3x3 for dilated motion vectors (reduces edge artifacts)
    float2 closestUV = GetClosestUV(uv, texelSize);
    float depth = DepthTex.SampleLevel(DepthSampler, closestUV, 0).r;

    // 3. Compute motion vector: current clip -> world -> previous clip
    float4x4 invViewProj = InverseMatrix(View.ViewProj);

    float2 ndc = closestUV * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 worldPos = mul(invViewProj, float4(ndc, depth, 1.0));
    worldPos /= worldPos.w;

    float4 prevClip = mul(View.LastViewProj, worldPos);
    float2 prevNDC = prevClip.xy / prevClip.w;
    float2 prevUV = float2(prevNDC.x, -prevNDC.y) * 0.5 + 0.5;

    float2 velocity = prevUV - closestUV;

    // 4. Sample history at reprojected position
    float2 historyUV = uv + velocity;
    float3 historyColor = HistoryColor.SampleLevel(HistorySampler, historyUV, 0).rgb;

    // 5. Neighborhood clamping with variance clip (Karis 2014)
    float3 m1 = float3(0.0, 0.0, 0.0);
    float3 m2 = float3(0.0, 0.0, 0.0);
    float3 neighborMin = float3(1e10, 1e10, 1e10);
    float3 neighborMax = float3(-1e10, -1e10, -1e10);

    static const float2 offsets[9] = {
        float2(-1, -1), float2( 0, -1), float2( 1, -1),
        float2(-1,  0), float2( 0,  0), float2( 1,  0),
        float2(-1,  1), float2( 0,  1), float2( 1,  1)
    };

    [unroll]
    for (int i = 0; i < 9; i++)
    {
        float3 s = TonemapColor(InColor.SampleLevel(InColorSampler, uv + offsets[i] * texelSize, 0).rgb);
        m1 += s;
        m2 += s * s;
        neighborMin = min(neighborMin, s);
        neighborMax = max(neighborMax, s);
    }

    float3 mu = m1 / 9.0;
    float3 sigma = sqrt(abs(m2 / 9.0 - mu * mu));
    float3 boxMin = max(mu - 1.25 * sigma, neighborMin);
    float3 boxMax = min(mu + 1.25 * sigma, neighborMax);

    // Clip history to AABB in tonemapped space
    float3 historyTM = TonemapColor(historyColor);
    float3 clippedHistory = clamp(historyTM, boxMin, boxMax);
    historyColor = UntonemapColor(clippedHistory);

    // 6. Adaptive blend factor based on velocity
    float velocityMag = length(velocity * screenSize);
    float blendFactor = lerp(TSAA_BLEND_MIN, TSAA_BLEND_MAX, saturate(velocityMag * TSAA_VELOCITY_WEIGHT));

    // Reject history for out-of-screen reprojection
    if (any(historyUV < 0.0) || any(historyUV > 1.0))
    {
        blendFactor = 1.0;
    }

    // 7. Final temporal blend: result = lerp(history, current, alpha)
    float3 result = lerp(historyColor, currentColor, blendFactor);

    return float4(result, 1.0);
}
