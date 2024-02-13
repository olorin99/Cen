#ifndef CEN_DEBUGPASSES_H
#define CEN_DEBUGPASSES_H

#include <Canta/RenderGraph.h>

namespace cen {
    class Engine;
}

namespace cen::passes {

    struct VisibilityDebugParams {
        std::string_view name;
        canta::ImageIndex visibilityBuffer;
        canta::ImageIndex backbuffer;
        canta::BufferIndex meshletInstanceBuffer;
        canta::PipelineHandle pipeline;
    };
    auto debugVisibilityBuffer(canta::RenderGraph& graph, VisibilityDebugParams params) -> canta::RenderPass&;

    struct FrustumDebugParam {
        canta::ImageIndex backbuffer;
        canta::ImageIndex depth;
        canta::BufferIndex globalBuffer;
        canta::BufferIndex cameraBuffer;
        i32 cameraIndex;
        f32 lineWidth;
        std::array<f32, 3> colour;
        cen::Engine* engine;
    };
    auto debugFrustum(canta::RenderGraph& graph, FrustumDebugParam params) -> canta::RenderPass&;

}

#endif //CEN_DEBUGPASSES_H
