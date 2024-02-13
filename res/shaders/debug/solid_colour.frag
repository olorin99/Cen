#version 460

#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "cen.glsl"

layout (location = 0) in VsOut {
    vec3 colour;
} fsIn;

layout (location = 0) out vec4 FragColour;

void main() {
    FragColour = vec4(fsIn.colour, 1.0);
}