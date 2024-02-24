#version 460

#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "canta.glsl"
#include "cen.glsl"
#include "visibility_buffer/visibility.glsl"

#ifdef ALPHA_TEST
layout (set = 0, binding = CANTA_BINDLESS_SAMPLERS) uniform sampler samplers[];
layout (set = 0, binding = CANTA_BINDLESS_SAMPLED_IMAGES) uniform texture2D sampledImages[];
#endif

layout (location = 0) in VsOut {
    flat uint drawId;
    flat uint meshletId;
    vec2 uv;
} fsIn;

layout (push_constant) uniform Push {
    GlobalDataRef globalDataRef;
    MeshletInstanceBuffer meshletInstanceBuffer;
    int alphaPass;
    int padding;
};

layout (location = 0) out uint VisibilityInfo;

void main() {
    #ifdef ALPHA_TEST

    const GPUMesh mesh = globalDataRef.globalData.meshBufferRef.meshes[fsIn.drawId];
    if (mesh.alphaMapIndex >= 0) {
        float alpha = texture(sampler2D(sampledImages[mesh.alphaMapIndex], samplers[globalDataRef.globalData.textureSampler]), fsIn.uv).a;
        if (alpha < 0.8) {
            discard;
        }
    }

    #endif


    uint visibility = setMeshletId(fsIn.meshletId);
    visibility |= setPrimitiveId(gl_PrimitiveID);
    VisibilityInfo = visibility;
}