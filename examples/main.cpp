
#include <Canta/SDLWindow.h>
#include <Canta/RenderGraph.h>
#include <Canta/ImGuiContext.h>
#include <Canta/UploadBuffer.h>
#include <Cen/Engine.h>
#include <Cen/Camera.h>

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
        .window = &window,
        .assetPath = std::filesystem::path(CEN_SRC_DIR) / "res"
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
        ende::math::Vec3f normal = {};
        ende::math::Vec<2, f32> uvs = {};
    };
    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};
    struct Meshlet {
        u32 vertexOffset = 0;
        u32 indexOffset = 0;
        u32 indexCount = 0;
        u32 primitiveOffset = 0;
        u32 primitiveCount = 0;
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
                    .primitiveOffset = meshlet.triangle_offset + firstPrimitive,
                    .primitiveCount = meshlet.triangle_count
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
    auto camera = cen::Camera::create({
        .position = { 0, 0, 2 },
        .rotation = ende::math::Quaternion({ 0, 0, 1 }, ende::math::rad(180)),
        .width = 1920,
        .height = 1080
    });

    camera.updateFrustum();
    auto corners = camera.frustumCorners();

    std::array<canta::BufferHandle, canta::FRAMES_IN_FLIGHT> cameraBuffers = {};
    for (auto& buffer : cameraBuffers) {
        buffer = engine.device()->createBuffer({
            .size = sizeof(cen::GPUCamera),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = "camera_buffer"
        });
    }

    uploadBuffer.upload(vertexBuffer, vertices);
    uploadBuffer.upload(indexBuffer, indices);
    uploadBuffer.upload(primitiveBuffer, primitives);
    uploadBuffer.upload(meshletBuffer, meshlets);
    uploadBuffer.flushStagedData();
    uploadBuffer.wait();


    auto meshShader = engine.pipelineManager().getShader({
        .path = "shaders/default.mesh",
        .stage = canta::ShaderStage::MESH
    });
    auto fragmentShader = engine.pipelineManager().getShader({
        .path = "shaders/default.frag",
        .stage = canta::ShaderStage::FRAGMENT
    });
    auto meshPipeline = engine.pipelineManager().getPipeline({
        .fragment = { .module = fragmentShader },
        .mesh = { .module = meshShader },
        .rasterState = {
            .cullMode = canta::CullMode::NONE
        },
        .depthState = {
            .test = true,
            .write = true,
            .compareOp = canta::CompareOp::LEQUAL
        },
        .colourFormats = std::to_array({ swapchain->format() }),
        .depthFormat = canta::Format::D32_SFLOAT
    });

    auto outputIndicesShader = engine.pipelineManager().getShader({
        .path = "shaders/output_indirect.comp",
        .stage = canta::ShaderStage::COMPUTE
    });
    auto outputIndicesPipeline = engine.pipelineManager().getPipeline({
        .compute = { .module = outputIndicesShader },
    });
    auto vertexShader = engine.pipelineManager().getShader({
        .path = "shaders/default.vert",
        .stage = canta::ShaderStage::VERTEX
    });
    auto vertexPipeline = engine.pipelineManager().getPipeline({
        .vertex = { .module = vertexShader },
        .fragment = { .module = fragmentShader },
        .rasterState = {
            .cullMode = canta::CullMode::NONE
        },
        .depthState = {
            .test = true,
            .write = true,
            .compareOp = canta::CompareOp::LEQUAL
        },
        .colourFormats = std::to_array({ swapchain->format() }),
        .depthFormat = canta::Format::D32_SFLOAT
    });

    f64 dt = 1.f / 60;
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

        {
            auto cameraPosition = camera.position();
            auto cameraRotation = camera.rotation();
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W])
                camera.setPosition(cameraPosition + camera.rotation().front() * dt * 10);
//                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                camera.setPosition(cameraPosition + camera.rotation().back() * dt * 10);
//                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                camera.setPosition(cameraPosition + camera.rotation().left() * dt * 10);
//                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().left() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                camera.setPosition(cameraPosition + camera.rotation().right() * dt * 10);
//                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().right() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                camera.setPosition(cameraPosition + camera.rotation().down() * dt * 10);
//                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                camera.setPosition(cameraPosition + camera.rotation().up() * dt * 10);
//                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                camera.setRotation(cameraRotation * ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(-90) * dt * 10));
//                scene.getMainCamera()->transform().rotate({0, 1, 0}, ende::math::rad(-90) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                camera.setRotation(cameraRotation * ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(90) * dt * 10));
//                scene.getMainCamera()->transform().rotate({0, 1, 0}, ende::math::rad(90) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                camera.setRotation(cameraRotation * ende::math::Quaternion(cameraRotation.right(), ende::math::rad(45) * dt * 10));
//                scene.getMainCamera()->transform().rotate(scene.getMainCamera()->transform().rot().right(), ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                camera.setRotation(cameraRotation * ende::math::Quaternion(cameraRotation.right(), ende::math::rad(-45) * dt * 10));
//                scene.getMainCamera()->transform().rotate(scene.getMainCamera()->transform().rot().right(), ende::math::rad(-45) * dt);
        }


        engine.device()->beginFrame();
        engine.device()->gc();

        auto gpuCamera = camera.gpuCamera();
        cameraBuffers[engine.device()->flyingIndex()]->data(std::span<const u8>(reinterpret_cast<const u8*>(&gpuCamera), sizeof(gpuCamera)));

        {
            imguiContext.beginFrame();
            ImGui::ShowDemoWindow();

            if (ImGui::Begin("Stats")) {
                ImGui::Text("Delta Time: %f", dt);

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
            .handle = swapchainImage.value(),
            .name = "swapchain_image"
        });

        auto vertexBufferIndex = renderGraph.addBuffer({
            .handle = vertexBuffer,
            .name = "vertex_buffer"
        });
        auto indexBufferIndex = renderGraph.addBuffer({
            .handle = indexBuffer,
            .name = "index_buffer"
        });
        auto primitiveBufferIndex = renderGraph.addBuffer({
            .handle = primitiveBuffer,
            .name = "primitive_buffer"
        });
        auto meshletBufferIndex = renderGraph.addBuffer({
            .handle = meshletBuffer,
            .name = "meshlet_buffer"
        });
        auto cameraBufferIndex = renderGraph.addBuffer({
            .handle = cameraBuffers[engine.device()->flyingIndex()],
            .name = "camera_buffer"
        });

        auto depthIndex = renderGraph.addImage({
            .matchesBackbuffer = true,
            .format = canta::Format::D32_SFLOAT,
            .name = "depth_image"
        });

        if (engine.device()->meshShadersEnabled()) {
            auto& geometryPass = renderGraph.addPass("geometry", canta::RenderPass::Type::GRAPHICS);
            geometryPass.addStorageBufferRead(vertexBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(indexBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(primitiveBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addColourWrite(swapchainIndex);
            geometryPass.addDepthWrite(depthIndex);
            geometryPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
                auto vertexBuffer = graph.getBuffer(vertexBufferIndex);
                auto indexBuffer = graph.getBuffer(indexBufferIndex);
                auto primitiveBuffer = graph.getBuffer(primitiveBufferIndex);
                auto cameraBuffer = graph.getBuffer(cameraBufferIndex);

                cmd.bindPipeline(meshPipeline);
                cmd.setViewport({ 1920, 1080 });
                struct Push {
                    u64 meshletBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 primitiveBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::MESH, Push {
                        .meshletBuffer = meshletBuffer->address(),
                        .vertexBuffer = vertexBuffer->address(),
                        .indexBuffer = indexBuffer->address(),
                        .primitiveBuffer = primitiveBuffer->address(),
                        .cameraBuffer = cameraBuffer->address()
                });
                cmd.drawMeshTasksWorkgroups(meshlets.size(), 1, 1);
            });
        } else {
            auto outputIndicesIndex = renderGraph.addBuffer({
                .size = static_cast<u32>(meshlets.size() * 64 * 3 * sizeof(u32)),
                .name = "output_indices_buffer"
            });
            auto drawCommandsIndex = renderGraph.addBuffer({
                .size = static_cast<u32>(sizeof(u32) + meshlets.size() * sizeof(VkDrawIndexedIndirectCommand)),
                .name = "draw_commands_buffer"
            });

            auto outputIndicesAlias = renderGraph.addAlias(outputIndicesIndex);
            auto drawCommandsAlias = renderGraph.addAlias(drawCommandsIndex);
            auto& clearPass = renderGraph.addPass("clear", canta::RenderPass::Type::COMPUTE);
            clearPass.addTransferWrite(outputIndicesAlias);
            clearPass.addTransferWrite(drawCommandsAlias);
            clearPass.setExecuteFunction([=](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                cmd.clearBuffer(graph.getBuffer(outputIndicesAlias));
                cmd.clearBuffer(graph.getBuffer(drawCommandsAlias));
            });

            auto& outputIndexBufferPass = renderGraph.addPass("output_index_buffer", canta::RenderPass::Type::COMPUTE);
            outputIndexBufferPass.addStorageBufferRead(vertexBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(indexBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(primitiveBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(outputIndicesAlias, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(outputIndicesIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(drawCommandsAlias, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(drawCommandsIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.setExecuteFunction([&, outputIndicesIndex, drawCommandsIndex](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
                auto vertexBuffer = graph.getBuffer(vertexBufferIndex);
                auto indexBuffer = graph.getBuffer(indexBufferIndex);
                auto primitiveBuffer = graph.getBuffer(primitiveBufferIndex);
                auto outputIndexBuffer = graph.getBuffer(outputIndicesIndex);
                auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);

                cmd.bindPipeline(outputIndicesPipeline);
                struct Push {
                    u64 meshletBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 primitiveBuffer;
                    u64 outputIndexBuffer;
                    u64 drawCommandsBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                        .meshletBuffer = meshletBuffer->address(),
                        .vertexBuffer = vertexBuffer->address(),
                        .indexBuffer = indexBuffer->address(),
                        .primitiveBuffer = primitiveBuffer->address(),
                        .outputIndexBuffer = outputIndexBuffer->address(),
                        .drawCommandsBuffer = drawCommandsBuffer->address()
                });
                cmd.dispatchWorkgroups(meshlets.size(), 1, 1);
            });

            auto& geometryPass = renderGraph.addPass("geometry", canta::RenderPass::Type::GRAPHICS);
            geometryPass.addStorageBufferRead(outputIndicesIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addIndirectRead(drawCommandsIndex);
            geometryPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addColourWrite(swapchainIndex);
            geometryPass.addDepthWrite(depthIndex);
            geometryPass.setExecuteFunction([&, outputIndicesIndex, drawCommandsIndex] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto vertexBuffer = graph.getBuffer(vertexBufferIndex);
                auto indexBuffer = graph.getBuffer(outputIndicesIndex);
                auto cameraBuffer = graph.getBuffer(cameraBufferIndex);
                auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);

                cmd.bindPipeline(vertexPipeline);
                cmd.setViewport({ 1920, 1080 });
                struct Push {
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::VERTEX, Push {
                        .vertexBuffer = vertexBuffer->address(),
                        .indexBuffer = indexBuffer->address(),
                        .cameraBuffer = cameraBuffer->address()
                });
                cmd.drawIndirectCount(drawCommandsBuffer, sizeof(u32), drawCommandsBuffer, 0);
            });
        }


        auto uiSwapchainIndex = renderGraph.addAlias(swapchainIndex);
        auto& uiPass = renderGraph.addPass("ui", canta::RenderPass::Type::GRAPHICS);
        uiPass.addColourRead(swapchainIndex);
        uiPass.addColourWrite(uiSwapchainIndex);
        uiPass.setExecuteFunction([&imguiContext, &swapchain](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            imguiContext.render(ImGui::GetDrawData(), cmd, swapchain->format());
        });

        renderGraph.setBackbuffer(uiSwapchainIndex);
        renderGraph.compile();

        auto waits = std::to_array({
            { engine.device()->frameSemaphore(), engine.device()->framePrevValue() },
            swapchain->acquireSemaphore()->getPair(),
            uploadBuffer.timeline().getPair()
        });
        auto signals = std::to_array({
            engine.device()->frameSemaphore()->getPair(),
            swapchain->presentSemaphore()->getPair()
        });
        renderGraph.execute(waits, signals, true);

        swapchain->present();

        dt = engine.device()->endFrame() / 1000;


    }
    engine.device()->waitIdle();
    return 0;
}