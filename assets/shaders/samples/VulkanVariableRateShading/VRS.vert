#version 450
#extension GL_EXT_fragment_shading_rate : require

layout(location = 0) out vec4 outColor;

vec2 positions[3] = vec2[](
vec2(-1.0, -1.0),
vec2( 1.0, -1.0),
vec2( 0.0,  1.0)
);

vec4 color[3] = vec4[](
vec4(1.0, 0.0, 0.0, 1.0),
vec4(0.0, 1.0, 0.0, 1.0),
vec4(0.0, 0.0, 1.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
//    gl_PrimitiveShadingRateEXT = gl_ShadingRateFlag2VerticalPixelsEXT | gl_ShadingRateFlag2HorizontalPixelsEXT;
//    gl_PrimitiveShadingRateEXT = gl_ShadingRateFlag2VerticalPixelsEXT | gl_ShadingRateFlag4HorizontalPixelsEXT;
//    gl_PrimitiveShadingRateEXT = gl_ShadingRateFlag4VerticalPixelsEXT | gl_ShadingRateFlag2HorizontalPixelsEXT;
//    gl_PrimitiveShadingRateEXT = gl_ShadingRateFlag4VerticalPixelsEXT | gl_ShadingRateFlag4HorizontalPixelsEXT;
    outColor = color[gl_VertexIndex];
}