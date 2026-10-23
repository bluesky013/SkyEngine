// Shared source for the runtime UI renderer and offline shader compilation tests.

#pragma once

namespace sky::ui::detail {

    // Shape draws carry untransformed layout position in uv, so their SDF
    // remains correct when the quad is translated, scaled, rotated or mirrored.
    inline constexpr char UI_SHADER_SOURCE[] = R"UI_SHADER(
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int set; int binding; string owner; }
[AuroraResource(0, 0, "ui/UIShader")]
Texture2D<float4> tex : register(t0, space0);
[AuroraResource(0, 1, "ui/UIShader")]
SamplerState smp : register(s1, space0);
struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 color : COLOR0; float4 roundRect : TEXCOORD1; float radius : TEXCOORD2; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; float4 roundRect : TEXCOORD1; float radius : TEXCOORD2; };
[shader("vertex")]
VSOut vs_main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos, 0.0, 1.0);
    o.uv = i.uv;
    o.color = i.color;
    o.roundRect = i.roundRect;
    o.radius = i.radius;
    return o;
}
[shader("fragment")]
float4 fs_main(VSOut i) : SV_Target {
    return i.color * tex.Sample(smp, i.uv);
}
[shader("fragment")]
float4 fs_round(VSOut i) : SV_Target {
    float2 p = i.uv - i.roundRect.xy;
    float2 d = abs(p) - (i.roundRect.zw - i.radius);
    float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - i.radius;
    float w = max(fwidth(sd), 1e-4);
    float a = clamp(0.5 - sd / w, 0.0, 1.0);
    return float4(i.color.rgb, i.color.a * a);
}
)UI_SHADER";

} // namespace sky::ui::detail
