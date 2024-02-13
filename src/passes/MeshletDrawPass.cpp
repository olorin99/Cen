#include "MeshletDrawPass.h"
#include <Ende/util/colour.h>

void cen::passes::drawMeshlets(canta::RenderGraph& graph, cen::passes::DrawMeshletsParams params) {
    auto drawGroup = graph.getGroup("draw_meshlets", ende::util::rgb(63, 7, 91));

    if (params.useMeshShading) {
        auto& geometryPass = graph.addPass("geometry", canta::PassType::GRAPHICS, drawGroup);

        geometryPass.addIndirectRead(params.command);

        geometryPass.addStorageBufferRead(params.globalBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.vertexBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.indexBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.primitiveBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.meshletBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.meshletInstanceBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.transformBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::MESH_SHADER);

        geometryPass.addStorageBufferWrite(params.feedbackBuffer, canta::PipelineStage::MESH_SHADER);
        geometryPass.addColourWrite(params.backbufferImage);
        geometryPass.addDepthWrite(params.depthImage);

        geometryPass.setExecuteFunction([params] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto command = graph.getBuffer(params.command);
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto vertexBuffer = graph.getBuffer(params.vertexBuffer);
            auto indexBuffer = graph.getBuffer(params.indexBuffer);
            auto primitiveBuffer = graph.getBuffer(params.primitiveBuffer);
            auto meshletBuffer = graph.getBuffer(params.meshletBuffer);
            auto meshletInstanceBuffer = graph.getBuffer(params.meshletInstanceBuffer);
            auto transformsBuffer = graph.getBuffer(params.transformBuffer);
            auto cameraBuffer = graph.getBuffer(params.cameraBuffer);

            cmd.bindPipeline(params.meshShadingPipeline);
            cmd.setViewport({ 1920, 1080 });
            struct Push {
                u64 globalDataRef;
                u64 meshletBuffer;
                u64 meshletInstanceBuffer;
                u64 vertexBuffer;
                u64 indexBuffer;
                u64 primitiveBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::MESH, Push {
                    .globalDataRef = globalBuffer->address(),
                    .meshletBuffer = meshletBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .vertexBuffer = vertexBuffer->address(),
                    .indexBuffer = indexBuffer->address(),
                    .primitiveBuffer = primitiveBuffer->address(),
                    .transformsBuffer = transformsBuffer->address(),
                    .cameraBuffer = cameraBuffer->address()
            });
            cmd.drawMeshTasksIndirect(command, 0, 1);
        });
    } else {

        auto outputIndicesIndex = graph.addBuffer({
            .size = static_cast<u32>(sizeof(u32) + params.generatedPrimitiveCount * sizeof(u32)),
            .name = "output_indices_buffer"
        });
        auto drawCommandsIndex = graph.addBuffer({
            .size = static_cast<u32>(sizeof(u32) + params.maxMeshletInstancesCount * sizeof(VkDrawIndexedIndirectCommand)),
            .name = "draw_commands_buffer"
        });

        auto& clearPass = graph.addPass("clear", canta::PassType::TRANSFER, drawGroup);

        auto outputIndicesAlias = graph.addAlias(outputIndicesIndex);
        auto drawCommandsAlias = graph.addAlias(drawCommandsIndex);

        clearPass.addTransferWrite(drawCommandsAlias);
        clearPass.addTransferWrite(outputIndicesAlias);

        clearPass.setExecuteFunction([outputIndicesAlias, drawCommandsAlias] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto outputIndicesBuffer = graph.getBuffer(outputIndicesAlias);
            auto drawCommandBuffer = graph.getBuffer(drawCommandsAlias);
            cmd.clearBuffer(outputIndicesBuffer);
            cmd.clearBuffer(drawCommandBuffer);
        });

        auto& outputIndexBufferPass = graph.addPass("output_index_buffer", canta::PassType::COMPUTE, drawGroup);

        outputIndexBufferPass.addIndirectRead(params.command);
        outputIndexBufferPass.addStorageBufferRead(params.globalBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.vertexBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.indexBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.primitiveBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.meshletBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.meshletInstanceBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(outputIndicesAlias, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(drawCommandsAlias, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.transformBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::COMPUTE_SHADER);

        outputIndexBufferPass.addStorageBufferWrite(params.feedbackBuffer, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferWrite(drawCommandsIndex, canta::PipelineStage::COMPUTE_SHADER);
        outputIndexBufferPass.addStorageBufferWrite(outputIndicesIndex, canta::PipelineStage::COMPUTE_SHADER);

        outputIndexBufferPass.setExecuteFunction([params, outputIndicesIndex, drawCommandsIndex](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto command = graph.getBuffer(params.command);
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto vertexBuffer = graph.getBuffer(params.vertexBuffer);
            auto indexBuffer = graph.getBuffer(params.indexBuffer);
            auto primitiveBuffer = graph.getBuffer(params.primitiveBuffer);
            auto meshletBuffer = graph.getBuffer(params.meshletBuffer);
            auto meshletInstanceBuffer = graph.getBuffer(params.meshletInstanceBuffer);
            auto transformsBuffer = graph.getBuffer(params.transformBuffer);
            auto cameraBuffer = graph.getBuffer(params.cameraBuffer);

            auto outputIndexBuffer = graph.getBuffer(outputIndicesIndex);
            auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);

            cmd.bindPipeline(params.writePrimitivesPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshletBuffer;
                u64 meshletInstanceBuffer;
                u64 vertexBuffer;
                u64 indexBuffer;
                u64 primitiveBuffer;
                u64 outputIndexBuffer;
                u64 drawCommandsBuffer;
                u64 transformBuffer;
                u64 cameraBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .globalDataRef = globalBuffer->address(),
                    .meshletBuffer = meshletBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .vertexBuffer = vertexBuffer->address(),
                    .indexBuffer = indexBuffer->address(),
                    .primitiveBuffer = primitiveBuffer->address(),
                    .outputIndexBuffer = outputIndexBuffer->address(),
                    .drawCommandsBuffer = drawCommandsBuffer->address(),
                    .transformBuffer = transformsBuffer->address(),
                    .cameraBuffer = cameraBuffer->address()
            });
            cmd.dispatchIndirect(command, 0);
        });

        auto& geometryPass = graph.addPass("geometry", canta::PassType::GRAPHICS, drawGroup);

        geometryPass.addIndirectRead(drawCommandsIndex);
        geometryPass.addStorageBufferRead(params.globalBuffer, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(params.vertexBuffer, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(params.indexBuffer, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(params.meshletBuffer, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(params.meshletInstanceBuffer, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(outputIndicesIndex, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(params.transformBuffer, canta::PipelineStage::VERTEX_SHADER);
        geometryPass.addStorageBufferRead(params.cameraBuffer, canta::PipelineStage::VERTEX_SHADER);

        geometryPass.addColourWrite(params.backbufferImage);
        geometryPass.addDepthWrite(params.depthImage);

        geometryPass.setExecuteFunction([params, outputIndicesIndex, drawCommandsIndex] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto vertexBuffer = graph.getBuffer(params.vertexBuffer);
            auto indexBuffer = graph.getBuffer(params.indexBuffer);
            auto meshletBuffer = graph.getBuffer(params.meshletBuffer);
            auto meshletInstanceBuffer = graph.getBuffer(params.meshletInstanceBuffer);
            auto transformsBuffer = graph.getBuffer(params.transformBuffer);
            auto cameraBuffer = graph.getBuffer(params.cameraBuffer);
            auto meshletIndexBuffer = graph.getBuffer(outputIndicesIndex);
            auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);

            cmd.bindPipeline(params.vertexPipeline);
            cmd.setViewport({ 1920, 1080 });
            struct Push {
                u64 globalDataRef;
                u64 meshletBuffer;
                u64 meshletInstanceBuffer;
                u64 vertexBuffer;
                u64 indexBuffer;
                u64 meshletIndexBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::VERTEX, Push {
                    .globalDataRef = globalBuffer->address(),
                    .meshletBuffer = meshletBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .vertexBuffer = vertexBuffer->address(),
                    .indexBuffer = indexBuffer->address(),
                    .meshletIndexBuffer = meshletIndexBuffer->address(),
                    .transformsBuffer = transformsBuffer->address(),
                    .cameraBuffer = cameraBuffer->address()
            });
            cmd.drawIndirectCount(drawCommandsBuffer, sizeof(u32), drawCommandsBuffer, 0);
        });

    }
}