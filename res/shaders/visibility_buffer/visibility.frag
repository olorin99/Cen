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

//        int x = int(gl_FragCoord.x) % 4;
//        int y = int(gl_FragCoord.y) % 4;
//        int index = x + y * 4;
        float limit = 0.8;
//        if (x < 8) {
//            if (index == 0) limit = 0.0625;
//            if (index == 1) limit = 0.5625;
//            if (index == 2) limit = 0.1875;
//            if (index == 3) limit = 0.6875;
//            if (index == 4) limit = 0.8125;
//            if (index == 5) limit = 0.3125;
//            if (index == 6) limit = 0.9375;
//            if (index == 7) limit = 0.4375;
//            if (index == 8) limit = 0.25;
//            if (index == 9) limit = 0.75;
//            if (index == 10) limit = 0.126;
//            if (index == 11) limit = 0.625;
//            if (index == 12) limit = 1.0;
//            if (index == 13) limit = 0.5;
//            if (index == 14) limit = 0.875;
//            if (index == 15) limit = 0.375;
//        }
        if (alpha < limit) {
            discard;
        }
    }

    #endif


    uint visibility = setMeshletId(fsIn.meshletId);
    visibility |= setPrimitiveId(gl_PrimitiveID);
    VisibilityInfo = visibility;
}