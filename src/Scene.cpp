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
    for (u32 i = 0; auto& buffer : scene._lightBuffer) {
        buffer = info.engine->device()->createBuffer({
            .size = 100 * sizeof(GPULight),
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

auto cen::Scene::prepare() -> SceneInfo {
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

    _gpuLights.clear();
    for (auto& light : _lights) {
        if (light.shadowing()) {
            i32 cameraIndex = _gpuCameras.size();
            if (light.type() == Light::Type::DIRECTIONAL) {
                auto camera = Camera::create({
                    .position = light.position(),
                    .rotation = light.rotation(),
                    .fov = ende::math::rad(45),
                    .width = 100,
                    .height = 100,
                    .near = 0.1,
                    .far = 1000
                });
                _gpuCameras.push_back(camera.gpuCamera());
            } else {
                auto camera = Camera::create({
                     .position = light.position(),
                     .rotation = ende::math::Quaternion(0, 0, 0, 1),
                     .fov = ende::math::rad(45),
                     .width = 100,
                     .height = 100,
                     .near = 0.1,
                     .far = 1000
                 });
                for (u32 i = 0; i < 6; i++) {
                    auto cameraRotation = camera.rotation();
                    switch (i) {
                        case 0:
                            cameraRotation = (ende::math::Quaternion{{0, 1, 0}, ende::math::rad(90)} * cameraRotation).unit();
                            break;
                        case 1:
                            cameraRotation = (ende::math::Quaternion{{0, 1, 0}, ende::math::rad(180)} * cameraRotation).unit();
                            break;
                        case 2:
                            cameraRotation = (ende::math::Quaternion{{0, 1, 0}, ende::math::rad(90)} * cameraRotation).unit();
                            cameraRotation = (ende::math::Quaternion{{1, 0, 0}, ende::math::rad(90)} * cameraRotation).unit();
                            break;
                        case 3:
                            cameraRotation = (ende::math::Quaternion{{1, 0, 0}, ende::math::rad(180)} * cameraRotation).unit();
                            break;
                        case 4:
                            cameraRotation = (ende::math::Quaternion{{1, 0, 0}, ende::math::rad(90)} * cameraRotation).unit();
                            break;
                        case 5:
                            cameraRotation = (ende::math::Quaternion{{0, 1, 0}, ende::math::rad(180)} * cameraRotation).unit();
                            break;
                    }
                    camera.setRotation(cameraRotation);
                    _gpuCameras.push_back(camera.gpuCamera());
                }
            }
            light.setCameraIndex(cameraIndex);

        }
        _gpuLights.push_back(light.gpuLight());
    }

    _meshBuffer[flyingIndex]->data(_meshes);
    _transformBuffer[flyingIndex]->data(_worldTransforms);

    if (_cameraBuffer[flyingIndex]->size() < _gpuCameras.size() * sizeof(GPUCamera)) {
        _cameraBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_gpuCameras.size() * sizeof(GPUCamera))
        }, _cameraBuffer[flyingIndex]);
    }
    _cameraBuffer[flyingIndex]->data(_gpuCameras);

    if (_lightBuffer[flyingIndex]->size() < _gpuLights.size() * sizeof(GPULight)) {
        _lightBuffer[flyingIndex] = _engine->device()->createBuffer({
            .size = static_cast<u32>(_gpuLights.size() * sizeof(GPULight))
        }, _lightBuffer[flyingIndex]);
    }
    _lightBuffer[flyingIndex]->data(_gpuLights);

    _meshCount = _meshes.size();

    return {
        .meshBuffer = _meshBuffer[flyingIndex],
        .transformBuffer = _transformBuffer[flyingIndex],
        .cameraBuffer = _cameraBuffer[flyingIndex],
        .lightBuffer = _lightBuffer[flyingIndex],
        .meshCount = meshCount(),
        .cameraCount = static_cast<u32>(_gpuCameras.size()),
        .primaryCamera = static_cast<u32>(_primaryCamera),
        .cullingCamera = static_cast<u32>(_cullingCamera),
        .lightCount = static_cast<u32>(_gpuLights.size()),
        .sunIndex = _lights.front().type() == Light::DIRECTIONAL ? 0 : -1
    };
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
        .materialOffset = mesh.materialInstance ? mesh.materialInstance->index() : 0,
        .alphaMapIndex = mesh.alphaMapIndex
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

auto cen::Scene::addLight(std::string_view name, const cen::Light &light, const cen::Transform &transform, cen::Scene::SceneNode *parent) -> SceneNode * {
    std::unique_lock lock(*_mutex);
    auto index = _lights.size();
    _lights.push_back(light);

    auto node = std::make_unique<SceneNode>();
    node->type = NodeType::LIGHT;
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

auto cen::Scene::getLight(cen::Scene::SceneNode *node) -> Light & {
    return _lights[node->index];
}