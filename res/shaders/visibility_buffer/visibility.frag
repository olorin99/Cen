#version 460

#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "cen.glsl"
#include "visibility_buffer/visibility.glsl"

layout (location = 0) in VsOut {
    flat uint drawId;
    flat uint meshletId;
} fsIn;

layout (location = 0) out uint VisibilityInfo;

void main() {
    uint visibility = setMeshletId(fsIn.meshletId);
    visibility |= setPrimitiveId(gl_PrimitiveID);
    VisibilityInfo = visibility;
}