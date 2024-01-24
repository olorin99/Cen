#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (location = 0) out VsOut {
    flat uint drawId;
    flat uint meshletId;
} vertexOut;



struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer IndexBuffer {
    uint indexCount;
    uint indices[];
};

struct Frustum {
    vec4 planes[6];
    vec4 corners[8];
};
struct GPUCamera {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    Frustum frustum;
};
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer CameraBuffer {
    GPUCamera camera;
};

layout (push_constant) uniform Push {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    CameraBuffer cameraBuffer;
};

void main() {

    GPUCamera camera = cameraBuffer.camera;
    //    mat4 model = globalData.transformsBuffer.transforms[payload.meshIndex];

    uint index = indexBuffer.indices[gl_VertexIndex];
    Vertex vertex = vertexBuffer.vertices[index];

    vec4 fragPos = vec4(vertex.position, 1);

    gl_Position = camera.projection * camera.view * fragPos;
    vertexOut.drawId = 0;
    vertexOut.meshletId = gl_DrawID;
}