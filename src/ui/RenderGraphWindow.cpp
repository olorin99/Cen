#include <Cen/ui/RenderGraphWindow.h>
#include <imgui.h>
#include <Cen/Engine.h>

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
        for (u32 i = 0; auto& resource : renderGraph->resources()) {
            ImGui::Text("%s", resource->name.c_str());
            if (resource->type == canta::ResourceType::IMAGE) {
                auto& image = renderGraph->images()[dynamic_cast<canta::ImageResource*>(resource.get())->imageIndex];
                if (!image)
                    continue;
                ImGui::Text("\tHandle: %i", image.index());
                ImGui::Text("\tSize: %s", sizeToBytes(image->size()).c_str());
                if (ImGui::Button(std::format("Save##{}", i++).c_str())) {
                    engine->saveImageToDisk(image, std::format("{}.jpg", resource->name), canta::ImageLayout::COLOUR_ATTACHMENT);
                }
            } else {
                auto& buffer = renderGraph->buffers()[dynamic_cast<canta::BufferResource*>(resource.get())->bufferIndex];
                if (!buffer)
                    continue;
                ImGui::Text("\tHandle: %i", buffer.index());
                ImGui::Text("\tSize: %s", sizeToBytes(buffer->size()).c_str());
            }
        }
    }
    ImGui::End();
}