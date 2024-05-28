struct VSInput
{
    [[vk::location(0)]] float4 Pos     : POSITION;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
};