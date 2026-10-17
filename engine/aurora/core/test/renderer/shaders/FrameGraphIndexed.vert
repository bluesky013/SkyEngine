#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 vertexColor;
layout(set = 0, binding = 0) uniform View { vec4 offsetDepth; } view;
layout(set = 2, binding = 0) uniform Object { vec4 scale; } object;
layout(location = 0) out vec4 color;
void main() {
    gl_Position = vec4(position * object.scale.xy + view.offsetDepth.xy, view.offsetDepth.z, 1);
    color = vertexColor;
}
