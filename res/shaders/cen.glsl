#ifndef CEN_INCLUDE_GLSL
#define CEN_INCLUDE_GLSL

#include <Canta/canta.glsl>

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
declareBufferReference(DispatchIndirectCommandBuffer,
    DispatchIndirectCommand command;
);

struct GPUMesh {
    uint meshletOffset;
    uint meshletCount;
    vec4 min;
    vec4 max;
    int materialId;
    uint materialOffset;
    int alphaMapIndex;
};
declareBufferReference(MeshBuffer,
    GPUMesh meshes[];
);

struct Meshlet {
    uint vertexOffset;
    uint indexOffset;
    uint indexCount;
    uint primitiveOffset;
    uint primitiveCount;
    vec3 center;
    float radius;
};
declareBufferReference(MeshletBuffer,
    Meshlet meshlets[];
);

struct MeshletInstance {
    uint meshletId;
    uint meshId;
};
declareBufferReference(MeshletInstanceBuffer,
    uint opaqueCount;
    uint alphaCount;
    MeshletInstance instances[];
);

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};
declareBufferReference(VertexBuffer,
    Vertex vertices[];
);
declareBufferReference(IndexBuffer,
    uint indices[];
);
declareBufferReference(PrimitiveBuffer,
    uint8_t primitives[];
);
declareBufferReference(MeshletIndexBuffer,
    uint indexCount;
    uint indices[];
);
declareBufferReference(TransformsBuffer,
    mat4 transforms[];
);

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
declareBufferReference(CameraBuffer,
    GPUCamera camera;
);

struct GPULight {
    vec3 position;
    uint type;
    vec3 colour;
    float intensity;
    float radius;
    int cameraIndex;
};
declareBufferReference(LightBuffer,
    GPULight light;
);

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
declareBufferReference(FeedbackInfoRef,
    FeedbackInfo info;
);

struct GlobalData {
    uint maxMeshCount;
    uint maxMeshletCount;
    uint maxIndirectIndexCount;
    uint maxLightCount;
    uvec2 screenSize;
    float exposure;
    float bloomStrength;
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
declareBufferReference(GlobalDataRef,
    GlobalData globalData;
);

#define MAX_MESHLET_INSTANCE 10000000
#define MESHLET_CLEAR_ID MAX_MESHLET_INSTANCE + 1

#endif