#include <Cen/AssetManager.h>
#include <Cen/Engine.h>

#include <Ende/math/Quaternion.h>
#include <Ende/filesystem/File.h>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <meshoptimizer.h>
#include <stb_image.h>
#include <rapidjson/document.h>
#include <stack>
#include <span>
#include <cen.glsl>

template <>
cen::Model &cen::Asset<cen::Model>::operator*() {
    return _manager->_models[_manager->_metadata[_index].index];
}

template <>
cen::Model *cen::Asset<cen::Model>::operator->() {
    return &_manager->_models[_manager->_metadata[_index].index];
}

template <>
cen::Material &cen::Asset<cen::Material>::operator*() {
    return _manager->_materials[_manager->_metadata[_index].index];
}

template <>
cen::Material *cen::Asset<cen::Material>::operator->() {
    return &_manager->_materials[_manager->_metadata[_index].index];
}

template <>
struct fastgltf::ElementTraits<ende::math::Vec3f> : fastgltf::ElementTraitsBase<ende::math::Vec3f, AccessorType::Vec3, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec<2, f32>> : fastgltf::ElementTraitsBase<ende::math::Vec<2, f32>, AccessorType::Vec2, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec4f> : fastgltf::ElementTraitsBase<ende::math::Vec4f, AccessorType::Vec4, float> {};

auto findFile(const std::filesystem::path& path, std::span<const std::filesystem::path> searchPaths) -> std::expected<std::filesystem::path, int> {
    if (std::filesystem::exists(path))
        return path;
    for (auto& searchPath : searchPaths) {
        if (std::filesystem::exists(searchPath / path))
            return searchPath / path;
    }
    return std::unexpected(-1);
}


auto cen::AssetManager::create(cen::AssetManager::CreateInfo info) -> AssetManager {
    AssetManager manager = {};

    manager._engine = info.engine;
    manager._rootPath = info.rootPath;
    manager._models.reserve(5);

    return manager;
}

auto cen::AssetManager::getAssetIndex(u32 hash) -> i32 {
    auto it = _assetMap.find(hash);
    if (it == _assetMap.end())
        return -1;
    return it->second;
}

auto cen::AssetManager::registerAsset(u32 hash, const std::filesystem::path &path, const std::string &name, AssetType type) -> i32 {
    auto index = getAssetIndex(hash);
    if (index >= 0)
        return index;

    index = _metadata.size();

    i32 dataIndex = -1;
    switch (type) {
        case AssetType::IMAGE:
            dataIndex = _images.size();
            _images.push_back({});
            break;
        case AssetType::MODEL:
            dataIndex = _models.size();
            _models.push_back({});
            break;
        case AssetType::MATERIAL:
            dataIndex = _materials.size();
            _materials.push_back({});
            break;
    }

    _assetMap.insert(std::make_pair(hash, index));
    _metadata.push_back({
        hash,
        name,
        path,
        false,
        type,
        dataIndex
    });
    return index;
}

auto cen::AssetManager::loadImage(const std::filesystem::path &path, canta::Format format) -> canta::ImageHandle {
    auto hash = std::hash<std::filesystem::path>()(absolute(path));
    auto index = getAssetIndex(hash);
    if (index < 0)
        index = registerAsset(hash, path, "", AssetType::IMAGE);

    assert(index < _metadata.size());
    assert(_metadata[index].type == AssetType::IMAGE);
    if (_metadata[index].loaded) {
        assert(_metadata[index].index < _images.size());
        return _images[_metadata[index].index];
    }

    i32 width, height, channels;
    u8* data = nullptr;
    u32 length = 0;
    if (canta::formatSize(format) > 4) {
        f32* hdrData = stbi_loadf(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        data = reinterpret_cast<u8*>(hdrData);
    } else {
        data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    }
    length = width * height * canta::formatSize(format);
    //TODO: mips

    auto handle = _engine->device()->createImage({
        .width = static_cast<u32>(width),
        .height = static_cast<u32>(height),
        .format = format,
        .name = path.string()
    });

    _engine->uploadBuffer().upload(handle, std::span<u8>(data, length), {
        .width = static_cast<u32>(width),
        .height = static_cast<u32>(height),
        .format = format,
    });
    stbi_image_free(data);

    _images[_metadata[index].index] = handle;
    _metadata[index].loaded = true;
    return handle;
}

auto cen::AssetManager::loadModel(const std::filesystem::path &path, cen::Asset<Material> material) -> cen::Asset<Model> {
    auto hash = std::hash<std::filesystem::path>()(absolute(path));
    auto index = getAssetIndex(hash);
    if (index < 0)
        index = registerAsset(hash, path, "", AssetType::MODEL);

    assert(index < _metadata.size());
    assert(_metadata[index].type == AssetType::MODEL);
    if (_metadata[index].loaded) {
        assert(_metadata[index].index < _models.size());
        return { this, index };
    }

    fastgltf::Parser parser;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);
    auto type = fastgltf::determineGltfFileType(&data);
    auto asset = type == fastgltf::GltfType::GLB ?
                 parser.loadGltfBinary(&data, path.parent_path(), fastgltf::Options::None) :
                 parser.loadGltf(&data, path.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);

    if (auto error = asset.error(); error != fastgltf::Error::None) {
        return {};
    }

    std::vector<canta::ImageHandle> images = {};
    std::vector<MaterialInstance> materialInstances = {};
    for (auto& assetMaterial : asset->materials) {
        auto materialInstance = material->instance();

        if (assetMaterial.pbrData.baseColorTexture.has_value()) {
            i32 textureIndex = assetMaterial.pbrData.baseColorTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_SRGB));
            }
            if (!materialInstance.setParameter("albedoIndex", images.back().index()))
                std::printf("tried to load image");
//                _engine->logger().warn("tried to load \"albedoIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("albedoIndex", -1);


        if (assetMaterial.normalTexture.has_value()) {
            i32 textureIndex = assetMaterial.normalTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("normalIndex", images.back().index()))
                std::printf("tried to load image");
//                _engine->logger().warn("tried to load \"normalIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("normalIndex", -1);


        if (assetMaterial.pbrData.metallicRoughnessTexture.has_value()) {
            i32 textureIndex = assetMaterial.pbrData.metallicRoughnessTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("metallicRoughnessIndex", images.back().index()))
                std::printf("tried to load image");
//                _engine->logger().warn("tried to load \"metallicRoughnessIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("metallicRoughnessIndex", -1);

        if (assetMaterial.emissiveTexture.has_value()) {
            i32 textureIndex = assetMaterial.emissiveTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("emissiveIndex", images.back().index()))
                std::printf("tried to load image");
//                _engine->logger().warn("tried to load \"emissiveIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("emissiveIndex", -1);

        if (assetMaterial.emissiveStrength) {
            f32 emissiveStrength = assetMaterial.emissiveStrength;
            if (!materialInstance.setParameter("emissiveStrength", emissiveStrength))
                std::printf("tried to load image");
//                _engine->logger().warn("tried to load \"emissiveStrength\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("emissiveStrength", 0);

        materialInstances.push_back(std::move(materialInstance));
    }

    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};
    std::vector<Meshlet> meshlets = {};
    std::vector<u8> primitives = {};

    std::vector<Mesh> meshes = {};

    Model result = {};

    struct NodeInfo {
        u32 assetIndex = 0;
        u32 modelIndex = 0;
    };
    std::stack<NodeInfo> nodeInfos = {};
    for (u32 nodeIndex : asset->scenes.front().nodeIndices) {
        nodeInfos.push({
            .assetIndex = nodeIndex,
            .modelIndex = static_cast<u32>(result.nodes.size())
        });
        result.nodes.push_back({});
    }

    while (!nodeInfos.empty()) {
        auto [ assetIndex, modelIndex ] = nodeInfos.top();
        nodeInfos.pop();

        auto& assetNode = asset->nodes[assetIndex];

        ende::math::Mat4f transform = ende::math::identity<4, f32>();
        if (auto trs = std::get_if<fastgltf::TRS>(&assetNode.transform); trs) {
            ende::math::Quaternion rotation(trs->rotation[0], trs->rotation[1], trs->rotation[2], trs->rotation[3]);
            ende::math::Vec3f scale{ trs->scale[0], trs->scale[1], trs->scale[2] };
            ende::math::Vec3f translation{ trs->translation[0], trs->translation[1], trs->translation[2] };

            transform = ende::math::translation<4, f32>(translation) * rotation.toMat() * ende::math::scale<4, f32>(scale);
        } else if (auto* mat = std::get_if<fastgltf::Node::TransformMatrix>(&assetNode.transform); mat) {
            transform = ende::math::Mat4f(*mat);
        }

        for (u32 child : assetNode.children) {
            nodeInfos.push({
                .assetIndex = child,
                .modelIndex = static_cast<u32>(result.nodes.size())
            });
            result.nodes.push_back({});
        }

        result.nodes[modelIndex].name = assetNode.name;

        if (!assetNode.meshIndex.has_value())
            continue;
        u32 meshIndex = assetNode.meshIndex.value();
        auto& assetMesh = asset->meshes[meshIndex];
        for (auto& primitive : assetMesh.primitives) {
            ende::math::Vec4f min = { std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max() };
            ende::math::Vec4f max = { std::numeric_limits<f32>::lowest(), std::numeric_limits<f32>::lowest(), std::numeric_limits<f32>::lowest() };

            u32 firstVertex = vertices.size();
            u32 firstIndex = indices.size();
            u32 firstMeshlet = meshlets.size();
            u32 firstPrimitive = primitives.size();

            auto& indicesAccessor = asset->accessors[primitive.indicesAccessor.value()];
            u32 indexCount = indicesAccessor.count;

            std::vector<Vertex> meshVertices = {};
            std::vector<u32> meshIndices(indexCount);
            fastgltf::iterateAccessorWithIndex<u32>(asset.get(), indicesAccessor, [&](u32 index, u32 idx) {
                meshIndices[idx] = index;
            });

            if (auto positionsIt = primitive.findAttribute("POSITION"); positionsIt != primitive.attributes.end()) {
                auto& positionsAccessor = asset->accessors[positionsIt->second];
                meshVertices.resize(positionsAccessor.count);
                fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), positionsAccessor, [&](ende::math::Vec3f position, u32 idx) {
                    position = transform.transform(position);
                    meshVertices[idx].position = position;

                    min = { std::min(min.x(), position.x()), std::min(min.y(), position.y()), std::min(min.z(), position.z()), 1 };
                    max = { std::max(max.x(), position.x()), std::max(max.y(), position.y()), std::max(max.z(), position.z()), 1 };
                });
            }

            if (auto uvsIt = primitive.findAttribute("TEXCOORD_0"); uvsIt != primitive.attributes.end()) {
                auto& uvsAccessor = asset->accessors[uvsIt->second];
                meshVertices.resize(uvsAccessor.count);
                fastgltf::iterateAccessorWithIndex<ende::math::Vec<2, f32>>(asset.get(), uvsAccessor, [&](ende::math::Vec<2, f32> uvs, u32 idx) {
                    meshVertices[idx].uv = uvs;
                });
            }

            if (auto normalsIt = primitive.findAttribute("NORMAL"); normalsIt != primitive.attributes.end()) {
                auto& normalsAccessor = asset->accessors[normalsIt->second];
                meshVertices.resize(normalsAccessor.count);
                fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), normalsAccessor, [&](ende::math::Vec3f normal, u32 idx) {
                    meshVertices[idx].normal = normal;
                });
            }

            const u32 coneWeight = 0.f;

            u32 maxMeshlets = meshopt_buildMeshletsBound(meshIndices.size(), MAX_MESHLET_VERTICES, MAX_MESHLET_PRIMTIVES);
            std::vector<meshopt_Meshlet> meshoptMeshlets(maxMeshlets);
            std::vector<u32> meshletIndices(maxMeshlets * MAX_MESHLET_VERTICES);
            std::vector<u8> meshletPrimitives(maxMeshlets * MAX_MESHLET_PRIMTIVES * 3);

            u32 meshletCount = meshopt_buildMeshlets(meshoptMeshlets.data(), meshletIndices.data(), meshletPrimitives.data(), meshIndices.data(), meshIndices.size(), (f32*)meshVertices.data(), meshVertices.size(), sizeof(Vertex), MAX_MESHLET_VERTICES, MAX_MESHLET_PRIMTIVES, coneWeight);

            auto& lastMeshlet = meshoptMeshlets[meshletCount - 1];
            meshletIndices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
            meshletPrimitives.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));
            meshoptMeshlets.resize(meshletCount);

            std::vector<Meshlet> meshMeshlets;
            meshMeshlets.reserve(meshletCount);
            for (auto& meshlet : meshoptMeshlets) {
                auto bounds = meshopt_computeMeshletBounds(&meshletIndices[meshlet.vertex_offset], &meshletPrimitives[meshlet.triangle_offset], meshlet.triangle_count, (f32*)meshVertices.data(), meshVertices.size(), sizeof(Vertex));

                ende::math::Vec3f center = { bounds.center[0], bounds.center[1], bounds.center[2] };

                meshMeshlets.push_back({
                    .vertexOffset = firstVertex,
                    .indexOffset = meshlet.vertex_offset + firstIndex,
                    .indexCount = meshlet.vertex_count,
                    .primitiveOffset = meshlet.triangle_offset + firstPrimitive,
                    .primitiveCount = meshlet.triangle_count,
                    .center = center,
                    .radius = bounds.radius
                });
            }

            vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
            indices.insert(indices.end(), meshletIndices.begin(), meshletIndices.end());
            primitives.insert(primitives.end(), meshletPrimitives.begin(), meshletPrimitives.end());
            meshlets.insert(meshlets.end(), meshMeshlets.begin(), meshMeshlets.end());

            MaterialInstance* materialInstance = nullptr;
            if (primitive.materialIndex.has_value() && materialInstances.size() > primitive.materialIndex.value())
                materialInstance = &materialInstances[primitive.materialIndex.value()];

            meshes.push_back(Mesh{
                    .meshletOffset = firstMeshlet,
                    .meshletCount = static_cast<u32>(meshMeshlets.size()),
                    .min = min,
                    .max = max,
                    .materialInstance = materialInstance
            });
        }
    }

    auto vertexOffset = _engine->uploadVertexData(vertices);
    auto indexOffset = _engine->uploadIndexData(indices);
    auto primitiveOffset = _engine->uploadPrimitiveData(primitives);
    for (auto& meshlet : meshlets) {
        meshlet.vertexOffset += vertexOffset / sizeof(Vertex);
        meshlet.indexOffset += indexOffset / sizeof(u32);
        meshlet.primitiveOffset += primitiveOffset / sizeof(u8);
    }
    auto meshletOffset = _engine->uploadMeshletData(meshlets);

    for (auto& mesh : meshes) {
        mesh.meshletOffset += meshletOffset / sizeof(Meshlet);
    }

    result.meshes = meshes;
    result.materials = std::move(materialInstances);
    result.images = images;
    _models[_metadata[index].index] = std::move(result);
    _metadata[index].loaded = true;
    return { this, index };
}

auto cen::AssetManager::loadMaterial(const std::filesystem::path &path) -> cen::Asset<Material> {
    auto hash = std::hash<std::filesystem::path>()(absolute(path));
    auto index = getAssetIndex(hash);
    if (index < 0)
        index = registerAsset(hash, path, "", AssetType::MATERIAL);

    assert(index < _metadata.size());
    assert(_metadata[index].type == AssetType::MATERIAL);
    if (_metadata[index].loaded) {
        assert(_metadata[index].index < _materials.size());
        return { this, index };
    }

    auto file = ende::fs::File::open(_rootPath / path);

    rapidjson::Document document = {};
    document.Parse(file->read().c_str());
    assert(document.IsObject());

    const auto loadMaterialProperty = [rootPath = _rootPath, parentPath = std::filesystem::absolute(_rootPath / path.parent_path())] (rapidjson::Value& node) -> std::string {
        if (node.IsString())
            return node.GetString();
        if (node.IsObject()) {
            std::filesystem::path path = {};
            if (node.HasMember("path") && node["path"].IsString())
                path = node["path"].GetString();

            auto file = ende::fs::File::open(findFile(path, std::to_array({ rootPath, parentPath })).value());
            return file->read();
        }
        return {};
    };

    const auto macroise = [] (std::string_view str) -> std::string {
        std::string result = str.data();
        size_t index = 0;
        while ((index = result.find('\n', index)) != std::string::npos) {
            result.insert(index, "\\");
            index += 2;
        }
        return result;
    };

    if (!document.HasMember("base_pipeline") ||
        !document.HasMember("materialParameters") ||
        !document.HasMember("materialDefinition"))
        return {};

    auto pipelinePath = macroise(loadMaterialProperty(document["base_pipeline"]));
    auto materialParameters = macroise(loadMaterialProperty(document["materialParameters"]));
    auto materialDefinition = macroise(loadMaterialProperty(document["materialDefinition"]));
    auto materialLoad = macroise(loadMaterialProperty(document["materialLoad"]));

    auto material = Material::create({
        .engine = _engine,
        .lit = _engine->pipelineManager().getPipeline(_rootPath / pipelinePath, std::to_array({
            canta::Macro{
                .name = "materialParameters",
                .value = materialParameters
            },
            canta::Macro{
                .name = "materialDefinition",
                .value = materialDefinition
            },
            canta::Macro{
                .name = "materialLoad",
                .value = materialLoad
            },
            canta::Macro{
                .name = "materialEval",
                .value = macroise(loadMaterialProperty(document["lit"]))
            }
        }))
    });


    _materials[_metadata[index].index] = material;
    _metadata[index].loaded = true;
    return { this, index };
}

void cen::AssetManager::uploadMaterials() {
    for (auto& material : _materials)
        material.upload();
}