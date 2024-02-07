#ifndef CEN_SETTINGSWINDOW_H
#define CEN_SETTINGSWINDOW_H

#include <Cen/ui/Window.h>

namespace canta {
    class RenderGraph;
    class Swapchain;
}

namespace cen {
    class Engine;
}

namespace cen::ui {

    class SettingsWindow : public Window {
    public:

        void render() override;

        Engine* engine = nullptr;
        canta::RenderGraph* renderGraph = nullptr;
        canta::Swapchain* swapchain = nullptr;
        bool culling = true;

    private:


    };

}

#endif //CEN_SETTINGSWINDOW_H
