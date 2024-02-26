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
    int materialId;
    uint materialOffset;
    int alphaMapIndex;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer MeshBuffer {
    GPUMesh meshes[];
};
#else
#define MeshBuffer u64
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
#else
#define MeshletBuffer u64
#endif

struct MeshletInstance {
    uint meshletId;
    uint meshId;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) buffer MeshletInstanceBuffer {
    uint opaqueCount;
    uint alphaCount;
    MeshletInstance instances[];
};
#else
#define MeshletInstanceBuffer u64
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
#else
#define VertexBuffer u64
#define IndexBuffer u64
#define PrimitiveBuffer u64
#define MeshletIndexBuffer u64
#define TransformsBuffer u64
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
#else
#define CameraBuffer u64
#endif

struct GPULight {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float radius;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer LightBuffer {
    GPULight light;
};
#else
#define LightBuffer u64
#endif

struct FeedbackInfo {
    uint meshesDrawn;
    uint meshesTotal;
    uint meshletsDrawn;
    uint meshletsTotal;
    uint trianglesDrawn;
    uint meshId;
    uint meshletId;
    uint primitiveId;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) buffer FeedbackInfoRef {
    FeedbackInfo info;
};
#else
#define FeedbackInfoRef u64
#endif

struct GlobalData {
    uint maxMeshCount;
    uint maxMeshletCount;
    uint maxIndirectIndexCount;
    uint maxLightCount;
    uvec2 screenSize;
    float exposure;
    int cullingCamera;
    int primaryCamera;
    int textureSampler;
    int depthSampler;
    MeshBuffer meshBufferRef;
    MeshletBuffer meshletBufferRef;
    VertexBuffer vertexBufferRef;
    IndexBuffer indexBufferRef;
    PrimitiveBuffer primitiveBufferRef;
    TransformsBuffer transformsBufferRef;
    CameraBuffer cameraBufferRef;
    LightBuffer lightBufferRef;
    FeedbackInfoRef feedbackInfoRef;
};
#ifndef __cplusplus
layout (scalar, buffer_reference, buffer_reference_align = 4) readonly buffer GlobalDataRef {
    GlobalData globalData;
};
#endif

#define MAX_MESHLET_INSTANCE 10000000
#define MESHLET_CLEAR_ID MAX_MESHLET_INSTANCE + 1

#endif