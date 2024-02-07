#include <Cen/Scene.h>
#include <Cen/Engine.h>
#include <Canta/Buffer.h>

auto cen::Scene::create(cen::Scene::CreateInfo info) -> Scene {
    Scene scene = {};

    scene._engine = info.engine;
    for (u32 i = 0; auto& buffer : scene._meshBuffer) {
        buffer = info.engine->device()->createBuffer({
            .size = 100 * sizeof(GPUMesh),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = std::format("scene_mesh_buffer: {}", i++)
        });
    }
    for (u32 i = 0; auto& buffer : scene._transformBuffer) {
        buffer = info.engine->device()->createBuffer({
            .size = 100 * sizeof(ende::math::Mat4f),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = std::format("scene_transform_buffer: {}", i++)
        });
    }

    return scene;
}

void cen::Scene::prepare() {
    u32 flyingIndex = _engine->device()->flyingIndex();
    if (_meshBuffer[flyingIndex]->size() < _meshes.size() * sizeof(GPUMesh)) {
        _meshBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_meshes.size() * sizeof(GPUMesh))
        }, _meshBuffer[flyingIndex]);
    }
    if (_transformBuffer[flyingIndex]->size() < _transforms.size() * sizeof(ende::math::Mat4f)) {
        _transformBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_transforms.size() * sizeof(ende::math::Mat4f))
        }, _transformBuffer[flyingIndex]);
    }

    _meshBuffer[flyingIndex]->data(_meshes);
    _transformBuffer[flyingIndex]->data(_transforms);
}

void cen::Scene::addMesh(const GPUMesh &mesh, const ende::math::Mat4f &transform) {
    _meshes.push_back(mesh);
    _transforms.push_back(transform);
    assert(_meshes.size() == _transforms.size());
    _maxMeshlets = std::max(_maxMeshlets, mesh.meshletCount);
    _totalMeshlets += mesh.meshletCount;
}