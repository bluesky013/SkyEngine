struct VSInput
{
    float2 Pos   : POSITION;
    float2 UV    : UV;
    float4 Color : COLOR;
};


struct VSOutput
{
    float4 Pos : SV_POSITION;

    float2 UV    : UV;
    float4 Color : COLOR;
};

[[vk::binding(0, 1)]] cbuffer Constants : register(b0, space1)
{
    float2 Scale;
    float2 Translate;
}

[[vk::binding(1, 1)]] Texture2D FontTexture : register(t0, space1);
[[vk::binding(2, 1)]] SamplerState FontSampler : register(s0, space1);

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    output.Color    = input.Color;
    output.UV       = input.UV;

    output.Pos = float4(input.Pos * Scale + Translate, 0, 1);
    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return input.Color * FontTexture.Sample(FontSampler, input.UV);
}