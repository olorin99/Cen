#ifndef CEN_STATISTICSWINDOW_H
#define CEN_STATISTICSWINDOW_H

#include <Cen/ui/Window.h>
#include <cen.glsl>

namespace canta {
    class RenderGraph;
}

namespace cen {
    class Engine;
    class Renderer;
}

namespace cen::ui {

    class StatisticsWindow : public Window {
    public:

        void render() override;

        Engine* engine = nullptr;
        Renderer* renderer = nullptr;

        bool numericalStats = true;

        f32 milliseconds = 0;
        f32 dt = 0;

    };

}

#endif //CEN_STATISTICSWINDOW_H
