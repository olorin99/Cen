#version 460

#include "cen.glsl"

layout (location = 0) in VsOut {
    vec3 colour;
} fsIn;

layout (location = 0) out vec4 FragColour;

void main() {
    FragColour = vec4(fsIn.colour, 1.0);
}