#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "cen.glsl"
#include "visibility_buffer/visibility.glsl"

layout (location = 0) out VsOut {
    flat uint drawId;
    flat uint meshletId;
} vertexOut;

layout (push_constant) uniform Push {
    GlobalDataRef globalDataRef;
    MeshletInstanceBuffer meshletInstanceBuffer;
    MeshletIndexBuffer meshletIndexBuffer;
};

void main() {

    GPUCamera camera = globalDataRef.globalData.cameraBufferRef[globalDataRef.globalData.primaryCamera].camera;

    uint meshletIndex = meshletIndexBuffer.indices[gl_VertexIndex];
    uint meshletId = getMeshletId(meshletIndex);
    uint primitive = getPrimitiveId(meshletIndex);
    MeshletInstance instance = meshletInstanceBuffer.instances[meshletId];
    Meshlet meshlet = globalDataRef.globalData.meshletBufferRef.meshlets[instance.meshletId];
    uint index = globalDataRef.globalData.indexBufferRef.indices[meshlet.indexOffset + primitive];
    Vertex vertex = globalDataRef.globalData.vertexBufferRef.vertices[index];

    vec4 fragPos = globalDataRef.globalData.transformsBufferRef.transforms[instance.meshId] * vec4(vertex.position, 1);

    gl_Position = camera.projection * camera.view * fragPos;
    vertexOut.drawId = instance.meshId;
    vertexOut.meshletId = meshletId;
}