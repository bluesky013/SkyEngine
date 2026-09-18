//
// Built-in fullscreen blit shader for D3D12BlitEncoder::BlitImage scaling path.
// Regenerate D3D12BlitShader.h with gen/regen_blit_shader.ps1 when this changes.
//

Texture2D<float4> srcTexture : register(t0);
SamplerState srcSampler : register(s0);

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vertexId : SV_VertexID)
{
    VSOut output;
    const float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    output.uv = float2(uv.x, 1.0f - uv.y);
    return output;
}

float4 PSMain(VSOut input) : SV_Target
{
    return srcTexture.Sample(srcSampler, input.uv);
}
