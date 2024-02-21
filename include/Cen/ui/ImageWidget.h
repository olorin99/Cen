#ifndef CEN_IMAGEWIDGET_H
#define CEN_IMAGEWIDGET_H

#include <Cen/ui/Window.h>
#include <Canta/Device.h>

namespace cen::ui {

    class ImageWidget : public Window {
    public:

        void render() override;

        void setSize(u32 width, u32 height);

        canta::ImageHandle _image = {};
        u32 _width = 0;
        u32 _height = 0;
        u32 _mode = 0;

    };

}

#endif //CEN_IMAGEWIDGET_H
