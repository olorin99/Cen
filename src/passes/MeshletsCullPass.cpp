#include "MeshletsCullPass.h"
#include <cen.glsl>

void cen::passes::cullMeshlets(canta::RenderGraph &graph, cen::passes::CullMeshletsParams params) {

    auto meshOutputInstanceResource = graph.addBuffer({
        .size = static_cast<u32>(sizeof(u32) + sizeof(MeshletInstance) * params.maxMeshletInstancesCount),
        .name = "mesh_output_instances"
    });

    auto meshCullingOutputClear = graph.addAlias(meshOutputInstanceResource);
    auto meshletCullingOutputClear = graph.addAlias(params.meshletInstanceBuffer);
    auto meshCommandResource = graph.addAlias(params.outputCommand);
    {
        auto& clearMeshPass = graph.addPass("clear_mesh", canta::RenderPass::Type::TRANSFER);

        clearMeshPass.addTransferWrite(meshCullingOutputClear);
        clearMeshPass.addTransferWrite(meshletCullingOutputClear);

        clearMeshPass.setExecuteFunction([meshCullingOutputClear, meshletCullingOutputClear] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshletInstanceBuffer = graph.getBuffer(meshCullingOutputClear);
            auto meshletInstanceBuffer2 = graph.getBuffer(meshletCullingOutputClear);
            cmd.clearBuffer(meshletInstanceBuffer, 0, 0, sizeof(u32));
            cmd.clearBuffer(meshletInstanceBuffer2, 0, 0, sizeof(u32));
        });
    }

    {
        auto& cullMeshPass = graph.addPass("cull_meshes", canta::RenderPass::Type::COMPUTE);

        cullMeshPass.addStorageBufferRead(meshCullingOutputClear, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(params.meshBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(params.transformBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(params.globalBuffer, canta::PipelineStage::COMPUTE_SHADER);

        cullMeshPass.addStorageBufferWrite(meshOutputInstanceResource, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferWrite(params.feedbackBuffer, canta::PipelineStage::COMPUTE_SHADER);

        cullMeshPass.setExecuteFunction([meshOutputInstanceResource, params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto transformsBuffer = graph.getBuffer(params.transformBuffer);
            auto meshBuffer = graph.getBuffer(params.meshBuffer);
            auto meshletInstanceBuffer = graph.getBuffer(meshOutputInstanceResource);
            auto cameraBuffer = graph.getBuffer(params.cameraBuffer);

            cmd.bindPipeline(params.cullMeshesPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshBuffer;
                u64 meshletInstanceBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
                i32 cameraIndex;
                i32 padding;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffer->address(),
                .meshBuffer = meshBuffer->address(),
                .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                .transformsBuffer = transformsBuffer->address(),
                .cameraBuffer = cameraBuffer->address(),
                .cameraIndex = params.cameraIndex
            });
            cmd.dispatchThreads(params.meshCount);
        });
        auto& writeMeshCommandPass = graph.addPass("write_mesh_command", canta::RenderPass::Type::COMPUTE);

        writeMeshCommandPass.addStorageBufferRead(meshOutputInstanceResource, canta::PipelineStage::COMPUTE_SHADER);
        writeMeshCommandPass.addStorageBufferWrite(meshCommandResource, canta::PipelineStage::COMPUTE_SHADER);

        writeMeshCommandPass.setExecuteFunction([params, meshOutputInstanceResource, meshCommandResource] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
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
    }

    {
        auto& cullMeshletsPass = graph.addPass("cull_meshlets", canta::RenderPass::Type::COMPUTE);

        cullMeshletsPass.addIndirectRead(meshCommandResource);
        cullMeshletsPass.addStorageBufferRead(params.globalBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(meshletCullingOutputClear, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(params.transformBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(params.meshletBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(meshOutputInstanceResource, canta::PipelineStage::COMPUTE_SHADER);

        cullMeshletsPass.addStorageBufferWrite(params.meshletInstanceBuffer, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferWrite(params.feedbackBuffer, canta::PipelineStage::COMPUTE_SHADER);

        cullMeshletsPass.setExecuteFunction([params, meshCommandResource, meshOutputInstanceResource] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto meshCommandBuffer = graph.getBuffer(meshCommandResource);
            auto transformsBuffer = graph.getBuffer(params.transformBuffer);
            auto meshletBuffer = graph.getBuffer(params.meshletBuffer);
            auto meshletInstanceInputBuffer = graph.getBuffer(meshOutputInstanceResource);
            auto meshletInstanceOutputBuffer = graph.getBuffer(params.meshletInstanceBuffer);
            auto cameraBuffer = graph.getBuffer(params.cameraBuffer);

            cmd.bindPipeline(params.culLMeshletsPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshletBuffer;
                u64 meshletInstanceInputBuffer;
                u64 meshletInstanceOutputBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
                i32 cameraIndex;
                i32 padding;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffer->address(),
                .meshletBuffer = meshletBuffer->address(),
                .meshletInstanceInputBuffer = meshletInstanceInputBuffer->address(),
                .meshletInstanceOutputBuffer = meshletInstanceOutputBuffer->address(),
                .transformsBuffer = transformsBuffer->address(),
                .cameraBuffer = cameraBuffer->address(),
                .cameraIndex = params.cameraIndex
            });
            cmd.dispatchIndirect(meshCommandBuffer, 0);
        });

        auto& writeMeshletCommandPass = graph.addPass("write_meshlet_command", canta::RenderPass::Type::COMPUTE);

        writeMeshletCommandPass.addStorageBufferRead(params.meshletInstanceBuffer, canta::PipelineStage::COMPUTE_SHADER);
        writeMeshletCommandPass.addStorageBufferWrite(params.outputCommand, canta::PipelineStage::COMPUTE_SHADER);

        writeMeshletCommandPass.setExecuteFunction([params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
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
    }
}