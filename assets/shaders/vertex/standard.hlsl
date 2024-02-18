struct VSInput
{
    [[vk::location(0)]] float4 Pos     : POSITION;
    [[vk::location(1)]] float4 Normal  : NORMAL;
    [[vk::location(2)]] float4 Tangent : TANGENT;
    [[vk::location(3)]] float4 Color   : COLOR;
    [[vk::location(4)]] float4 UV      : TEXCOORD;
};


struct VSOutput
{
    float4 Pos : SV_POSITION;

    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float3 Normal   : NORMAL0;
    [[vk::location(2)]] float4 Tangent  : TANGENT;
    [[vk::location(3)]] float4 Color    : COLOR;
    [[vk::location(4)]] float4 UV       : TEXCOORD;
};