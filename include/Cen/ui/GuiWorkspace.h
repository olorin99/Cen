#ifndef CEN_GUIWORKSPACE_H
#define CEN_GUIWORKSPACE_H

#include <vector>
#include <Cen/ui/Window.h>
#include <Canta/ImGuiContext.h>

namespace cen {
    class Engine;
}

namespace cen::ui {

    class GuiWorkspace {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
            canta::SDLWindow* window = nullptr;
        };
        static auto create(CreateInfo info) -> GuiWorkspace;

        void render();

        void addWindow(Window* window);

        auto context() -> canta::ImGuiContext& { return _context; }

    private:

        canta::ImGuiContext _context = {};

        std::vector<Window*> _windows;

    };
}


#endif //CEN_GUIWORKSPACE_H
