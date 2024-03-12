#ifndef CEN_ENGINE_H
#define CEN_ENGINE_H

#include <Canta/Device.h>
#include <Canta/PipelineManager.h>
#include <Canta/UploadBuffer.h>
#include <Cen/AssetManager.h>
#include <Ende/thread/ThreadPool.h>
#include <cen.glsl>

namespace cen {

    constexpr const u32 MAX_MESHLET_VERTICES = 64;
    constexpr const u32 MAX_MESHLET_PRIMTIVES = 64;

    class Engine {
    public:

        struct CreateInfo {
            std::string_view applicationName = {};
            canta::Window* window = nullptr;
            std::filesystem::path assetPath = {};
            bool meshShadingEnabled = true;
            u32 threadCount = 1;
        };
        static auto create(CreateInfo info) -> std::unique_ptr<Engine>;

        Engine() = default;

        void gc();

        auto device() const -> canta::Device* { return _device.get(); }

        auto threadPool() -> ende::thread::ThreadPool& { return *_threadPool; }

        auto assetManager() -> AssetManager& { return _assetManager; }

        auto pipelineManager() -> canta::PipelineManager& { return _pipelineManager; }
        auto uploadBuffer() -> canta::UploadBuffer& { return _uploadBuffer; }

        auto vertexBuffer() const -> canta::BufferHandle { return _vertexBuffer; }
        auto indexBuffer() const -> canta::BufferHandle { return _indexBuffer; }
        auto primitiveBuffer() const -> canta::BufferHandle { return _primitiveBuffer; }
        auto meshletBuffer() const -> canta::BufferHandle { return _meshletBuffer; }

        auto meshShadingEnabled() const -> bool { return _meshShadingEnabled; }
        auto setMeshShadingEnabled(bool enabled) -> bool;

        auto uploadVertexData(std::span<const Vertex> data) -> u32;
        auto uploadIndexData(std::span<const u32> data) -> u32;
        auto uploadPrimitiveData(std::span<const u8> data) -> u32;
        auto uploadMeshletData(std::span<const Meshlet> data) -> u32;

        auto saveImageToDisk(canta::ImageHandle image, const std::filesystem::path& path, canta::ImageLayout srcLayout, bool tonemap = true) -> bool;

    private:

        std::unique_ptr<canta::Device> _device = {};
        std::unique_ptr<ende::thread::ThreadPool> _threadPool = {};
        canta::PipelineManager _pipelineManager = {};
        canta::UploadBuffer _uploadBuffer = {};
        AssetManager _assetManager = {};

        canta::BufferHandle _vertexBuffer = {};
        u32 _vertexOffset = 0;
        canta::BufferHandle _indexBuffer = {};
        u32 _indexOffset = 0;
        canta::BufferHandle _primitiveBuffer = {};
        u32 _primitiveOffset = 0;
        canta::BufferHandle _meshletBuffer = {};
        u32 _meshletOffset = 0;

        bool _meshShadingEnabled = true;

        std::mutex _mutex = {};

    };

}

#endif //CEN_ENGINE_H
