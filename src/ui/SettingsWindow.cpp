#include <Cen/ui/SettingsWindow.h>
#include <imgui.h>

#include <Cen/Engine.h>
#include <Canta/RenderGraph.h>

void cen::ui::SettingsWindow::render() {
    if (ImGui::Begin("Settings")) {
        ImGui::Checkbox("Culling", &culling);

        auto meshShadingEnabled = engine->meshShadingEnabled();
        if (ImGui::Checkbox("Mesh Shading", &meshShadingEnabled))
            engine->setMeshShadingEnabled(meshShadingEnabled);

        auto timingEnabled = renderGraph->timingEnabled();
        if (ImGui::Checkbox("RenderGraph Timing", &timingEnabled))
            renderGraph->setTimingEnabled(timingEnabled);
        auto individualTiming = renderGraph->individualTiming();
        if (ImGui::Checkbox("RenderGraph Per Pass Timing", &individualTiming))
            renderGraph->setIndividualTiming(individualTiming);
        auto pipelineStatsEnabled = renderGraph->pipelineStatisticsEnabled();
        if (ImGui::Checkbox("RenderGraph PiplelineStats", &pipelineStatsEnabled))
            renderGraph->setPipelineStatisticsEnabled(pipelineStatsEnabled);
        auto individualPipelineStatistics = renderGraph->individualPipelineStatistics();
        if (ImGui::Checkbox("RenderGraph Per Pass PiplelineStats", &individualPipelineStatistics))
            renderGraph->setIndividualPipelineStatistics(individualPipelineStatistics);


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
    }
    ImGui::End();
}