#include <Cen/ui/ImageWidget.h>
#include <imgui.h>

void cen::ui::renderImage(canta::ImageHandle image, u32 width, u32 height, u32 mode) {
    if (image) {
        f32 width = image->width();
        f32 height = image->height();

        f32 u0 = 0;
        f32 u1 = 1;
        f32 v0 = 0;
        f32 v1 = 1;

        if (mode == 0) {
            u0 = ((width / 2) - (static_cast<f32>(width) / 2)) / width;
            u1 = ((width / 2) + (static_cast<f32>(width) / 2)) / width;

            v0 = ((height / 2) - (static_cast<f32>(height) / 2)) / height;
            v1 = ((height / 2) + (static_cast<f32>(height) / 2)) / height;
        }

        ImGui::Image((ImTextureID)(u64)image->defaultView().index(), { static_cast<f32>(width), static_cast<f32>(height) }, { u0, v0 }, { u1, v1 });
    }
}

void cen::ui::ImageWidget::render() {
    if (_image) {
        f32 width = _image->width();
        f32 height = _image->height();

        f32 u0 = 0;
        f32 u1 = 1;
        f32 v0 = 0;
        f32 v1 = 1;

        if (_mode == 0) {
            u0 = ((width / 2) - (static_cast<f32>(_width) / 2)) / width;
            u1 = ((width / 2) + (static_cast<f32>(_width) / 2)) / width;

            v0 = ((height / 2) - (static_cast<f32>(_height) / 2)) / height;
            v1 = ((height / 2) + (static_cast<f32>(_height) / 2)) / height;
        }

        ImGui::Image((ImTextureID)(u64)_image->defaultView().index(), { static_cast<f32>(_width), static_cast<f32>(_height) }, { u0, v0 }, { u1, v1 });
    }
}

void cen::ui::ImageWidget::setSize(u32 width, u32 height) {
    _width = width;
    _height = height;
}