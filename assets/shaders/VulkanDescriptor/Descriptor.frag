#version 450

layout (location = 0) in vec4 color;

layout (location = 0) out vec4 outFragColor;

//layout (set = 0, binding = 0) uniform sampler   sampler0;
//layout (set = 0, binding = 1) uniform texture2D texture0;
//layout (set = 0, binding = 2) uniform sampler2D imageSampler0;
//layout (set = 0, binding = 3, rgba8) uniform readonly image2D   storageImage0;
//layout (set = 0, binding = 4, rgba8) uniform writeonly image2D  storageImage1;

layout (set = 0, binding = 5) uniform Ubo{
    vec2 position;
} ubo;

//layout (set = 0, binding = 6) buffer Storage {
//    vec4 value[];
//} sbo;

void main()
{
    vec2 coord = gl_FragCoord.xy;

    vec2 diff = coord - ubo.position;
    float dist = sqrt(diff.x * diff.x + diff.y * diff.y);

    if (dist < 10) {
        outFragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        outFragColor = color * gl_FragCoord;
    }


}