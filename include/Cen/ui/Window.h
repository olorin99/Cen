#ifndef CEN_WINDOW_H
#define CEN_WINDOW_H

#include <string>

namespace cen::ui {

    class Window {
    public:

        virtual ~Window() = default;

        virtual void render() {};

        std::string name = "default_window";
        bool enabled = true;

    };

}

#endif //CEN_WINDOW_H
