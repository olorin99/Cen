#include <Cen/ui/AssetManagerWindow.h>
#include <Cen/AssetManager.h>
#include <Canta/PipelineManager.h>
#include <imgui.h>

void renderAssetPipeline(canta::PipelineManager& manager, canta::Pipeline::CreateInfo info, canta::PipelineHandle handle) {
    ImGui::PushID(handle.index());
    ImGui::Text("%s", info.name.data());
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (info.vertex.module)
            manager.reload(info.vertex.module);
        if (info.tesselationControl.module)
            manager.reload(info.tesselationControl.module);
        if (info.tesselationEvaluation.module)
            manager.reload(info.tesselationEvaluation.module);
        if (info.geometry.module)
            manager.reload(info.geometry.module);
        if (info.fragment.module)
            manager.reload(info.fragment.module);
        if (info.compute.module)
            manager.reload(info.compute.module);
        if (info.rayGen.module)
            manager.reload(info.rayGen.module);
        if (info.anyHit.module)
            manager.reload(info.anyHit.module);
        if (info.closestHit.module)
            manager.reload(info.closestHit.module);
        if (info.miss.module)
            manager.reload(info.miss.module);
        if (info.intersection.module)
            manager.reload(info.intersection.module);
        if (info.callable.module)
            manager.reload(info.callable.module);
        if (info.task.module)
            manager.reload(info.task.module);
        if (info.mesh.module)
            manager.reload(info.mesh.module);

        manager.reload(info);
    }
    ImGui::PopID();
}

void cen::ui::AssetManagerWindow::render() {
    i32 columnCount = 4;

    if (ImGui::Begin("AssetManager")) {
        if (ImGui::BeginTabBar("Assets")) {
            if (ImGui::BeginTabItem("Pipelines")) {
                if (ImGui::BeginTable("Pipelines", columnCount)) {
                    for (auto& [key, value] : pipelineManager->pipelines()) {
                        renderAssetPipeline(*pipelineManager, key, value);
                        ImGui::TableNextColumn();
                    }
                    ImGui::EndTable();
                }


                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Images")) {
                if (ImGui::BeginTable("Images", columnCount)) {
                    for (auto& image : assetManager->images()) {
                        if (!image) continue;
                        ImGui::PushID(image.index());
                        ImGui::TextWrapped("%s", image->name().data());
                        ImGui::Text("Handle: %d", image->defaultView().index());
                        ImGui::PopID();
                        ImGui::TableNextColumn();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Models")) {
                if (ImGui::BeginTable("Models", columnCount)) {
                    for (auto& model : assetManager->models()) {
                        ImGui::PushID(model.meshes.size());
                        ImGui::Text("ImageCount: %d", model.images.size());
                        ImGui::Text("MeshCount: %d", model.meshes.size());
                        ImGui::PopID();
                        ImGui::TableNextColumn();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Materials")) {
                if (ImGui::BeginTable("Materials", columnCount)) {
                    for (auto& material : assetManager->materials()) {
                        ImGui::PushID(material.id());
                        ImGui::Text("Material: %d", material.id());
                        ImGui::Text("Size: %d", material.size());
                        ImGui::PopID();
                        ImGui::TableNextColumn();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}