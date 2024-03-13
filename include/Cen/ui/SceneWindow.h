#ifndef CEN_SCENEWINDOW_H
#define CEN_SCENEWINDOW_H

#include <Cen/ui/Window.h>
#include <Ende/platform.h>

namespace cen {
    class Scene;
    class Renderer;
}
namespace cen::ui {

    class SceneWindow : public Window {
    public:

        void render() override;

        Scene* scene = nullptr;
        Renderer* renderer = nullptr;
        i32 selectedMesh = -1;

    };

}

#endif //CEN_SCENEWINDOW_H
