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
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer DispatchIndirectCommandBuffer {
    DispatchIndirectCommand command;
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
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer MeshletBuffer {
    Meshlet meshlets[];
};
#endif

struct MeshletInstance {
    uint meshletId;
    uint meshId;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 8) buffer MeshletInstanceBuffer {
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
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer VertexBuffer {
    Vertex vertices[];
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
layout (scalar, buffer_reference, buffer_reference_align = 8) readonly buffer CameraBuffer {
    GPUCamera camera;
};
#endif

#endif