#ifndef CEN_MESHLETSCULLPASS_H
#define CEN_MESHLETSCULLPASS_H

#include <Canta/RenderGraph.h>

namespace cen::passes {

    struct CullMeshletsParams {
        canta::BufferIndex globalBuffer;
        canta::BufferIndex meshBuffer;
        canta::BufferIndex meshletBuffer;
        canta::BufferIndex meshletInstanceBuffer;
        canta::BufferIndex transformBuffer;
        canta::BufferIndex cameraBuffer;
        canta::BufferIndex feedbackBuffer;
        canta::BufferIndex outputCommand;
        u32 maxMeshletInstancesCount;
        u32 meshCount;
        i32 cameraIndex;
        bool testAlpha;
        canta::PipelineHandle cullMeshesPipeline;
        canta::PipelineHandle writeMeshletCullCommandPipeline;
        canta::PipelineHandle culLMeshletsPipeline;
        canta::PipelineHandle writeMeshletDrawCommandPipeline;
        std::string_view name;
        std::optional<canta::ImageIndex> read = {};
    };

    auto cullMeshlets(canta::RenderGraph& graph, CullMeshletsParams params) -> canta::RenderPass&;

}

#endif //CEN_MESHLETSCULLPASS_H
