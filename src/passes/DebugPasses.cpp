#include "DebugPasses.h"
#include <Cen/Engine.h>

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

auto cen::passes::debugFrustum(canta::RenderGraph &graph, cen::passes::FrustumDebugParam params) -> canta::RenderPass & {
    auto& debugFrustumPass = graph.addPass("debug_frustum", canta::PassType::GRAPHICS);

    debugFrustumPass.addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::VERTEX_SHADER);
    debugFrustumPass.addDepthRead(params.depth);

    debugFrustumPass.addColourWrite(params.backbuffer);

    debugFrustumPass.setExecuteFunction([params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
        auto globalBuffer = graph.getBuffer(params.globalBuffer);
        auto cameraBuffer = graph.getBuffer(params.cameraBuffer);
        auto backbuffer = graph.getImage(params.backbuffer);

        cmd.bindPipeline(params.engine->pipelineManager().getPipeline({
            .vertex = { .module = params.engine->pipelineManager().getShader({
                .path = "debug/frustum.vert",
                .stage = canta::ShaderStage::VERTEX
            })},
            .fragment = { .module = params.engine->pipelineManager().getShader({
                .path = "debug/solid_colour.frag",
                .stage = canta::ShaderStage::FRAGMENT
            })},
            .rasterState = {
                .cullMode = canta::CullMode::NONE,
                .polygonMode = canta::PolygonMode::LINE,
                .lineWidth = params.lineWidth
            },
            .depthState = {
                .test = true,
                .write = false,
                .compareOp = canta::CompareOp::GEQUAL
            },
            .colourFormats = { backbuffer->format() },
            .depthFormat = canta::Format::D32_SFLOAT
        }));

        struct Push {
            u64 globalBuffer;
            u64 cameraBuffer;
            std::array<f32, 3> colour;
            i32 cameraIndex;
        };
        cmd.pushConstants(canta::ShaderStage::VERTEX, Push {
            .globalBuffer = globalBuffer->address(),
            .cameraBuffer = cameraBuffer->address(),
            .colour = params.colour,
            .cameraIndex = params.cameraIndex
        });
        cmd.draw(36, 1, 0, 0);
    });

    return debugFrustumPass;
}