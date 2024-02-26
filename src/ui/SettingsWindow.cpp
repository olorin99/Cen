#include <Cen/ui/SettingsWindow.h>
#include <imgui.h>

#include <Cen/Engine.h>
#include <Cen/Renderer.h>
#include <Canta/RenderGraph.h>

void cen::ui::SettingsWindow::render() {
    if (ImGui::Begin("Settings")) {
        ImGui::Checkbox("Culling", &culling);

        auto meshShadingEnabled = engine->meshShadingEnabled();
        if (ImGui::Checkbox("Mesh Shading", &meshShadingEnabled))
            engine->setMeshShadingEnabled(meshShadingEnabled);

        auto timingEnabled = renderer->renderGraph().timingEnabled();
        if (ImGui::Checkbox("RenderGraph Timing", &timingEnabled))
            renderer->renderGraph().setTimingEnabled(timingEnabled);
        const char* timingModes[] = { "PER_PASS", "PER_GROUP", "SINGLE" };
        static int timingModeIndex = 0;
        if (ImGui::Combo("TimingMode", &timingModeIndex, timingModes, 3)) {
            switch (timingModeIndex) {
                case 0:
                    renderer->renderGraph().setTimingMode(canta::RenderGraph::TimingMode::PER_PASS);
                    break;
                case 1:
                    renderer->renderGraph().setTimingMode(canta::RenderGraph::TimingMode::PER_GROUP);
                    break;
                case 2:
                    renderer->renderGraph().setTimingMode(canta::RenderGraph::TimingMode::SINGLE);
                    break;
            }
        }
        auto pipelineStatsEnabled = renderer->renderGraph().pipelineStatisticsEnabled();
        if (ImGui::Checkbox("RenderGraph PiplelineStats", &pipelineStatsEnabled))
            renderer->renderGraph().setPipelineStatisticsEnabled(pipelineStatsEnabled);
        auto individualPipelineStatistics = renderer->renderGraph().individualPipelineStatistics();
        if (ImGui::Checkbox("RenderGraph Per Pass PiplelineStats", &individualPipelineStatistics))
            renderer->renderGraph().setIndividualPipelineStatistics(individualPipelineStatistics);


        const char* modes[] = { "FIFO", "MAILBOX", "IMMEDIATE" };
        static int modeIndex = 0;
        if (ImGui::Combo("PresentMode", &modeIndex, modes, 3)) {
            engine->device()->waitIdle();
            switch (modeIndex) {
                case 0:
                    swapchain->setPresentMode(canta::PresentMode::FIFO);
                    break;
                case 1:
                    swapchain->setPresentMode(canta::PresentMode::MAILBOX);
                    break;
                case 2:
                    swapchain->setPresentMode(canta::PresentMode::IMMEDIATE);
                    break;
            }
        }

        if (ImGui::Button("Reload Pipelines")) {
            engine->pipelineManager().reloadAll(true);
        }

        auto& renderSettings = renderer->renderSettings();
        const char* tonemapModes[] = { "AGX", "ACES", "REINHARD", "REINHARD2", "LOTTES", "UCHIMURA" };
        static int tonemapModeIndex = 0;
        if (ImGui::Combo("Tonemap Operator", &tonemapModeIndex, tonemapModes, 6)) {
            renderSettings.tonemapModeIndex = tonemapModeIndex;
        }
        ImGui::Checkbox("MeshletId", &renderSettings.debugMeshletId);
        ImGui::Checkbox("PrimitiveId", &renderSettings.debugPrimitiveId);
        ImGui::Checkbox("MeshId", &renderSettings.debugMeshId);
        ImGui::Checkbox("MaterialId", &renderSettings.debugMaterialId);
        ImGui::Checkbox("Wireframe Solid", &renderSettings.debugWireframe);
        ImGui::SliderInt("DebugFrustum", &renderSettings.debugFrustumIndex, -1, cameraCount - 1);
        ImGui::DragFloat("Debug Line Width", &renderSettings.debugLineWidth, 0.1);
        ImGui::ColorEdit3("Debug Colour", &renderSettings.debugColour[0]);
    }
    ImGui::End();
}