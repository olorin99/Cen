#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "shaders/cen.glsl"

layout (location = 0) out VsOut {
    flat uint drawId;
    flat uint meshletId;
} vertexOut;

layout (push_constant) uniform Push {
    MeshletBuffer meshletBuffer;
    MeshletInstanceBuffer meshletInstanceBuffer;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    MeshletIndexBuffer meshletIndexBuffer;
    TransformsBuffer transformsBuffer;
    CameraBuffer cameraBuffer;
};

void main() {

    GPUCamera camera = cameraBuffer.camera;

    uint meshletIndex = meshletIndexBuffer.indices[gl_VertexIndex];
    uint meshletId = getMeshletID(meshletIndex);
    uint primitive = getPrimitiveID(meshletIndex);
    MeshletInstance instance = meshletInstanceBuffer.instances[meshletId];
    Meshlet meshlet = meshletBuffer.meshlets[instance.meshletId];
    uint index = indexBuffer.indices[meshlet.indexOffset + primitive];
    Vertex vertex = vertexBuffer.vertices[index];

    vec4 fragPos = transformsBuffer.transforms[instance.meshId] * vec4(vertex.position, 1);

    gl_Position = camera.projection * camera.view * fragPos;
    vertexOut.drawId = instance.meshId;
    vertexOut.meshletId = instance.meshletId;
}