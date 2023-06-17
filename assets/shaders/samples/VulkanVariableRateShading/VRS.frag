#version 450
#extension GL_EXT_fragment_shading_rate : require

layout (location = 0) in vec4 color;

layout (location = 0) out vec4 outFragColor;

void main()
{
    int v = 1;
    int h = 1;

    if ((gl_ShadingRateEXT & gl_ShadingRateFlag2VerticalPixelsEXT) == gl_ShadingRateFlag2VerticalPixelsEXT)
    {
        v = 2;
    }
    if ((gl_ShadingRateEXT & gl_ShadingRateFlag4VerticalPixelsEXT) == gl_ShadingRateFlag4VerticalPixelsEXT)
    {
        v = 4;
    }
    if ((gl_ShadingRateEXT & gl_ShadingRateFlag2HorizontalPixelsEXT) == gl_ShadingRateFlag2HorizontalPixelsEXT)
    {
        h = 2;
    }
    if ((gl_ShadingRateEXT & gl_ShadingRateFlag4HorizontalPixelsEXT) == gl_ShadingRateFlag4HorizontalPixelsEXT)
    {
        h = 4;
    }

    outFragColor = vec4(vec3(h * v / 16.0), 1);
}