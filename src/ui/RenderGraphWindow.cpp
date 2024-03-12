#include <Cen/ui/RenderGraphWindow.h>
#include <imgui.h>
#include <Cen/Engine.h>
#include <Cen/Renderer.h>
#include <Cen/ui/ImageWidget.h>

auto sizeToBytes(u32 size) -> std::string {
    if (size > 1000000000)
        return std::format("{} gb", size / 1000000000);
    else if (size > 1000000)
        return std::format("{} mb", size / 1000000);
    else if (size > 1000)
        return std::format("{} kb", size / 1000);
    return std::format("{} bytes", size);
}

void cen::ui::RenderGraphWindow::render() {
    if (ImGui::Begin("RenderGraph")) {
        for (auto& pass : renderGraph->orderedPasses()) {
            ImGui::Text("%s:", pass->name().data());

            ImGui::Text("\tInputs:");
            for (u32 i = 0; auto& input : pass->inputs()) {
                canta::Resource* resource = renderGraph->resources()[input.index].get();
                if (ImGui::TreeNode(std::format("{}##{}##input##{}", resource->name.c_str(), pass->name(), i++).c_str())) {
                    ImGui::Text("\t\t%s:", resource->name.c_str());
                    if (resource->type == canta::ResourceType::IMAGE) {
                        auto& image = renderGraph->images()[dynamic_cast<canta::ImageResource*>(resource)->imageIndex];
                        if (!image)
                            continue;
                        ImGui::Text("\t\t\tFormat: %s", canta::formatString(image->format()));
                        ImGui::Text("\t\t\tHandle: %i", image->defaultView().index());
                        ImGui::Text("\t\t\tSize: %s", sizeToBytes(image->size()).c_str());
                        if (ImGui::TreeNode(std::format("Show Image##{}##{}", pass->name(), resource->name).c_str())) {
                            auto availSize = ImGui::GetContentRegionAvail();
                            renderImage(image, availSize.x, availSize.y);
                            ImGui::TreePop();
                        }
                        if (ImGui::Button(std::format("Save##{}", i++).c_str())) {
                            renderer->renderSettings().screenshotImage = { .id = input.id, .index = input.index };
                            renderer->renderSettings().screenshotPath = std::format("{}.jpg", resource->name);
                        }
                    } else {
                        auto& buffer = renderGraph->buffers()[dynamic_cast<canta::BufferResource*>(resource)->bufferIndex];
                        if (!buffer)
                            continue;
                        ImGui::Text("\t\t\tHandle: %i", buffer.index());
                        ImGui::Text("\t\t\tSize: %s", sizeToBytes(buffer->size()).c_str());
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::Text("\tOutputs:");
            for (u32 i = 0; auto& output : pass->output()) {
                canta::Resource* resource = renderGraph->resources()[output.index].get();
                if (ImGui::TreeNode(std::format("{}##{}##output##{}", resource->name.c_str(), pass->name(), i++).c_str())) {
                    ImGui::Text("\t\t%s:", resource->name.c_str());
                    if (resource->type == canta::ResourceType::IMAGE) {
                        auto& image = renderGraph->images()[dynamic_cast<canta::ImageResource*>(resource)->imageIndex];
                        if (!image)
                            continue;
                        ImGui::Text("\t\t\tFormat: %s", canta::formatString(image->format()));
                        ImGui::Text("\t\t\tHandle: %i", image->defaultView().index());
                        ImGui::Text("\t\t\tSize: %s", sizeToBytes(image->size()).c_str());
                        if (ImGui::TreeNode(std::format("Show Image##{}##{}", pass->name(), resource->name).c_str())) {
                            auto availSize = ImGui::GetContentRegionAvail();
                            renderImage(image, availSize.x, availSize.y);
                            ImGui::TreePop();
                        }
                        if (ImGui::Button(std::format("Save##{}", i++).c_str())) {
                            renderer->renderSettings().screenshotImage = { .id = output.id, .index = output.index };
                            renderer->renderSettings().screenshotPath = std::format("{}.jpg", resource->name);
                        }
                    } else {
                        auto& buffer = renderGraph->buffers()[dynamic_cast<canta::BufferResource*>(resource)->bufferIndex];
                        if (!buffer)
                            continue;
                        ImGui::Text("\t\t\tHandle: %i", buffer.index());
                        ImGui::Text("\t\t\tSize: %s", sizeToBytes(buffer->size()).c_str());
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
}