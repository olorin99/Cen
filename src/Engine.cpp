#include "../include/Cen/Engine.h"

auto cen::Engine::create(CreateInfo info) -> std::unique_ptr<Engine> {
    auto engine = std::make_unique<Engine>();

    engine->_device = canta::Device::create({
        .applicationName = "Cen",
        .enableMeshShading = info.meshShadingEnabled,
        .instanceExtensions = info.window->requiredExtensions(),
    }).value();
    engine->_pipelineManager = canta::PipelineManager::create({
        .device = engine->device(),
        .rootPath = info.assetPath
    });
    engine->_uploadBuffer = canta::UploadBuffer::create({
        .device = engine->device(),
        .size = 1 << 16
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

auto cen::Engine::setMeshShadingEnabled(bool enabled) -> bool {
    _meshShadingEnabled = _device->meshShadersEnabled() && enabled;
    return _meshShadingEnabled;
}

auto cen::Engine::uploadVertexData(std::span<const Vertex> data) -> u32 {
    if (_vertexOffset + data.size() * sizeof(Vertex) >= _vertexBuffer->size()) {
        uploadBuffer().flushStagedData();
        uploadBuffer().wait();
        _vertexBuffer = device()->createBuffer({
            .size = static_cast<u32>(_vertexOffset + data.size() * sizeof(Vertex))
        }, _vertexBuffer);
    }

    auto currentOffset = _vertexOffset;
    _vertexOffset += uploadBuffer().upload(_vertexBuffer, data, _vertexOffset);
    return currentOffset;
}

auto cen::Engine::uploadIndexData(std::span<const u32> data) -> u32 {
    if (_indexOffset + data.size() * sizeof(u32) >= _indexBuffer->size()) {
        uploadBuffer().flushStagedData();
        uploadBuffer().wait();
        _indexBuffer = device()->createBuffer({
            .size = static_cast<u32>(_indexOffset + data.size() * sizeof(u32))
        }, _indexBuffer);
    }

    auto currentOffset = _indexOffset;
    _indexOffset += uploadBuffer().upload(_indexBuffer, data, _indexOffset);
    return currentOffset;
}

auto cen::Engine::uploadPrimitiveData(std::span<const u8> data) -> u32 {
    if (_primitiveOffset + data.size() * sizeof(u8) >= _primitiveBuffer->size()) {
        uploadBuffer().flushStagedData();
        uploadBuffer().wait();
        _primitiveBuffer = device()->createBuffer({
            .size = static_cast<u32>(_primitiveOffset + data.size() * sizeof(u8)) * 2
        }, _primitiveBuffer);
    }

    auto currentOffset = _primitiveOffset;
    _primitiveOffset += uploadBuffer().upload(_primitiveBuffer, data, _primitiveOffset);
    return currentOffset;
}

auto cen::Engine::uploadMeshletData(std::span<const Meshlet> data) -> u32 {
    if (_meshletOffset + data.size() * sizeof(Meshlet) >= _meshletBuffer->size()) {
        uploadBuffer().flushStagedData();
        uploadBuffer().wait();
        _meshletBuffer = device()->createBuffer({
            .size = static_cast<u32>(_meshletOffset + data.size() * sizeof(Meshlet))
        }, _meshletBuffer);
    }

    auto currentOffset = _meshletOffset;
    _meshletOffset += uploadBuffer().upload(_meshletBuffer, data, _meshletOffset);
    return currentOffset;
}