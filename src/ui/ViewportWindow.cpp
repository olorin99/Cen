#include <Cen/ui/ViewportWindow.h>
#include <imgui.h>
#include <SDL_mouse.h>
#include <Cen/Renderer.h>

void cen::ui::ViewportWindow::render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoBackground)) {
        i32 mouseX = 0;
        i32 mouseY = 0;
        auto mouseState = SDL_GetMouseState(&mouseX, &mouseY);
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {

            auto windowPos = ImGui::GetWindowPos();
            auto cursorPos = ImGui::GetCursorPos();
            auto availSize = ImGui::GetContentRegionAvail();
            auto handle = _image._image;
            auto imageWidth = 0;
            auto imageHeight = 0;
            if (handle) {
                imageWidth = handle->width();
                imageHeight = handle->height();
            }

            f32 u0 = ((imageWidth / 2) - (availSize.x / 2)) / imageWidth;
            f32 v0 = ((imageHeight / 2) - (availSize.y / 2)) / imageHeight;

            f32 imageOffsetWidth = imageWidth * u0;
            f32 imageOffsetHeight = imageHeight * v0;

            i32 adjustedMouseX = mouseX - windowPos.x - cursorPos.x + imageOffsetWidth;
            i32 adjustedMouseY = mouseY - windowPos.y - cursorPos.y + imageOffsetHeight;

            _renderer->renderSettings().mouseX = adjustedMouseX;
            _renderer->renderSettings().mouseY = adjustedMouseY;
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && SDL_BUTTON_LEFT == mouseState)
                _renderer->renderSettings().mousePick = true;
            else
                _renderer->renderSettings().mousePick = false;
        }


        auto availSize = ImGui::GetContentRegionAvail();
        _image.setSize(availSize.x, availSize.y);
        _image.render();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void cen::ui::ViewportWindow::setBackbuffer(canta::ImageHandle image) {
    _image._image = image;
}