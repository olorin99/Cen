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
        canta::BufferHandle lightBuffer = {};
        u32 meshCount = 0;
        u32 cameraCount = 0;
        u32 primaryCamera = 0;
        u32 cullingCamera = 0;
        u32 lightCount = 0;
    };

    class Renderer {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
            canta::Format swapchainFormat = canta::Format::RGBA8_UNORM;
        };

        static auto create(CreateInfo info) -> Renderer;


        auto render(const SceneInfo& sceneInfo, canta::Swapchain* swapchain, ui::GuiWorkspace* guiWorkspace) -> canta::ImageHandle;

        auto renderGraph() -> canta::RenderGraph& { return _renderGraph; }
        auto feedbackInfo() -> FeedbackInfo { return _feedbackInfo; }

        struct RenderSettings {
            bool bloom = true;
            i32 bloomMips = 5;
            f32 bloomStrength = 0.3;
            i32 tonemapModeIndex = 0;

            bool debugMeshletId = false;
            bool debugPrimitiveId = false;
            bool debugMeshId = false;
            bool debugMaterialId = false;
            bool debugWireframe = false;
            i32 debugFrustumIndex = -1;
            f32 debugLineWidth = 1;
            std::array<f32, 3> debugColour = { 1, 1, 1 };

            bool mousePick = false;
            i32 mouseX = 0;
            i32 mouseY = 0;
        };
        auto renderSettings() -> RenderSettings& { return _renderSettings; }

    private:

        Engine* _engine = nullptr;
        canta::RenderGraph _renderGraph = {};

        RenderSettings _renderSettings = {};

        GlobalData _globalData = {};
        FeedbackInfo _feedbackInfo = {};

        canta::SamplerHandle _textureSampler = {};
        canta::SamplerHandle _depthSampler = {};
        canta::SamplerHandle _bilinearSampler = {};

        canta::BufferHandle _globalBuffers[canta::FRAMES_IN_FLIGHT] = {};
        canta::BufferHandle _feedbackBuffers[canta::FRAMES_IN_FLIGHT] = {};

        canta::PipelineHandle _cullMeshesPipeline = {};
        canta::PipelineHandle _writeMeshletCullCommandPipeline = {};
        canta::PipelineHandle _cullMeshletsPipeline = {};
        canta::PipelineHandle _writeMeshletDrawCommandPipeline = {};
        canta::PipelineHandle _writePrimitivesPipeline = {};
        canta::PipelineHandle _drawMeshletsPipelineMeshPath = {};
        canta::PipelineHandle _drawMeshletsPipelineMeshAlphaPath = {};
        canta::PipelineHandle _drawMeshletsPipelineVertexPath = {};

        canta::PipelineHandle _tonemapPipeline = {};

        canta::PipelineHandle _bloomDownsamplePipeline = {};
        canta::PipelineHandle _bloomUpsamplePipeline = {};
        canta::PipelineHandle _bloomCompositePipeline = {};

    };

}

#endif //CEN_RENDERER_H
