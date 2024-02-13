#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "cen.glsl"

layout (location = 0) out VsOut {
    vec3 colour;
} vertexOut;

layout (push_constant) uniform Push {
    GlobalDataRef globalDataRef;
    CameraBuffer cameraBuffer;
    vec3 colour;
    int cameraIndex;
};

const uint cubeIndices[] = {
    0, 1, 2, //left
    2, 1, 3,
    0, 5, 1, //top
    0, 4, 5,
    5, 7, 5, //right
    4, 6, 7,
    2, 7, 3, //bottom
    2, 6, 7,
    0, 2, 4, //near
    2, 6, 4,
    7, 5, 1, //far
    1, 3, 7
};

void main() {

    GPUCamera frustumCamera = cameraBuffer[cameraIndex].camera;
    GPUCamera camera = cameraBuffer[globalDataRef.globalData.primaryCamera].camera;

    uint index = cubeIndices[gl_VertexIndex];
    vec4 fragPos = frustumCamera.frustum.corners[index];

    vertexOut.colour = colour;

    gl_Position = camera.projection * camera.view * fragPos;
}