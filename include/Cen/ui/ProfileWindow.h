#ifndef CEN_PROFILEWINDOW_H
#define CEN_PROFILEWINDOW_H

#include <Cen/ui/Window.h>
#include <Ende/platform.h>
#include <vector>
#include <tsl/robin_map.h>

namespace cen {
    class Renderer;
}

namespace cen::ui {

    class ProfileWindow : public Window {
    public:

        ProfileWindow();

        void render() override;

        Renderer* renderer = nullptr;
        f32 milliseconds = 0;

        std::vector<f32> _frameTime = {};
        tsl::robin_map<std::string, std::vector<f32>> _times = {};

        bool _showGraphs = true;
        bool _individualGraphs = false;
        bool _fill = false;
        bool _showLegend = true;
        bool _allowZoom = false;
        i32 _windowFrameCount = 60 * 5;
        u32 _frameOffset = 0;
    };

}

#endif //CEN_PROFILEWINDOW_H
