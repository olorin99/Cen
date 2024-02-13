#ifndef CEN_RENDERER_H
#define CEN_RENDERER_H

#include <Canta/Device.h>
#include <Canta/RenderGraph.h>
#include <cen.glsl>

namespace cen {

    class Engine;

    namespace ui {
        class GuiWorkspace;
    }

    struct SceneInfo {
        canta::BufferHandle meshBuffer = {};
        canta::BufferHandle transformBuffer = {};
        canta::BufferHandle cameraBuffer = {};
        u32 meshCount = 0;
        u32 cameraCount = 0;
        u32 primaryCamera = 0;
        u32 cullingCamera = 0;
    };

    class Renderer {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
            canta::Format swapchainFormat = canta::Format::RGBA8_UNORM;
        };

        static auto create(CreateInfo info) -> Renderer;


        void render(const SceneInfo& sceneInfo, canta::Swapchain* swapchain, ui::GuiWorkspace* guiWorkspace);

        auto renderGraph() -> canta::RenderGraph& { return _renderGraph; }
        auto feedbackInfo() -> FeedbackInfo { return _feedbackInfo; }

        struct RenderSettings {
            bool debugMeshletId = false;
            bool debugPrimitiveId = false;
            bool debugMeshId = false;
        };
        auto renderSettings() -> RenderSettings& { return _renderSettings; }

    private:

        Engine* _engine = nullptr;
        canta::RenderGraph _renderGraph = {};

        RenderSettings _renderSettings = {};

        GlobalData _globalData = {};
        FeedbackInfo _feedbackInfo = {};

        canta::BufferHandle _globalBuffers[canta::FRAMES_IN_FLIGHT] = {};
        canta::BufferHandle _feedbackBuffers[canta::FRAMES_IN_FLIGHT] = {};

        canta::PipelineHandle _cullMeshesPipeline = {};
        canta::PipelineHandle _writeMeshletCullCommandPipeline = {};
        canta::PipelineHandle _cullMeshletsPipeline = {};
        canta::PipelineHandle _writeMeshletDrawCommandPipeline = {};
        canta::PipelineHandle _writePrimitivesPipeline = {};
        canta::PipelineHandle _drawMeshletsPipelineMeshPath = {};
        canta::PipelineHandle _drawMeshletsPipelineVertexPath = {};

    };

}

#endif //CEN_RENDERER_H
