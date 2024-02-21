#ifndef CEN_VIEWPORTWINDOW_H
#define CEN_VIEWPORTWINDOW_H

#include <Cen/ui/Window.h>
#include <Cen/ui/ImageWidget.h>

namespace cen {
    class Renderer;
}

namespace cen::ui {

    class ViewportWindow : public Window {
    public:

        void render() override;

        void setBackbuffer(canta::ImageHandle image);

//    private:

        ImageWidget _image = {};
        Renderer* _renderer = nullptr;

    };

}

#endif //CEN_VIEWPORTWINDOW_H
