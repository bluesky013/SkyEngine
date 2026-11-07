#include <particle/ParticleSpritePass.slangh>

struct VSOut
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID, float4 i0 : INSTANCE0, float4 i1 : INSTANCE1, float4 i2 : INSTANCE2, float4 i3 : INSTANCE3)
{
    const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(-1, 1)
    };
    float2 c = corners[id];
    float s = sin(i2.x);
    float co = cos(i2.x);
    float2 rotated = float2(c.x * co - c.y * s, c.x * s + c.y * co);
    float3 world = i0.xyz + (gParticleSprite.cameraRight.xyz * rotated.x + gParticleSprite.cameraUp.xyz * rotated.y) * i0.w;
    VSOut result;
    result.position = mul(gParticleSprite.viewProjection, float4(world, 1));
    result.color = i1;
    result.uv = i3.xy + (c * 0.5 + 0.5) * i3.zw;
    return result;
}

float4 FSMain(VSOut input) : SV_Target
{
    return gSpriteTexture.Sample(gSpriteSampler, input.uv) * input.color;
}
