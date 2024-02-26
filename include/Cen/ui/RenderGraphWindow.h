#ifndef CEN_RENDERGRAPHWINDOW_H
#define CEN_RENDERGRAPHWINDOW_H

#include <Cen/ui/Window.h>
#include <Canta/RenderGraph.h>

namespace cen {
    class Engine;
}

namespace cen::ui {

    class RenderGraphWindow : public Window {
    public:

        void render() override;

        Engine* engine = nullptr;
        canta::RenderGraph* renderGraph = nullptr;

    };

}

#endif //CEN_RENDERGRAPHWINDOW_H
