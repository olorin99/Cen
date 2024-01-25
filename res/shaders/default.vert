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

layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer IndexBuffer {
    uint indexCount;
    uint indices[];
};

layout (push_constant) uniform Push {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    TransformsBuffer transformsBuffer;
    CameraBuffer cameraBuffer;
};

void main() {

    GPUCamera camera = cameraBuffer.camera;
    //    mat4 model = globalData.transformsBuffer.transforms[payload.meshIndex];

    uint index = indexBuffer.indices[gl_VertexIndex];
    Vertex vertex = vertexBuffer.vertices[index];

    vec4 fragPos = transformsBuffer.transforms[0] * vec4(vertex.position, 1);

    gl_Position = camera.projection * camera.view * fragPos;
    vertexOut.drawId = 0;
    vertexOut.meshletId = gl_DrawID;
}