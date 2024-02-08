#ifndef CEN_SCENEWINDOW_H
#define CEN_SCENEWINDOW_H

#include <Cen/ui/Window.h>

namespace cen {
    class Scene;
}
namespace cen::ui {

    class SceneWindow : public Window {
    public:

        void render() override;

        Scene* scene = nullptr;

    };

}

#endif //CEN_SCENEWINDOW_H
