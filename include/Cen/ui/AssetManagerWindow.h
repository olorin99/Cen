#ifndef CEN_ASSETMANAGERWINDOW_H
#define CEN_ASSETMANAGERWINDOW_H

#include <Cen/ui/Window.h>

namespace canta {
    class PipelineManager;
}

namespace cen {
    class AssetManager;
}

namespace cen::ui {

    class AssetManagerWindow : public Window {
    public:

        void render() override;

        AssetManager* assetManager = nullptr;
        canta::PipelineManager* pipelineManager = nullptr;
    };

}

#endif //CEN_ASSETMANAGERWINDOW_H
