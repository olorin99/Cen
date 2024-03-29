#include <Cen/AssetManager.h>
#include <Cen/Engine.h>

#include <Ende/math/Quaternion.h>
#include <Ende/filesystem/File.h>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <meshoptimizer.h>
#include <stb_image.h>
#include <ktx.h>
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
    manager._assetMutex = std::make_unique<std::mutex>();
    manager._rootPath = info.rootPath;
    manager._models.reserve(5);

    return manager;
}

auto cen::AssetManager::getAssetIndex(u32 hash) -> i32 {
    std::unique_lock lock(*_assetMutex);
    auto it = _assetMap.find(hash);
    if (it == _assetMap.end())
        return -1;
    return it->second;
}

auto cen::AssetManager::registerAsset(u32 hash, const std::filesystem::path &path, const std::string &name, AssetType type) -> i32 {
    auto index = getAssetIndex(hash);
    std::unique_lock lock(*_assetMutex);
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

auto loadKtxImage(cen::Engine* engine, const std::filesystem::path& path, canta::Format format) -> canta::ImageHandle {
    ktxTexture2* texture = nullptr;

    auto result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    if (result != ktxResult::KTX_SUCCESS)
        return {};
    if (ktxTexture2_NeedsTranscoding(texture)) {
        result = ktxTexture2_TranscodeBasis(texture, KTX_TTF_BC7_RGBA, KTX_TF_HIGH_QUALITY);
        if (result != ktxResult::KTX_SUCCESS)
            return {};
    }

    u32 width = texture->baseWidth;
    u32 height = texture->baseHeight;
    u32 mips = texture->numLevels;
    format = static_cast<canta::Format>(texture->vkFormat);

    auto handle = engine->device()->createImage({
        .width = static_cast<u32>(width),
        .height = static_cast<u32>(height),
        .format = format,
        .mipLevels = mips,
        .usage = canta::ImageUsage::SAMPLED | canta::ImageUsage::TRANSFER_DST | canta::ImageUsage::TRANSFER_SRC,
        .name = path.string()
    });

    for (u32 mip = 0; mip < mips; mip++) {
        size_t offset = 0;
        ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture*>(texture), mip, 0, 0, &offset);
        u32 length = ktxTexture_GetImageSize(reinterpret_cast<ktxTexture*>(texture), mip);
        u8* data = ktxTexture_GetData(reinterpret_cast<ktxTexture*>(texture)) + offset;

        u32 mipWidth = width >> mip;
        u32 mipHeight = height >> mip;

        engine->uploadBuffer().upload(handle, std::span<u8>(data, length), {
            .width = mipWidth,
            .height = mipHeight,
            .format = format,
            .mipLevel = mip,
            .final = mip == (mips - 1)
        });
    }
    ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(texture));
    return handle;
}

auto loadStbImage(cen::Engine* engine, const std::filesystem::path& path, canta::Format format) -> canta::ImageHandle {
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    u8* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    u32 length = width * height * canta::formatSize(format);
    assert(length != 0);

    auto handle = engine->device()->createImage({
        .width = static_cast<u32>(width),
        .height = static_cast<u32>(height),
        .format = format,
        .usage = canta::ImageUsage::SAMPLED | canta::ImageUsage::TRANSFER_DST | canta::ImageUsage::TRANSFER_SRC,
        .name = path.string()
    });
    engine->uploadBuffer().upload(handle, std::span<u8>(data, length), {
        .width = static_cast<u32>(width),
        .height = static_cast<u32>(height),
        .format = format,
    });

    stbi_image_free(data);
    return handle;
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

    bool isKtx = path.extension() == ".ktx2";

    canta::ImageHandle handle = {};
    if (isKtx) {
        handle = loadKtxImage(_engine, path, format);
    } else {
        handle = loadStbImage(_engine, path, format);
    }

    _images[_metadata[index].index] = handle;
    _metadata[index].loaded = true;
    return handle;
}

auto cen::AssetManager::loadImageAsync(const std::filesystem::path &path, canta::Format format) -> std::future<canta::ImageHandle> {
    return _engine->threadPool().addJob([this] (const std::filesystem::path& path, canta::Format format) {
        return loadImage(path, format);
    }, path, format);
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

    constexpr auto gltfExtensions = fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::EXT_meshopt_compression |
                                    fastgltf::Extensions::KHR_materials_emissive_strength;
    fastgltf::Parser parser(gltfExtensions);
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);
    auto type = fastgltf::determineGltfFileType(&data);
    auto asset = type == fastgltf::GltfType::GLB ?
                 parser.loadGltfBinary(&data, path.parent_path(), fastgltf::Options::None) :
                 parser.loadGltf(&data, path.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);

    if (auto error = asset.error(); error != fastgltf::Error::None) {
        return {};
    }

    struct ImageInfo {
        size_t materialInstanceIndex = -1;
        std::string_view materialParameter = {};
        std::future<canta::ImageHandle> image = {};
    };
    std::vector<ImageInfo> futures = {};
    std::vector<canta::ImageHandle> images = {};
    std::vector<MaterialInstance> materialInstances = {};
    for (auto& assetMaterial : asset->materials) {
        auto materialInstance = material->instance();

        if (assetMaterial.pbrData.baseColorTexture) {
            i32 textureIndex = assetMaterial.pbrData.baseColorTexture->textureIndex;
            i32 imageIndex = -1;
            if (asset->textures[textureIndex].imageIndex)
                imageIndex = asset->textures[textureIndex].imageIndex.value();
            else if (asset->textures[textureIndex].basisuImageIndex)
                imageIndex = asset->textures[textureIndex].basisuImageIndex.value();
            if (imageIndex >= 0) {
                auto& image = asset->images[imageIndex];
                if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                    futures.push_back({
                        .materialInstanceIndex = materialInstances.size(),
                        .materialParameter = "albedoIndex",
                        .image = loadImageAsync(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_SRGB)
                    });
                }
            }
        } else
            materialInstance.setParameter("albedoIndex", -1);


        if (assetMaterial.normalTexture) {
            i32 textureIndex = assetMaterial.normalTexture->textureIndex;
            i32 imageIndex = -1;
            if (asset->textures[textureIndex].imageIndex)
                imageIndex = asset->textures[textureIndex].imageIndex.value();
            else if (asset->textures[textureIndex].basisuImageIndex)
                imageIndex = asset->textures[textureIndex].basisuImageIndex.value();
            if (imageIndex >= 0) {
                auto& image = asset->images[imageIndex];
                if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                    futures.push_back({
                        .materialInstanceIndex = materialInstances.size(),
                        .materialParameter = "normalIndex",
                        .image = loadImageAsync(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_UNORM)
                    });
                }
            }
        } else
            materialInstance.setParameter("normalIndex", -1);


        if (assetMaterial.pbrData.metallicRoughnessTexture) {
            i32 textureIndex = assetMaterial.pbrData.metallicRoughnessTexture->textureIndex;
            i32 imageIndex = -1;
            if (asset->textures[textureIndex].imageIndex)
                imageIndex = asset->textures[textureIndex].imageIndex.value();
            else if (asset->textures[textureIndex].basisuImageIndex)
                imageIndex = asset->textures[textureIndex].basisuImageIndex.value();
            if (imageIndex >= 0) {
                auto& image = asset->images[imageIndex];
                if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                    futures.push_back({
                        .materialInstanceIndex = materialInstances.size(),
                        .materialParameter = "metallicRoughnessIndex",
                        .image = loadImageAsync(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_UNORM)
                    });
                }
            }
        } else
            materialInstance.setParameter("metallicRoughnessIndex", -1);

        if (assetMaterial.emissiveTexture) {
            i32 textureIndex = assetMaterial.emissiveTexture->textureIndex;
            i32 imageIndex = -1;
            if (asset->textures[textureIndex].imageIndex)
                imageIndex = asset->textures[textureIndex].imageIndex.value();
            else if (asset->textures[textureIndex].basisuImageIndex)
                imageIndex = asset->textures[textureIndex].basisuImageIndex.value();
            if (imageIndex >= 0) {
                auto& image = asset->images[imageIndex];
                if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                    futures.push_back({
                        .materialInstanceIndex = materialInstances.size(),
                        .materialParameter = "emissiveIndex",
                        .image = loadImageAsync(path.parent_path() / filePath->uri.path(), canta::Format::RGBA8_UNORM)
                    });
                }
            }
        } else
            materialInstance.setParameter("emissiveIndex", -1);

        if (assetMaterial.emissiveStrength) {
            f32 emissiveStrength = assetMaterial.emissiveStrength;
            if (!materialInstance.setParameter("emissiveStrength", emissiveStrength))
                std::printf("tried to load image");
        } else
            materialInstance.setParameter("emissiveStrength", 0);

        if (assetMaterial.alphaMode != fastgltf::AlphaMode::Opaque && assetMaterial.alphaCutoff < 1)
            materialInstance.setTransparent(true);

        materialInstances.push_back(std::move(materialInstance));
    }

    if (materialInstances.empty())
        materialInstances.push_back(material->instance());

    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};
    std::vector<Meshlet> meshlets = {};
    std::vector<u8> primitives = {};

    std::vector<Mesh> meshes = {};

    Model result = {};

    struct NodeInfo {
        u32 assetIndex = 0;
        u32 modelIndex = 0;
        ende::math::Mat4f parentTransform = ende::math::identity<4, f32>();
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
        auto [ assetIndex, modelIndex, parentTransform ] = nodeInfos.top();
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

        auto worldTransform = parentTransform * transform;

        for (u32 child : assetNode.children) {
            nodeInfos.push({
                .assetIndex = child,
                .modelIndex = static_cast<u32>(result.nodes.size()),
                .parentTransform = worldTransform
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
                    position = worldTransform.transform(position);
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

            MaterialInstance* materialInstance = &materialInstances.front();
            if (primitive.materialIndex.has_value() && materialInstances.size() > primitive.materialIndex.value())
                materialInstance = &materialInstances[primitive.materialIndex.value()];

            i32 alphaMapIndex = -1;
            if (materialInstance->isTransparent()) {
                alphaMapIndex = materialInstance->getParameter<i32>("albedoIndex");
            }

            meshes.push_back(Mesh{
                    .meshletOffset = firstMeshlet,
                    .meshletCount = static_cast<u32>(meshMeshlets.size()),
                    .min = min,
                    .max = max,
                    .materialInstance = materialInstance,
                    .alphaMapIndex = alphaMapIndex
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

    while (!futures.empty()) {
        for (auto it = futures.begin(); it != futures.end(); it++) {
            if (it->image.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                it->image.wait();
                auto image = it->image.get();
                images.push_back(image);
                materialInstances[it->materialInstanceIndex].setParameter(it->materialParameter, image->defaultView().index());
                futures.erase(it--);
            }
        }
    }

    for (auto& mesh : meshes) {
        if (mesh.materialInstance->isTransparent()) {
            mesh.alphaMapIndex = mesh.materialInstance->getParameter<i32>("albedoIndex");
        }
    }

    _engine->uploadBuffer().flushStagedData().wait();

    result.name = path;
    result.meshes = meshes;
    result.materials = std::move(materialInstances);
    result.images = images;
    _models[_metadata[index].index] = std::move(result);
    _metadata[index].loaded = true;
    return { this, index };
}

auto cen::AssetManager::loadModelAsync(const std::filesystem::path &path, Asset<cen::Material> material) -> std::future<Asset<Model>> {
    return _engine->threadPool().addJob([this] (const std::filesystem::path& path, Asset<cen::Material> material) {
        return loadModel(path, material);
    }, path, material);
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

    const auto removeComments = [] (std::string_view str) -> std::string {
        std::string result = str.data();
        size_t index = 0;
        while ((index = result.find("//", index)) != std::string::npos) {
            auto endLine = result.find('\n', index + 2);
            result.erase(index, endLine != std::string::npos ? endLine - index : 2);
        }
        return result;
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
    auto materialLoad = macroise(removeComments(loadMaterialProperty(document["materialLoad"])));

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
                .value = macroise(removeComments(loadMaterialProperty(document["lit"])))
            },
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