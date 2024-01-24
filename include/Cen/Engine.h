#ifndef CEN_ENGINE_H
#define CEN_ENGINE_H

#include <Canta/Device.h>
#include <Canta/PipelineManager.h>

namespace cen {

    class Engine {
    public:

        struct CreateInfo {
            std::string_view applicationName = {};
            canta::Window* window = nullptr;
            std::filesystem::path assetPath = {};
        };
        static auto create(CreateInfo info) -> Engine;

        auto device() const -> canta::Device* { return _device.get(); }

        auto pipelineManager() -> canta::PipelineManager& { return _pipelineManager; }

    private:

        Engine() = default;

        std::unique_ptr<canta::Device> _device = {};
        canta::PipelineManager _pipelineManager = {};


    };

}

#endif //CEN_ENGINE_H
