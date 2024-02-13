#ifndef CEN_DEBUGPASSES_H
#define CEN_DEBUGPASSES_H

#include <Canta/RenderGraph.h>

namespace cen::passes {

    struct VisibilityDebugParams {
        std::string_view name;
        canta::ImageIndex visibilityBuffer;
        canta::ImageIndex backbuffer;
        canta::BufferIndex meshletInstanceBuffer;
        canta::PipelineHandle pipeline;
    };
    auto debugVisibilityBuffer(canta::RenderGraph& graph, VisibilityDebugParams params) -> canta::RenderPass&;

}

#endif //CEN_DEBUGPASSES_H
