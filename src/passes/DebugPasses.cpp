#include "DebugPasses.h"

auto cen::passes::debugVisibilityBuffer(canta::RenderGraph &graph, cen::passes::VisibilityDebugParams params) -> canta::RenderPass& {
    auto& debugVisibilityBufferpass = graph.addPass(params.name, canta::PassType::COMPUTE);

    debugVisibilityBufferpass.addStorageImageRead(params.visibilityBuffer, canta::PipelineStage::COMPUTE_SHADER);
    debugVisibilityBufferpass.addStorageImageWrite(params.backbuffer, canta::PipelineStage::COMPUTE_SHADER);
    debugVisibilityBufferpass.addStorageBufferRead(params.meshletInstanceBuffer, canta::PipelineStage::COMPUTE_SHADER);

    debugVisibilityBufferpass.setExecuteFunction([params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
        auto visibilityBufferImage = graph.getImage(params.visibilityBuffer);
        auto backbufferImage = graph.getImage(params.backbuffer);
        auto meshletInstanceBuffer = graph.getBuffer(params.meshletInstanceBuffer);

        cmd.bindPipeline(params.pipeline);

        struct Push {
            i32 visibilityIndex;
            i32 backbufferIndex;
            u64 meshletInstanceBuffer;
        };
        cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
            .visibilityIndex = visibilityBufferImage.index(),
            .backbufferIndex = backbufferImage.index(),
            .meshletInstanceBuffer = meshletInstanceBuffer->address()
        });
        cmd.dispatchThreads(backbufferImage->width(), backbufferImage->height());
    });
    return debugVisibilityBufferpass;
}
