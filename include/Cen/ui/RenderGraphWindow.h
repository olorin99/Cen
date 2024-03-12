#ifndef CEN_RENDERGRAPHWINDOW_H
#define CEN_RENDERGRAPHWINDOW_H

#include <Cen/ui/Window.h>
#include <Canta/RenderGraph.h>

namespace cen {
    class Engine;
    class Renderer;
}

namespace cen::ui {

    class RenderGraphWindow : public Window {
    public:

        void render() override;

        Engine* engine = nullptr;
        Renderer* renderer = nullptr;
        canta::RenderGraph* renderGraph = nullptr;

    };

}

#endif //CEN_RENDERGRAPHWINDOW_H
