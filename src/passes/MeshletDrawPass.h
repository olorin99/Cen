#ifndef CEN_MESHLETDRAWPASS_H
#define CEN_MESHLETDRAWPASS_H

#include <Canta/RenderGraph.h>

namespace cen::passes {

    struct DrawMeshletsParams {
        canta::BufferIndex command;
        canta::BufferIndex globalBuffer;
        canta::BufferIndex vertexBuffer;
        canta::BufferIndex indexBuffer;
        canta::BufferIndex primitiveBuffer;
        canta::BufferIndex meshletBuffer;
        canta::BufferIndex meshletInstanceBuffer;
        canta::BufferIndex transformBuffer;
        canta::BufferIndex cameraBuffer;
        canta::BufferIndex feedbackBuffer;
        canta::ImageIndex backbufferImage;
        canta::ImageIndex depthImage;
        bool useMeshShading = true;
        canta::PipelineHandle meshShadingPipeline;
        canta::PipelineHandle meshShadingAlphaPipeline;
        canta::PipelineHandle writePrimitivesPipeline;
        canta::PipelineHandle vertexPipeline;
        u32 maxMeshletInstancesCount;
        u32 generatedPrimitiveCount;
        std::string_view name;
    };
    auto drawMeshlets(canta::RenderGraph& graph, DrawMeshletsParams params) -> canta::RenderPass&;

}

#endif //CEN_MESHLETDRAWPASS_H
