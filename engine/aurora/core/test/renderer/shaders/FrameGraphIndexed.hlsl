cbuffer View : register(b0, space0) { float4 offsetDepth; }
#if MATERIAL_BINDING == 0
cbuffer Material : register(b0, space1) { float4 tint; }
#else
cbuffer Material : register(b1, space1) { float4 tint; }
#endif
cbuffer Object : register(b0, space2) { float4 scale; }
struct Vertex { float2 position : POSITION; float4 color : COLOR; };
struct Varying { float4 position : SV_Position; float4 color : COLOR; };
Varying VSMain(Vertex input) {
    Varying output;
    output.position = float4(input.position * scale.xy + offsetDepth.xy, offsetDepth.z, 1);
    output.color = input.color;
    return output;
}
float4 FSMain(Varying input) : SV_Target { return input.color * tint; }
