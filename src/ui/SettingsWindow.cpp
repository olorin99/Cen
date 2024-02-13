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
        auto individualTiming = renderer->renderGraph().individualTiming();
        if (ImGui::Checkbox("RenderGraph Per Pass Timing", &individualTiming))
            renderer->renderGraph().setIndividualTiming(individualTiming);
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
        ImGui::Checkbox("MeshletId", &renderSettings.debugMeshletId);
        ImGui::Checkbox("PrimitiveId", &renderSettings.debugPrimitiveId);
        ImGui::Checkbox("MeshId", &renderSettings.debugMeshId);

    }
    ImGui::End();
}