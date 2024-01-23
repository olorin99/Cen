#ifndef CEN_ENGINE_H
#define CEN_ENGINE_H

#include <Canta/Device.h>

namespace cen {

    class Engine {
    public:

        struct CreateInfo {
            std::string_view applicationName = {};
            canta::Window* window = nullptr;
        };
        static auto create(CreateInfo info) -> Engine;

        auto device() const -> canta::Device* { return _device.get(); }

    private:

        Engine() = default;

        std::unique_ptr<canta::Device> _device = {};


    };

}

#endif //CEN_ENGINE_H
