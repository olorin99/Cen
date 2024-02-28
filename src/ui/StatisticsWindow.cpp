#include <Cen/ui/StatisticsWindow.h>
#include <imgui.h>

#include <Cen/Engine.h>
#include <Cen/Renderer.h>
#include <Canta/RenderGraph.h>

std::string numberToWord(u64 number) {
    if (number > 1000000000) {
        return std::to_string(number / 1000000000) + " billion";
    } else if (number > 1000000) {
        return std::to_string(number / 1000000) + " million";
    } else if (number > 1000) {
        return std::to_string(number / 1000) + " thousand";
    }
    return std::to_string(number);
}

void cen::ui::StatisticsWindow::render() {
    if (ImGui::Begin("Stats")) {
        ImGui::Text("Milliseconds: %f", milliseconds);
        ImGui::Text("Delta Time: %f", dt);

        auto pipelineStatistics = renderer->renderGraph().pipelineStatistics();
        for (auto& pipelineStats : pipelineStatistics) {
            if (ImGui::TreeNode(pipelineStats.first.c_str())) {
                auto stats = pipelineStats.second.result().value();
                ImGui::Text("Input Assembly Vertices: %d", stats.inputAssemblyVertices);
                ImGui::Text("Input Assembly Primitives: %d", stats.inputAssemblyPrimitives);
                ImGui::Text("Vertex Shader Invocations: %d", stats.vertexShaderInvocations);
                ImGui::Text("Geometry Shader Invocations: %d", stats.geometryShaderInvocations);
                ImGui::Text("Geometry Shader Primitives: %d", stats.geometryShaderPrimitives);
                ImGui::Text("Clipping Invocations: %d", stats.clippingInvocations);
                ImGui::Text("Clipping Primitives: %d", stats.clippingPrimitives);
                ImGui::Text("Fragment Shader Invocations: %d", stats.fragmentShaderInvocations);
                ImGui::Text("Tessellation Control Shader Patches: %d", stats.tessellationControlShaderPatches);
                ImGui::Text("Tessellation Evaluation Shader Invocations: %d", stats.tessellationEvaluationShaderInvocations);
                ImGui::Text("Compute Shader Invocations: %d", stats.computeShaderInvocations);
                ImGui::TreePop();
            }
        }

        auto feedbackInfo = renderer->feedbackInfo();
        if (ImGui::TreeNode("Feedback")) {
            ImGui::Checkbox("Numerical Stats", &numericalStats);
            if (numericalStats) {
//                ImGui::Text("Total Meshlets: %d", scene.totalMeshlets());
//                        ImGui::Text("Total Primitives: %d", scene.totalMeshlets() * primitives.size());

                ImGui::Text("Drawn Meshes: %d", feedbackInfo.meshesDrawn);
                ImGui::Text("Culled Meshes: %d", feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn);
                f32 culledMeshRatio = (static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn) / (static_cast<f32>(feedbackInfo.meshesDrawn) + static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn))) * 100;
                if (feedbackInfo.meshesDrawn + feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn == 0)
                    culledMeshRatio = 0;
                ImGui::Text("Culled mesh ratio: %.0f%%", culledMeshRatio);
                ImGui::Text("Drawn Meshlets: %d", feedbackInfo.meshletsDrawn);
                ImGui::Text("Culled Meshlets: %d", feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn);
                f32 culledMeshletRatio = (static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn) / (static_cast<f32>(feedbackInfo.meshletsDrawn) + static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn))) * 100;
                if (feedbackInfo.meshletsDrawn + feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn == 0)
                    culledMeshletRatio = 0;
                ImGui::Text("Culled meshlet ratio: %.0f%%", culledMeshletRatio);
                ImGui::Text("Drawn Triangles %d", feedbackInfo.trianglesDrawn);
            } else {
//                ImGui::Text("Total Meshlets: %s", numberToWord(scene.totalMeshlets()).c_str());
//                        ImGui::Text("Total Primitives: %s", numberToWord(scene.totalMeshlets() * primitives.size()).c_str());

                ImGui::Text("Drawn Meshes: %s", numberToWord(feedbackInfo.meshesDrawn).c_str());
                ImGui::Text("Culled Meshes: %s", numberToWord(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn).c_str());
                f32 culledMeshRatio = (static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn) / (static_cast<f32>(feedbackInfo.meshesDrawn) + static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn))) * 100;
                if (feedbackInfo.meshesDrawn + feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn == 0)
                    culledMeshRatio = 0;
                ImGui::Text("Culled mesh ratio: %.0f%%", culledMeshRatio);
                ImGui::Text("Drawn Meshlets: %s", numberToWord(feedbackInfo.meshletsDrawn).c_str());
                ImGui::Text("Culled Meshlets: %s", numberToWord(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn).c_str());
                f32 culledMeshletRatio = (static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn) / (static_cast<f32>(feedbackInfo.meshletsDrawn) + static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn))) * 100;
                if (feedbackInfo.meshletsDrawn + feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn == 0)
                    culledMeshletRatio = 0;
                ImGui::Text("Culled meshlet ratio: %.0f%%", culledMeshletRatio);
                ImGui::Text("Drawn Triangles %s", numberToWord(feedbackInfo.trianglesDrawn).c_str());
            }

            ImGui::Text("MeshId: %d", feedbackInfo.meshId);
            ImGui::Text("MeshletId: %d", feedbackInfo.meshletId);
            ImGui::Text("PrimitiveId: %d", feedbackInfo.primitiveId);

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Resource Stats")) {
            auto resourceStats = engine->device()->resourceStats();
            ImGui::Text("Shader Count %d", resourceStats.shaderCount);
            ImGui::Text("Shader Allocated %d", resourceStats.shaderAllocated);
            ImGui::Text("Pipeline Count %d", resourceStats.pipelineCount);
            ImGui::Text("Pipeline Allocated %d", resourceStats.pipelineAllocated);
            ImGui::Text("Image Count %d", resourceStats.imageCount);
            ImGui::Text("Image Allocated %d", resourceStats.imageAllocated);
            ImGui::Text("Buffer Count %d", resourceStats.bufferCount);
            ImGui::Text("Buffer Allocated %d", resourceStats.bufferAllocated);
            ImGui::Text("Sampler Count %d", resourceStats.samplerCount);
            ImGui::Text("Sampler Allocated %d", resourceStats.shaderAllocated);
            ImGui::Text("Timestamp Query Pools %d", resourceStats.timestampQueryPools);
            ImGui::Text("PipelineStats Pools %d", resourceStats.pipelineStatsPools);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("RenderGraph Stats")) {
            auto renderGraphStats = renderer->renderGraph().statistics();
            ImGui::Text("Passes: %d", renderGraphStats.passes);
            ImGui::Text("Resource: %d", renderGraphStats.resources);
            ImGui::Text("Image: %d", renderGraphStats.images);
            ImGui::Text("Buffers: %d", renderGraphStats.buffers);
            ImGui::Text("Command Buffers: %d", renderGraphStats.commandBuffers);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Memory Stats")) {
            VmaTotalStatistics statistics = {};
            vmaCalculateStatistics(engine->device()->allocator(), &statistics);
            for (auto& stats : statistics.memoryHeap) {
                if (stats.unusedRangeSizeMax == 0 && stats.allocationSizeMax == 0)
                    break;
                ImGui::Separator();
                ImGui::Text("\tAllocations: %u", stats.statistics.allocationCount);
                ImGui::Text("\tAllocations size: %lu", stats.statistics.allocationBytes / 1000000);
                ImGui::Text("\tAllocated blocks: %u", stats.statistics.blockCount);
                ImGui::Text("\tBlock size: %lu mb", stats.statistics.blockBytes / 1000000);

                ImGui::Text("\tLargest allocation: %lu mb", stats.allocationSizeMax / 1000000);
                ImGui::Text("\tSmallest allocation: %lu b", stats.allocationSizeMin);
                ImGui::Text("\tUnused range count: %u", stats.unusedRangeCount);
                ImGui::Text("\tUnused range max: %lu mb", stats.unusedRangeSizeMax / 1000000);
                ImGui::Text("\tUnused range min: %lu b", stats.unusedRangeSizeMin);
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
}