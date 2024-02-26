#include "MeshletsCullPass.h"
#include <cen.glsl>
#include <Ende/util/colour.h>

auto cen::passes::cullMeshlets(canta::RenderGraph &graph, cen::passes::CullMeshletsParams params) -> canta::RenderPass& {
    auto cullGroup = graph.getGroup(params.name, ende::util::rgb(7, 91, 79));

    auto meshOutputInstanceResource = graph.addBuffer({
        .size = static_cast<u32>((sizeof(u32) * 2) + sizeof(MeshletInstance) * params.maxMeshletInstancesCount),
        .name = "mesh_output_instances"
    });

    auto meshCommandResource = graph.addAlias(params.outputCommand);
    auto& clearMeshPass = graph.addPass("clear_mesh", canta::PassType::TRANSFER, cullGroup)
        .addTransferWrite(meshOutputInstanceResource)
        .addTransferWrite(params.meshletInstanceBuffer)
        .setExecuteFunction([meshOutputInstanceResource, params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshletInstanceBuffer = graph.getBuffer(meshOutputInstanceResource);
            auto meshletInstanceBuffer2 = graph.getBuffer(params.meshletInstanceBuffer);
            cmd.clearBuffer(meshletInstanceBuffer, 0, 0, sizeof(u32) * 2);
            cmd.clearBuffer(meshletInstanceBuffer2, 0, 0, sizeof(u32) * 2);
        });
    if (params.read)
        clearMeshPass.addStorageImageRead(params.read.value(), canta::PipelineStage::COMPUTE_SHADER);

    auto [ meshCullingOutputClear, meshletCullingOutputClear ] = clearMeshPass.aliasBufferOutputs<2>();


    graph.addPass("cull_meshes", canta::PassType::COMPUTE, cullGroup)
        .addStorageBufferRead(meshCullingOutputClear, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.meshBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.transformBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.globalBuffer, canta::PipelineStage::COMPUTE_SHADER)

        .addStorageBufferWrite(meshOutputInstanceResource, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferWrite(params.feedbackBuffer, canta::PipelineStage::COMPUTE_SHADER)

        .setExecuteFunction([meshOutputInstanceResource, params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto meshletInstanceBuffer = graph.getBuffer(meshOutputInstanceResource);

            cmd.bindPipeline(params.cullMeshesPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshletInstanceBuffer;
                i32 cameraIndex;
                i32 testAlpha;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffer->address(),
                .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                .cameraIndex = params.cameraIndex,
                .testAlpha = params.testAlpha
            });
            cmd.dispatchThreads(params.meshCount);
        });

    graph.addPass("write_mesh_command", canta::PassType::COMPUTE, cullGroup)
        .addStorageBufferRead(meshOutputInstanceResource, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferWrite(meshCommandResource, canta::PipelineStage::COMPUTE_SHADER)

        .setExecuteFunction([params, meshOutputInstanceResource, meshCommandResource] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshletInstanceBuffer = graph.getBuffer(meshOutputInstanceResource);
            auto meshletCommandBuffer = graph.getBuffer(meshCommandResource);

            cmd.bindPipeline(params.writeMeshletCullCommandPipeline);
            struct Push {
                u64 meshletInstanceBuffer;
                u64 commandBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                .commandBuffer = meshletCommandBuffer->address()
            });
            cmd.dispatchWorkgroups();
        });



    graph.addPass("cull_meshlets", canta::PassType::COMPUTE, cullGroup)

        .addIndirectRead(meshCommandResource)
        .addStorageBufferRead(params.globalBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(meshletCullingOutputClear, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.transformBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.meshletBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferRead(meshOutputInstanceResource, canta::PipelineStage::COMPUTE_SHADER)

        .addStorageBufferWrite(params.meshletInstanceBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferWrite(params.feedbackBuffer, canta::PipelineStage::COMPUTE_SHADER)

        .setExecuteFunction([params, meshCommandResource, meshOutputInstanceResource] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto meshCommandBuffer = graph.getBuffer(meshCommandResource);
            auto meshletInstanceInputBuffer = graph.getBuffer(meshOutputInstanceResource);
            auto meshletInstanceOutputBuffer = graph.getBuffer(params.meshletInstanceBuffer);

            cmd.bindPipeline(params.culLMeshletsPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshletInstanceInputBuffer;
                u64 meshletInstanceOutputBuffer;
                i32 cameraIndex;
                i32 alpha;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffer->address(),
                .meshletInstanceInputBuffer = meshletInstanceInputBuffer->address(),
                .meshletInstanceOutputBuffer = meshletInstanceOutputBuffer->address(),
                .cameraIndex = params.cameraIndex,
                .alpha = false
            });
            cmd.dispatchIndirect(meshCommandBuffer, 0);
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffer->address(),
                .meshletInstanceInputBuffer = meshletInstanceInputBuffer->address(),
                .meshletInstanceOutputBuffer = meshletInstanceOutputBuffer->address(),
                .cameraIndex = params.cameraIndex,
                .alpha = true
            });
            cmd.dispatchIndirect(meshCommandBuffer, sizeof(DispatchIndirectCommand));
        });

    graph.addPass("write_meshlet_command", canta::PassType::COMPUTE, cullGroup)

        .addStorageBufferRead(params.meshletInstanceBuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageBufferWrite(params.outputCommand, canta::PipelineStage::COMPUTE_SHADER)

        .setExecuteFunction([params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshletInstanceBuffer = graph.getBuffer(params.meshletInstanceBuffer);
            auto meshletCommandBuffer = graph.getBuffer(params.outputCommand);

            cmd.bindPipeline(params.writeMeshletDrawCommandPipeline);
            struct Push {
                u64 meshletInstanceBuffer;
                u64 commandBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                .commandBuffer = meshletCommandBuffer->address()
            });
            cmd.dispatchWorkgroups();
        });

    return clearMeshPass;
}