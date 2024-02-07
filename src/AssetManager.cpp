#include <Cen/AssetManager.h>
#include <Cen/Engine.h>

#include <Ende/math/Quaternion.h>

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <meshoptimizer.h>
#include <stack>
#include <cen.glsl>

template <>
struct fastgltf::ElementTraits<ende::math::Vec3f> : fastgltf::ElementTraitsBase<ende::math::Vec3f, AccessorType::Vec3, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec<2, f32>> : fastgltf::ElementTraitsBase<ende::math::Vec<2, f32>, AccessorType::Vec2, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec4f> : fastgltf::ElementTraitsBase<ende::math::Vec4f, AccessorType::Vec4, float> {};

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
    _assetMap.insert(std::make_pair(hash, index));
    _metadata.push_back({
        hash,
        name,
        path,
        false,
        type,
        index
    });

    switch (type) {
        case AssetType::IMAGE:
            break;
        case AssetType::MODEL:
            _models.push_back({});
            break;
    }
    return index;
}

auto cen::AssetManager::loadModel(const std::filesystem::path &path) -> Model* {
    auto hash = std::hash<std::filesystem::path>()(absolute(path));
    auto index = getAssetIndex(hash);
    if (index < 0)
        index = registerAsset(hash, path, "", AssetType::MODEL);

    assert(index < _metadata.size());
    if (_metadata[index].loaded) {
        assert(_metadata[index].index < _models.size());
        return &_models[_metadata[index].index];
    }

    fastgltf::Parser parser;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);
    auto type = fastgltf::determineGltfFileType(&data);
    auto asset = type == fastgltf::GltfType::GLB ?
                 parser.loadGltfBinary(&data, path.parent_path(), fastgltf::Options::None) :
                 parser.loadGltf(&data, path.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);

    if (auto error = asset.error(); error != fastgltf::Error::None) {
        return nullptr;
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

            meshes.push_back(Mesh{
                    .meshletOffset = firstMeshlet,
                    .meshletCount = static_cast<u32>(meshMeshlets.size()),
                    .min = min,
                    .max = max
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
    _metadata[index].loaded = true;
    _models[_metadata[index].index] = result;
    return &_models[_metadata[index].index];
}