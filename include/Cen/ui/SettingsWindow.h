#ifndef CEN_SETTINGSWINDOW_H
#define CEN_SETTINGSWINDOW_H

#include <Cen/ui/Window.h>

namespace canta {
    class RenderGraph;
    class Swapchain;
}

namespace cen {
    class Engine;
    class Renderer;
}

namespace cen::ui {

    class SettingsWindow : public Window {
    public:

        void render() override;

        Engine* engine = nullptr;
        Renderer* renderer = nullptr;
        canta::Swapchain* swapchain = nullptr;
        bool culling = true;

    private:


    };

}

#endif //CEN_SETTINGSWINDOW_H
