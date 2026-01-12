#version 450

layout (location = 0) out vec4 color;
layout (location = 0) in vec2 vTexCoords;


layout (set = 0, binding = 0) uniform sampler2D yPlaneImage;
void main() {
    color = texture(yPlaneImage, vTexCoords);
}