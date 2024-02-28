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

        auto pipelineStatsEnabled = renderer->renderGraph().pipelineStatisticsEnabled();
        if (ImGui::Checkbox("RenderGraph PiplelineStats", &pipelineStatsEnabled))
            renderer->renderGraph().setPipelineStatisticsEnabled(pipelineStatsEnabled);
        auto individualPipelineStatistics = renderer->renderGraph().individualPipelineStatistics();
        if (ImGui::Checkbox("RenderGraph Per Pass PiplelineStats", &individualPipelineStatistics))
            renderer->renderGraph().setIndividualPipelineStatistics(individualPipelineStatistics);


        const char* modes[] = { "FIFO", "MAILBOX", "IMMEDIATE" };
        i32 modeIndex = 0;
        switch (swapchain->getPresentMode()) {
            case canta::PresentMode::FIFO:
                modeIndex = 0;
                break;
            case canta::PresentMode::MAILBOX:
                modeIndex = 1;
                break;
            case canta::PresentMode::IMMEDIATE:
                modeIndex = 2;
                break;
        }
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
        if (ImGui::TreeNode("Bloom Settings")) {
            ImGui::Checkbox("Enable Bloom", &renderSettings.bloom);
            ImGui::SliderInt("Bloom Mips", &renderSettings.bloomMips, 1, 10);
            ImGui::SliderFloat("Bloom Strength", &renderSettings.bloomStrength, 0, 1);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Tonemap Settings")) {
            const char* tonemapModes[] = { "AGX", "ACES", "REINHARD", "REINHARD2", "LOTTES", "UCHIMURA" };
            i32 tonemapModeIndex = renderSettings.tonemapModeIndex;
            if (ImGui::Combo("Tonemap Operator", &tonemapModeIndex, tonemapModes, 6)) {
                renderSettings.tonemapModeIndex = tonemapModeIndex;
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Debug Settings")) {
            ImGui::Checkbox("MeshletId", &renderSettings.debugMeshletId);
            ImGui::Checkbox("PrimitiveId", &renderSettings.debugPrimitiveId);
            ImGui::Checkbox("MeshId", &renderSettings.debugMeshId);
            ImGui::Checkbox("MaterialId", &renderSettings.debugMaterialId);
            ImGui::Checkbox("Wireframe Solid", &renderSettings.debugWireframe);
            ImGui::SliderInt("DebugFrustum", &renderSettings.debugFrustumIndex, -1, cameraCount - 1);
            ImGui::DragFloat("Debug Line Width", &renderSettings.debugLineWidth, 0.1);
            ImGui::ColorEdit3("Debug Colour", &renderSettings.debugColour[0]);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}