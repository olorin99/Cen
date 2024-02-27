#ifndef CEN_BLOOMPASS_H
#define CEN_BLOOMPASS_H

#include <Canta/RenderGraph.h>

namespace cen::passes {

    struct BloomParams {
        i32 mips;
        u32 width;
        u32 height;
        canta::ImageIndex hdrBackbuffer;
        canta::BufferIndex globalBuffer;
        canta::SamplerHandle bilinearSampler;
        canta::PipelineHandle downsamplePipeline;
        canta::PipelineHandle upsamplePipeline;
        canta::PipelineHandle compositePipeline;
    };
    auto bloom(canta::RenderGraph& graph, BloomParams params) -> canta::ImageIndex;

}

#endif //CEN_BLOOMPASS_H
