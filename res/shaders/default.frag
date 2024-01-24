#version 460

#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (location = 0) in VsOut {
    flat uint drawId;
    flat uint meshletId;
} fsIn;

layout (location = 0) out vec4 OutColour;

vec3 hue2rgb(float hue) {
    hue = fract(hue);
    float r = abs(hue * 6 - 3) - 1;
    float g = 2 - abs(hue * 6 - 2);
    float b = 2 - abs(hue * 6 - 4);
    return clamp(vec3(r, g, b), vec3(0), vec3(1));
}

void main() {
    OutColour = vec4(hue2rgb(fsIn.meshletId * 1.71f), 1);
}