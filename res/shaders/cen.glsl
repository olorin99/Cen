#ifndef CEN_INCLUDE_GLSL
#define CEN_INCLUDE_GLSL

#ifdef __cplusplus
#include <Ende/platform.h>
#include <Ende/math/Vec.h>
#include <Ende/math/Mat.h>

using uint = u32;
using uvec2 = ende::math::Vec<2, u32>;
using uvec3 = ende::math::Vec<3, u32>;
using uvec4 = ende::math::Vec<4, u32>;

using ivec2 = ende::math::Vec<2, i32>;
using ivec3 = ende::math::Vec<3, i32>;
using ivec4 = ende::math::Vec<4, i32>;

using vec2 = ende::math::Vec<2, f32>;
using vec3 = ende::math::Vec<3, f32>;
using vec4 = ende::math::Vec<4, f32>;

using mat2 = ende::math::Mat<2, f32>;
using mat3 = ende::math::Mat<3, f32>;
using mat4 = ende::math::Mat<4, f32>;
#endif

struct DispatchIndirectCommand {
    uint x;
    uint y;
    uint z;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer DispatchIndirectCommandBuffer {
    DispatchIndirectCommand command;
};
#endif

struct GPUMesh {
    uint meshletOffset;
    uint meshletCount;
    vec4 min;
    vec4 max;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer MeshBuffer {
    GPUMesh meshes[];
};
#endif

struct Meshlet {
    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint primitiveOffset;
    uint primitiveCount;
    vec3 center;
    float radius;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer MeshletBuffer {
    Meshlet meshlets[];
};
#endif

struct MeshletInstance {
    uint meshletId;
    uint meshId;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) buffer MeshletInstanceBuffer {
    uint count;
    MeshletInstance instances[];
};
#endif

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer IndexBuffer {
    uint indices[];
};

layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer PrimitiveBuffer {
    uint8_t primitives[];
};

layout (scalar, buffer_reference, buffer_reference_align = 4) buffer MeshletIndexBuffer {
    uint indexCount;
    uint indices[];
};

layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer TransformsBuffer {
    mat4 transforms[];
};
#endif

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
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer CameraBuffer {
    GPUCamera camera;
};
#endif

#define MESHLET_ID_BITS 26
#define PRIMITIVE_ID_BITS 6
#define MESHLET_MASK ((1u << MESHLET_ID_BITS) - 1u)
#define PRIMITIVE_MASK ((1u << PRIMITIVE_ID_BITS) - 1u)

#ifndef __cplusplus
uint setMeshletID(uint meshletID) {
    return meshletID << PRIMITIVE_ID_BITS;
}

uint setPrimitiveID(uint primitiveID) {
    return primitiveID & PRIMITIVE_MASK;
}

uint getMeshletID(uint visibility) {
    return (visibility >> PRIMITIVE_ID_BITS) & MESHLET_MASK;
}

uint getPrimitiveID(uint visibility) {
    return uint(visibility & PRIMITIVE_MASK);
}
#endif

#endif