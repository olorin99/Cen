#include "../include/Cen/Engine.h"
#include <stb_image_write.h>

auto cen::Engine::create(CreateInfo info) -> std::unique_ptr<Engine> {
    auto engine = std::make_unique<Engine>();

    engine->_device = canta::Device::create({
        .applicationName = "Cen",
        .enableMeshShading = info.meshShadingEnabled,
        .instanceExtensions = info.window->requiredExtensions(),
    }).value();
    engine->_pipelineManager = canta::PipelineManager::create({
        .device = engine->device(),
        .rootPath = info.assetPath / "shaders"
    });
    engine->_uploadBuffer = canta::UploadBuffer::create({
        .device = engine->device(),
        .size = 1 << 20
    });
    engine->_assetManager = AssetManager::create({
        .engine = engine.get(),
        .rootPath = std::filesystem::path(CEN_SRC_DIR) / "res"
    });

    engine->_vertexBuffer = engine->device()->createBuffer({
        .size = 1 << 16,
        .usage = canta::BufferUsage::STORAGE,
        .name = "vertex_buffer"
    });
    engine->_indexBuffer = engine->device()->createBuffer({
        .size = 1 << 16,
        .usage = canta::BufferUsage::STORAGE,
        .name = "index_buffer"
    });
    engine->_primitiveBuffer = engine->device()->createBuffer({
        .size = 1 << 16,
        .usage = canta::BufferUsage::STORAGE,
        .name = "primitive_buffer"
    });
    engine->_meshletBuffer = engine->device()->createBuffer({
        .size = 1 << 16,
        .usage = canta::BufferUsage::STORAGE,
        .name = "meshlet_buffer"
    });

    engine->_meshShadingEnabled = engine->_device->meshShadersEnabled() && info.meshShadingEnabled;

    return engine;
}

void cen::Engine::gc() {
    assetManager().uploadMaterials();
    uploadBuffer().flushStagedData();
    pipelineManager().reloadAll();
    device()->gc();
}

auto cen::Engine::setMeshShadingEnabled(bool enabled) -> bool {
    _meshShadingEnabled = _device->meshShadersEnabled() && enabled;
    return _meshShadingEnabled;
}

auto cen::Engine::uploadVertexData(std::span<const Vertex> data) -> u32 {
    std::unique_lock lock(_mutex);
    if (_vertexOffset + data.size() * sizeof(Vertex) >= _vertexBuffer->size()) {
        uploadBuffer().flushStagedData().wait();
        auto newBuffer = device()->createBuffer({
            .size = static_cast<u32>(_vertexOffset + data.size() * sizeof(Vertex))
        });
        device()->immediate([&](canta::CommandBuffer& cmd) {
            cmd.copyBuffer({
                .src = _vertexBuffer,
                .dst = newBuffer,
                .srcOffset = 0,
                .dstOffset = 0,
                .size = _vertexBuffer->size()
            });
        });
        _vertexBuffer = newBuffer;
    }

    auto currentOffset = _vertexOffset;
    _vertexOffset += uploadBuffer().upload(_vertexBuffer, data, _vertexOffset);
    return currentOffset;
}

auto cen::Engine::uploadIndexData(std::span<const u32> data) -> u32 {
    std::unique_lock lock(_mutex);
    if (_indexOffset + data.size() * sizeof(u32) >= _indexBuffer->size()) {
        uploadBuffer().flushStagedData().wait();
        auto newBuffer = device()->createBuffer({
            .size = static_cast<u32>(_indexOffset + data.size() * sizeof(u32))
        });
        device()->immediate([&](canta::CommandBuffer& cmd) {
            cmd.copyBuffer({
                .src = _indexBuffer,
                .dst = newBuffer,
                .srcOffset = 0,
                .dstOffset = 0,
                .size = _indexBuffer->size()
            });
        });
        _indexBuffer = newBuffer;
    }

    auto currentOffset = _indexOffset;
    _indexOffset += uploadBuffer().upload(_indexBuffer, data, _indexOffset);
    return currentOffset;
}

auto cen::Engine::uploadPrimitiveData(std::span<const u8> data) -> u32 {
    std::unique_lock lock(_mutex);
    if (_primitiveOffset + data.size() * sizeof(u8) >= _primitiveBuffer->size()) {
        uploadBuffer().flushStagedData().wait();
        auto newBuffer = device()->createBuffer({
            .size = static_cast<u32>(_primitiveOffset + data.size() * sizeof(u8)) * 2
        });
        device()->immediate([&](canta::CommandBuffer& cmd) {
            cmd.copyBuffer({
                .src = _primitiveBuffer,
                .dst = newBuffer,
                .srcOffset = 0,
                .dstOffset = 0,
                .size = _primitiveBuffer->size()
            });
        });
        _primitiveBuffer = newBuffer;
    }

    auto currentOffset = _primitiveOffset;
    _primitiveOffset += uploadBuffer().upload(_primitiveBuffer, data, _primitiveOffset);
    return currentOffset;
}

auto cen::Engine::uploadMeshletData(std::span<const Meshlet> data) -> u32 {
    std::unique_lock lock(_mutex);
    if (_meshletOffset + data.size() * sizeof(Meshlet) >= _meshletBuffer->size()) {
        uploadBuffer().flushStagedData().wait();
        auto newBuffer = device()->createBuffer({
            .size = static_cast<u32>(_meshletOffset + data.size() * sizeof(Meshlet))
        });
        device()->immediate([&](canta::CommandBuffer& cmd) {
            cmd.copyBuffer({
                .src = _meshletBuffer,
                .dst = newBuffer,
                .srcOffset = 0,
                .dstOffset = 0,
                .size = _meshletBuffer->size()
            });
        });
        _meshletBuffer = newBuffer;
    }

    auto currentOffset = _meshletOffset;
    _meshletOffset += uploadBuffer().upload(_meshletBuffer, data, _meshletOffset);
    return currentOffset;
}

auto cen::Engine::saveImageToDisk(canta::ImageHandle image, const std::filesystem::path &path, canta::ImageLayout srcLayout, bool tonemap) -> bool {
    auto tmpBuffer = _device->createBuffer({
        .size = image->size(),
        .usage = canta::BufferUsage::TRANSFER_DST,
        .type = canta::MemoryType::READBACK,
        .persistentlyMapped = true
    });
    canta::ImageHandle tmpImage = {};
    if (tonemap) {
        tmpImage = _device->createImage({
            .width = image->width(),
            .height = image->height(),
            .format = canta::Format::RGBA8_UNORM,
            .usage = canta::ImageUsage::TRANSFER_DST | canta::ImageUsage::TRANSFER_SRC | canta::ImageUsage::STORAGE
        });
    }
    _device->immediate([&] (canta::CommandBuffer& cmd) {
        cmd.barrier({
            .image = image,
            .srcStage = canta::PipelineStage::TOP,
            .dstStage = canta::PipelineStage::TRANSFER,
            .srcAccess = canta::Access::NONE,
            .dstAccess = canta::Access::TRANSFER_READ,
            .srcLayout = srcLayout,
            .dstLayout = canta::ImageLayout::TRANSFER_SRC
        });
        if (tonemap) {
            cmd.barrier({
                .image = tmpImage,
                .srcStage = canta::PipelineStage::TOP,
                .dstStage = canta::PipelineStage::TRANSFER,
                .srcAccess = canta::Access::NONE,
                .dstAccess = canta::Access::TRANSFER_WRITE,
                .srcLayout = canta::ImageLayout::UNDEFINED,
                .dstLayout = canta::ImageLayout::TRANSFER_DST
            });
            //TODO: replace with proper tonemapping function
            cmd.blit({
                .src = image,
                .dst = tmpImage,
                .srcLayout = canta::ImageLayout::TRANSFER_SRC,
                .dstLayout = canta::ImageLayout::TRANSFER_DST
            });
            cmd.barrier({
                .image = tmpImage,
                .srcStage = canta::PipelineStage::TRANSFER,
                .dstStage = canta::PipelineStage::TRANSFER,
                .srcAccess = canta::Access::TRANSFER_WRITE,
                .dstAccess = canta::Access::TRANSFER_READ,
                .srcLayout = canta::ImageLayout::TRANSFER_DST,
                .dstLayout = canta::ImageLayout::TRANSFER_SRC
            });
        }
        cmd.copyImageToBuffer({
            .buffer = tmpBuffer,
            .image = tonemap ? tmpImage : image,
            .dstLayout = canta::ImageLayout::TRANSFER_SRC,
            .srcOffset = 0,
        });
        cmd.barrier({
            .image = image,
            .srcStage = canta::PipelineStage::TRANSFER,
            .dstStage = canta::PipelineStage::BOTTOM,
            .srcAccess = canta::Access::TRANSFER_READ,
            .dstAccess = canta::Access::NONE,
            .srcLayout = canta::ImageLayout::TRANSFER_SRC,
            .dstLayout = srcLayout
        });
    });
    if (!tonemap && canta::formatSize(image->format()) > 8)
        return stbi_write_hdr(path.c_str(), image->width(), image->height(), 4, reinterpret_cast<f32*>(tmpBuffer->mapped().address())) == 1;
    return stbi_write_jpg(path.c_str(), image->width(), image->height(), 4, tmpBuffer->mapped().address(), 100) == 1;
}