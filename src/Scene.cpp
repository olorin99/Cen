#include <Cen/Scene.h>
#include <Cen/Engine.h>
#include <Canta/Buffer.h>

auto cen::Scene::create(cen::Scene::CreateInfo info) -> Scene {
    Scene scene = {};

    scene._engine = info.engine;
    scene._rootNode = std::make_unique<SceneNode>();
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

void traverseNode(cen::Scene::SceneNode* node, ende::math::Mat4f worldTransform, std::vector<ende::math::Mat4f>& transforms) {
    worldTransform = worldTransform * node->transform;
    if (node->type == cen::Scene::NodeType::MESH) {
        transforms[node->index] = worldTransform;
    }
    for (auto& child : node->children) {
        traverseNode(child.get(), worldTransform, transforms);
    }
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

    traverseNode(_rootNode.get(), ende::math::identity<4, f32>(), _transforms);

    _meshBuffer[flyingIndex]->data(_meshes);
    _transformBuffer[flyingIndex]->data(_transforms);
}

auto cen::Scene::addMesh(const cen::Mesh &mesh, const ende::math::Mat4f &transform, cen::Scene::SceneNode *parent) -> SceneNode* {
    auto index = _meshes.size();
    _meshes.push_back({
        .meshletOffset = mesh.meshletOffset,
        .meshletCount = mesh.meshletCount,
        .min = mesh.min,
        .max = mesh.max
    });
    _transforms.push_back(transform);

    auto node = std::make_unique<SceneNode>();
    node->type = NodeType::MESH;
    node->transform = transform;
    node->index = index;

    if (parent) {
        node->parent = parent;
        parent->children.push_back(std::move(node));
        return parent->children.back().get();
    } else {
        node->parent = _rootNode.get();
        _rootNode->children.push_back(std::move(node));
        return _rootNode->children.back().get();
    }
}