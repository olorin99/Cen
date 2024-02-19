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
    for (u32 i = 0; auto& buffer : scene._cameraBuffer) {
        buffer = info.engine->device()->createBuffer({
            .size = 100 * sizeof(GPUCamera),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = std::format("scene_camera_buffer: {}", i++)
        });
    }

    scene._mutex = std::make_unique<std::mutex>();

    return scene;
}

void traverseNode(cen::Scene::SceneNode* node, ende::math::Mat4f worldTransform, std::vector<ende::math::Mat4f>& transforms) {
    if (node->transform.dirty())
        node->worldTransform = worldTransform * node->transform.local();

    if (node->type == cen::Scene::NodeType::MESH) {
        transforms[node->index] = node->worldTransform;
    }
    for (auto& child : node->children) {
        if (node->transform.dirty())
            child->transform.setDirty(node->transform.dirty());
        traverseNode(child.get(), node->worldTransform, transforms);
    }
    node->transform.setDirty(false);
}

void cen::Scene::prepare() {
    std::unique_lock lock(*_mutex);
    u32 flyingIndex = _engine->device()->flyingIndex();
    if (_meshBuffer[flyingIndex]->size() < _meshes.size() * sizeof(GPUMesh)) {
        _meshBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_meshes.size() * sizeof(GPUMesh))
        }, _meshBuffer[flyingIndex]);
    }
    if (_transformBuffer[flyingIndex]->size() < _worldTransforms.size() * sizeof(ende::math::Mat4f)) {
        _transformBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_worldTransforms.size() * sizeof(ende::math::Mat4f))
        }, _transformBuffer[flyingIndex]);
    }

    traverseNode(_rootNode.get(), ende::math::identity<4, f32>(), _worldTransforms);
    assert(_meshes.size() == _worldTransforms.size());

    _gpuCameras.clear();
    for (u32 cameraIndex = 0; cameraIndex < _cameras.size(); cameraIndex++) {
        getCamera(cameraIndex).updateFrustum();
        _gpuCameras.push_back(getCamera(cameraIndex).gpuCamera());
    }

    _meshBuffer[flyingIndex]->data(_meshes);
    _transformBuffer[flyingIndex]->data(_worldTransforms);

    if (_cameraBuffer[flyingIndex]->size() < _gpuCameras.size() * sizeof(GPUCamera)) {
        _cameraBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_gpuCameras.size() * sizeof(GPUCamera))
        }, _cameraBuffer[flyingIndex]);
    }
    _cameraBuffer[flyingIndex]->data(_gpuCameras);

    _meshCount = _meshes.size();
}

auto cen::Scene::addNode(std::string_view name, const cen::Transform &transform, cen::Scene::SceneNode *parent) -> SceneNode * {
    std::unique_lock lock(*_mutex);
    auto node = std::make_unique<SceneNode>();
    node->type = NodeType::NONE;
    node->transform = transform;
    node->worldTransform = transform.local();
    node->name = name;
    node->index = -1;

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

auto cen::Scene::addModel(std::string_view name, const cen::Model &model, const cen::Transform &transform, cen::Scene::SceneNode *parent) -> SceneNode * {
    auto node = addNode(name, transform, parent);
    for (auto& mesh : model.meshes) {
        addMesh(name, mesh, Transform::create({}), node);
    }
    return node;
}

auto cen::Scene::addMesh(std::string_view name, const cen::Mesh &mesh, const Transform &transform, cen::Scene::SceneNode *parent) -> SceneNode* {
    std::unique_lock lock(*_mutex);
    auto index = _meshes.size();
    _meshes.push_back({
        .meshletOffset = mesh.meshletOffset,
        .meshletCount = mesh.meshletCount,
        .min = mesh.min,
        .max = mesh.max,
        .materialId = mesh.materialInstance ? static_cast<i32>(mesh.materialInstance->material()->id()) : -1,
        .materialOffset = mesh.materialInstance ? mesh.materialInstance->offset() : 0
    });
    _worldTransforms.push_back(transform.local());

    assert(_meshes.size() == _worldTransforms.size());

    auto node = std::make_unique<SceneNode>();
    node->type = NodeType::MESH;
    node->transform = transform;
    node->worldTransform = transform.local();
    node->name = name;
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

auto cen::Scene::getMesh(cen::Scene::SceneNode *node) -> GPUMesh & {
    return _meshes[node->index];
}

auto cen::Scene::addCamera(std::string_view name, const cen::Camera &camera, const cen::Transform &transform, cen::Scene::SceneNode *parent) -> SceneNode * {
    std::unique_lock lock(*_mutex);
    auto index = _cameras.size();
    _cameras.push_back(camera);
    if (_primaryCamera < 0)
        _primaryCamera = index;
    if (_cullingCamera < 0)
        _cullingCamera = index;

    auto node = std::make_unique<SceneNode>();
    node->type = NodeType::CAMERA;
    node->transform = transform;
    node->worldTransform = transform.local();
    node->name = name;
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

auto cen::Scene::getCamera(cen::Scene::SceneNode *node) -> Camera & {
    return _cameras[node->index];
}

auto cen::Scene::getCamera(i32 index) -> Camera & {
    return _cameras[index];
}

void cen::Scene::setPrimaryCamera(cen::Scene::SceneNode *node) {
    _primaryCamera = node->index;
}

void cen::Scene::setCullingCamera(cen::Scene::SceneNode *node) {
    _cullingCamera = node->index;
}