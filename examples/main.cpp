
#include <Canta/SDLWindow.h>
#include <Canta/RenderGraph.h>
#include <Canta/ImGuiContext.h>
#include <Canta/UploadBuffer.h>
#include <Cen/Engine.h>

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <meshoptimizer.h>
#include <stack>

template <>
struct fastgltf::ElementTraits<ende::math::Vec3f> : fastgltf::ElementTraitsBase<ende::math::Vec3f, AccessorType::Vec3, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec<2, f32>> : fastgltf::ElementTraitsBase<ende::math::Vec<2, f32>, AccessorType::Vec2, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec4f> : fastgltf::ElementTraitsBase<ende::math::Vec4f, AccessorType::Vec4, float> {};

int main(int argc, char* argv[]) {

    std::filesystem::path gltfPath = {};
    if (argc > 1)
        gltfPath = argv[1];
    else
        return -1;

    canta::SDLWindow window("Cen Main", 1920, 1080);

    auto engine = cen::Engine::create({
        .applicationName = "CenMain",
        .window = &window
    });
    auto swapchain = engine.device()->createSwapchain({
        .window = &window
    });

    auto renderGraph = canta::RenderGraph::create({
        .device = engine.device()
    });
    auto imguiContext = canta::ImGuiContext::create({
        .device = engine.device(),
        .window = &window
    });
    auto uploadBuffer = canta::UploadBuffer::create({
        .device = engine.device(),
        .size = 1 << 16
    });


    fastgltf::Parser parser;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(gltfPath);
    auto type = fastgltf::determineGltfFileType(&data);
    auto asset = type == fastgltf::GltfType::GLB ?
            parser.loadGltfBinary(&data, gltfPath.parent_path(), fastgltf::Options::None) :
            parser.loadGltf(&data, gltfPath.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);

    if (auto error = asset.error(); error != fastgltf::Error::None) {
        return -2;
    }

    struct Vertex {
        ende::math::Vec3f position = {};
        ende::math::Vec<2, f32> uvs = {};
        ende::math::Vec3f normal = {};
    };
    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};
    struct Meshlet {
        u32 vertexOffset = 0;
        u32 indexOffset = 0;
        u32 indexCount = 0;
        u32 primitiveOffset = 0;
    };
    std::vector<Meshlet> meshlets = {};
    std::vector<u8> primitives = {};

    struct Mesh {
        u32 meshletOffset = 0;
        u32 meshletCount = 0;
    };
    std::vector<Mesh> meshes = {};

    struct NodeInfo {
        u32 assetIndex = 0;
    };
    std::stack<NodeInfo> nodeInfos = {};
    for (u32 index : asset->scenes.front().nodeIndices) {
        nodeInfos.push({
            .assetIndex = index
        });
    }

    while (!nodeInfos.empty()) {
        auto [ assetIndex ] = nodeInfos.top();
        nodeInfos.pop();

        auto& assetNode = asset->nodes[assetIndex];

        for (u32 child : assetNode.children) {
            nodeInfos.push({
                .assetIndex = child
            });
        }

        if (!assetNode.meshIndex.has_value())
            continue;
        u32 meshIndex = assetNode.meshIndex.value();
        auto& assetMesh = asset->meshes[meshIndex];
        for (auto& primitive : assetMesh.primitives) {
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
                    meshVertices[idx].position = position;
                });
            }

            if (auto uvsIt = primitive.findAttribute("TEXCOORD_0"); uvsIt != primitive.attributes.end()) {
                auto& uvsAccessor = asset->accessors[uvsIt->second];
                meshVertices.resize(uvsAccessor.count);
                fastgltf::iterateAccessorWithIndex<ende::math::Vec<2, f32>>(asset.get(), uvsAccessor, [&](ende::math::Vec<2, f32> uvs, u32 idx) {
                    meshVertices[idx].uvs = uvs;
                });
            }

            if (auto normalsIt = primitive.findAttribute("NORMAL"); normalsIt != primitive.attributes.end()) {
                auto& normalsAccessor = asset->accessors[normalsIt->second];
                meshVertices.resize(normalsAccessor.count);
                fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), normalsAccessor, [&](ende::math::Vec3f normal, u32 idx) {
                    meshVertices[idx].normal = normal;
                });
            }

            const u32 maxVertices = 64;
            const u32 maxTriangles = 64;
            const u32 coneWeight = 0.f;

            u32 maxMeshlets = meshopt_buildMeshletsBound(meshIndices.size(), maxVertices, maxTriangles);
            std::vector<meshopt_Meshlet> meshoptMeshlets(maxMeshlets);
            std::vector<u32> meshletIndices(maxMeshlets * maxVertices);
            std::vector<u8> meshletPrimitives(maxMeshlets * maxTriangles * 3);

            u32 meshletCount = meshopt_buildMeshlets(meshoptMeshlets.data(), meshletIndices.data(), meshletPrimitives.data(), meshIndices.data(), meshIndices.size(), (f32*)meshVertices.data(), meshVertices.size(), sizeof(Vertex), maxVertices, maxTriangles, coneWeight);

            auto& lastMeshlet = meshoptMeshlets[meshletCount - 1];
            meshletIndices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
            meshletPrimitives.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));
            meshoptMeshlets.resize(meshletCount);

            std::vector<Meshlet> meshMeshlets;
            meshMeshlets.reserve(meshletCount);
            for (auto& meshlet : meshoptMeshlets) {
                meshMeshlets.push_back({
                    .vertexOffset = firstVertex,
                    .indexOffset = meshlet.vertex_offset + firstIndex,
                    .indexCount = meshlet.vertex_count,
                    .primitiveOffset = meshlet.triangle_offset + firstPrimitive
                });
            }

            vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
            indices.insert(indices.end(), meshletIndices.begin(), meshletIndices.end());
            primitives.insert(primitives.end(), meshletPrimitives.begin(), meshletPrimitives.end());
            meshlets.insert(meshlets.end(), meshMeshlets.begin(), meshMeshlets.end());

            meshes.push_back({
                .meshletOffset = firstMeshlet,
                .meshletCount = static_cast<u32>(meshMeshlets.size())
            });
        }
    }

    auto vertexBuffer = engine.device()->createBuffer({
        .size = static_cast<u32>(vertices.size() * sizeof(Vertex)),
        .usage = canta::BufferUsage::STORAGE,
        .name = "vertex_buffer"
    });
    auto indexBuffer = engine.device()->createBuffer({
        .size = static_cast<u32>(indices.size() * sizeof(u32)),
        .usage = canta::BufferUsage::STORAGE,
        .name = "index_buffer"
    });
    auto primitiveBuffer = engine.device()->createBuffer({
        .size = static_cast<u32>(primitives.size() * sizeof(u8)),
        .usage = canta::BufferUsage::STORAGE,
        .name = "primitive_buffer"
    });
    auto meshletBuffer = engine.device()->createBuffer({
        .size = static_cast<u32>(meshlets.size() * sizeof(Meshlet)),
        .usage = canta::BufferUsage::STORAGE,
        .name = "meshlet_buffer"
    });

    uploadBuffer.upload(vertexBuffer, vertices);
    uploadBuffer.upload(indexBuffer, indices);
    uploadBuffer.upload(primitiveBuffer, primitives);
    uploadBuffer.upload(meshletBuffer, meshlets);


    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
            imguiContext.processEvent(&event);
        }

        engine.device()->beginFrame();
        engine.device()->gc();


        {
            imguiContext.beginFrame();
            ImGui::ShowDemoWindow();

            if (ImGui::Begin("Stats")) {

                auto resourceStats = engine.device()->resourceStats();
                ImGui::Text("Shader Count %d", resourceStats.shaderCount);
                ImGui::Text("Shader Allocated %d", resourceStats.shaderAllocated);
                ImGui::Text("Pipeline Count %d", resourceStats.pipelineCount);
                ImGui::Text("Pipeline Allocated %d", resourceStats.pipelineAllocated);
                ImGui::Text("Image Count %d", resourceStats.imageCount);
                ImGui::Text("Image Allocated %d", resourceStats.imageAllocated);
                ImGui::Text("Buffer Count %d", resourceStats.bufferCount);
                ImGui::Text("Buffer Allocated %d", resourceStats.bufferAllocated);
                ImGui::Text("Sampler Count %d", resourceStats.samplerCount);
                ImGui::Text("Sampler Allocated %d", resourceStats.shaderAllocated);
            }
            ImGui::End();

            ImGui::Render();
        }


        auto swapchainImage = swapchain->acquire();

        auto swapchainIndex = renderGraph.addImage({
            .handle = swapchainImage.value()
        });

        auto& uiPass = renderGraph.addPass("ui", canta::RenderPass::Type::GRAPHICS);
        uiPass.addColourWrite(swapchainIndex);
        uiPass.setExecuteFunction([&imguiContext, &swapchain](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            imguiContext.render(ImGui::GetDrawData(), cmd, swapchain->format());
        });

        renderGraph.setBackbuffer(swapchainIndex);
        renderGraph.compile();

        auto waits = std::to_array({
            { engine.device()->frameSemaphore(), engine.device()->framePrevValue() },
            swapchain->acquireSemaphore()->getPair()
        });
        auto signals = std::to_array({
            engine.device()->frameSemaphore()->getPair(),
            swapchain->presentSemaphore()->getPair()
        });
        renderGraph.execute(waits, signals, true);

        swapchain->present();

        engine.device()->endFrame();


    }
    engine.device()->waitIdle();
    return 0;
}