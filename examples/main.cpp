
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

#include <cen.glsl>

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
        .device = engine.device(),
        .name = "RenderGraph"
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

    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};
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

            meshes.push_back({
                .meshletOffset = firstMeshlet,
                .meshletCount = static_cast<u32>(meshMeshlets.size())
            });
        }
    }

    std::vector<ende::math::Mat4f> transforms;
    std::vector<GPUMesh> gpuMeshes;
    for (u32 i = 0; i < 30; i++) {
        for (u32 j = 0; j < 30; j++) {
            gpuMeshes.push_back({
                .meshletOffset = 0,
                .meshletCount = static_cast<u32>(meshlets.size()),
                .min = { -1, -1, -1 },
                .max = { 1, 1, 1 },
            });
            transforms.push_back(ende::math::translation<4, f32>({ static_cast<f32>(i * 2), static_cast<f32>(j * 2), 0 }));
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
    auto meshBuffer = engine.device()->createBuffer({
        .size = static_cast<u32>(sizeof(GPUMesh) * transforms.size()),
        .usage = canta::BufferUsage::STORAGE,
        .name = "transforms_buffer"
    });
    auto transformsBuffer = engine.device()->createBuffer({
        .size = static_cast<u32>(sizeof(ende::math::Mat4f) * transforms.size()),
        .usage = canta::BufferUsage::STORAGE,
        .name = "transforms_buffer"
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
            .size = sizeof(cen::GPUCamera) * 2,
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
    uploadBuffer.upload(meshBuffer, gpuMeshes);
    uploadBuffer.upload(transformsBuffer, transforms);
    uploadBuffer.flushStagedData();
    uploadBuffer.wait();

    auto cullMeshPipeline = engine.pipelineManager().getPipeline({
        .compute = { .module = engine.pipelineManager().getShader({
            .path = "shaders/cull_meshes.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    auto writeMeshCommandPipeline = engine.pipelineManager().getPipeline({
        .compute = { .module = engine.pipelineManager().getShader({
            .path = "shaders/write_mesh_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    auto cullMeshletPipeline = engine.pipelineManager().getPipeline({
        .compute = { .module = engine.pipelineManager().getShader({
            .path = "shaders/cull_meshlets.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    auto writeMeshletCommandPipeline = engine.pipelineManager().getPipeline({
        .compute = { .module = engine.pipelineManager().getShader({
            .path = "shaders/write_meshlet_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });

    auto meshShader = engine.pipelineManager().getShader({
        .path = "shaders/default.mesh",
        .stage = canta::ShaderStage::MESH
    });
    auto fragmentShader = engine.pipelineManager().getShader({
        .path = "shaders/default.frag",
        .stage = canta::ShaderStage::FRAGMENT
    });
//    auto meshPipeline = engine.pipelineManager().getPipeline({
//        .fragment = { .module = fragmentShader },
//        .mesh = { .module = meshShader },
//        .rasterState = {
//            .cullMode = canta::CullMode::NONE
//        },
//        .depthState = {
//            .test = true,
//            .write = true,
//            .compareOp = canta::CompareOp::LEQUAL
//        },
//        .colourFormats = std::to_array({ swapchain->format() }),
//        .depthFormat = canta::Format::D32_SFLOAT
//    });

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

    bool culling = true;

    f64 milliseconds = 16;
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
        renderGraph.reset();

        camera.updateFrustum();
        auto gpuCamera = camera.gpuCamera();
        cameraBuffers[engine.device()->flyingIndex()]->data(std::span<const u8>(reinterpret_cast<const u8*>(&gpuCamera), sizeof(gpuCamera)));
        if (culling)
            cameraBuffers[engine.device()->flyingIndex()]->data(std::span<const u8>(reinterpret_cast<const u8*>(&gpuCamera), sizeof(gpuCamera)), sizeof(gpuCamera));

        {
            imguiContext.beginFrame();
            ImGui::ShowDemoWindow();

            if (ImGui::Begin("Stats")) {
                ImGui::Text("Milliseconds: %f", milliseconds);
                ImGui::Text("Delta Time: %f", dt);

                auto timers = renderGraph.timers();
                for (auto& timer : timers) {
                    ImGui::Text("%s: %f ms", timer.first.c_str(), timer.second.result().value() / 1000000.f);
                }
                auto pipelineStatistics = renderGraph.pipelineStatistics();
                for (auto& pipelineStats : pipelineStatistics) {
                    if (ImGui::TreeNode(pipelineStats.first.c_str())) {
                        auto stats = pipelineStats.second.result().value();
                        ImGui::Text("Input Assembly Vertices: %d", stats.inputAssemblyVertices);
                        ImGui::Text("Input Assembly Primitives: %d", stats.inputAssemblyPrimitives);
                        ImGui::Text("Vertex Shader Invocations: %d", stats.vertexShaderInvocations);
                        ImGui::Text("Geometry Shader Invocations: %d", stats.geometryShaderInvocations);
                        ImGui::Text("Geometry Shader Primitives: %d", stats.geometryShaderPrimitives);
                        ImGui::Text("Clipping Invocations: %d", stats.clippingInvocations);
                        ImGui::Text("Clipping Primitives: %d", stats.clippingPrimitives);
                        ImGui::Text("Fragment Shader Invocations: %d", stats.fragmentShaderInvocations);
                        ImGui::Text("Tessellation Control Shader Patches: %d", stats.tessellationControlShaderPatches);
                        ImGui::Text("Tessellation Evaluation Shader Invocations: %d", stats.tessellationEvaluationShaderInvocations);
                        ImGui::Text("Compute Shader Invocations: %d", stats.computeShaderInvocations);
                        ImGui::TreePop();
                    }
                }

                if (ImGui::TreeNode("Resource Stats")) {
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
                    ImGui::Text("Timestamp Query Pools %d", resourceStats.timestampQueryPools);
                    ImGui::Text("PipelineStats Pools %d", resourceStats.pipelineStatsPools);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("RenderGraph Stats")) {
                    auto renderGraphStats = renderGraph.statistics();
                    ImGui::Text("Passes: %d", renderGraphStats.passes);
                    ImGui::Text("Resource: %d", renderGraphStats.resources);
                    ImGui::Text("Image: %d", renderGraphStats.images);
                    ImGui::Text("Buffers: %d", renderGraphStats.buffers);
                    ImGui::Text("Command Buffers: %d", renderGraphStats.commandBuffers);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Memory Stats")) {
                    VmaTotalStatistics statistics = {};
                    vmaCalculateStatistics(engine.device()->allocator(), &statistics);
                    for (auto& stats : statistics.memoryHeap) {
                        if (stats.unusedRangeSizeMax == 0 && stats.allocationSizeMax == 0)
                            break;
                        ImGui::Separator();
                        ImGui::Text("\tAllocations: %u", stats.statistics.allocationCount);
                        ImGui::Text("\tAllocations size: %lu", stats.statistics.allocationBytes / 1000000);
                        ImGui::Text("\tAllocated blocks: %u", stats.statistics.blockCount);
                        ImGui::Text("\tBlock size: %lu mb", stats.statistics.blockBytes / 1000000);

                        ImGui::Text("\tLargest allocation: %lu mb", stats.allocationSizeMax / 1000000);
                        ImGui::Text("\tSmallest allocation: %lu b", stats.allocationSizeMin);
                        ImGui::Text("\tUnused range count: %u", stats.unusedRangeCount);
                        ImGui::Text("\tUnused range max: %lu mb", stats.unusedRangeSizeMax / 1000000);
                        ImGui::Text("\tUnused range min: %lu b", stats.unusedRangeSizeMin);
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::End();

            if (ImGui::Begin("Settings")) {
                ImGui::Checkbox("Culling", &culling);

                auto meshShadingEnabled = engine.meshShadingEnabled();
                if (ImGui::Checkbox("Mesh Shading", &meshShadingEnabled))
                    engine.setMeshShadingEnabled(meshShadingEnabled);

                auto timingEnabled = renderGraph.timingEnabled();
                if (ImGui::Checkbox("RenderGraph Timing", &timingEnabled))
                    renderGraph.setTimingEnabled(timingEnabled);
                auto individualTiming = renderGraph.individualTiming();
                if (ImGui::Checkbox("RenderGraph Per Pass Timing", &individualTiming))
                    renderGraph.setIndividualTiming(individualTiming);
                auto pipelineStatsEnabled = renderGraph.pipelineStatisticsEnabled();
                if (ImGui::Checkbox("RenderGraph PiplelineStats", &pipelineStatsEnabled))
                    renderGraph.setPipelineStatisticsEnabled(pipelineStatsEnabled);
                auto individualPipelineStatistics = renderGraph.individualPipelineStatistics();
                if (ImGui::Checkbox("RenderGraph Per Pass PiplelineStats", &individualPipelineStatistics))
                    renderGraph.setIndividualPipelineStatistics(individualPipelineStatistics);


                const char* modes[] = { "FIFO", "MAILBOX", "IMMEDIATE" };
                static int modeIndex = 0;
                if (ImGui::Combo("PresentMode", &modeIndex, modes, 3)) {
                    engine.device()->waitIdle();
                    switch (modeIndex) {
                        case 0:
                            swapchain->setPresentMode(canta::PresentMode::FIFO);
                            break;
                        case 1:
                            swapchain->setPresentMode(canta::PresentMode::MAILBOX);
                            break;
                        case 2:
                            swapchain->setPresentMode(canta::PresentMode::IMMEDIATE);
                            break;
                    }
                }
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
        auto meshBufferIndex = renderGraph.addBuffer({
            .handle = meshBuffer,
            .name = "mesh_buffer"
        });
        auto meshletBufferIndex = renderGraph.addBuffer({
            .handle = meshletBuffer,
            .name = "meshlet_buffer"
        });
        auto meshletInstanceBufferIndex = renderGraph.addBuffer({
            .size = static_cast<u32>(sizeof(u32) + sizeof(MeshletInstance) * gpuMeshes.size() * meshlets.size()),
            .name = "meshlet_instance_buffer"
        });
        auto meshletInstanceBuffer2Index = renderGraph.addBuffer({
            .size = static_cast<u32>(sizeof(u32) + sizeof(MeshletInstance) * gpuMeshes.size() * meshlets.size()),
            .name = "meshlet_instance_2_buffer"
        });
        auto meshletCommandBufferIndex = renderGraph.addBuffer({
            .size = sizeof(DispatchIndirectCommand),
            .name = "meshlet_command_buffer"
        });
        auto cameraBufferIndex = renderGraph.addBuffer({
            .handle = cameraBuffers[engine.device()->flyingIndex()],
            .name = "camera_buffer"
        });
        auto transformsIndex = renderGraph.addBuffer({
            .handle = transformsBuffer,
            .name = "transforms_buffer"
        });

        auto depthIndex = renderGraph.addImage({
            .matchesBackbuffer = true,
            .format = canta::Format::D32_SFLOAT,
            .name = "depth_image"
        });

        auto& cullMeshPass = renderGraph.addPass("cull_meshes", canta::RenderPass::Type::COMPUTE);
        cullMeshPass.addStorageBufferRead(meshBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(transformsIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferWrite(meshletInstanceBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto transformsBuffer = graph.getBuffer(transformsIndex);
            auto meshBuffer = graph.getBuffer(meshBufferIndex);
            auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBufferIndex);
            auto cameraBuffer = graph.getBuffer(cameraBufferIndex);

            cmd.bindPipeline(cullMeshPipeline);
            struct Push {
                u64 meshBuffer;
                u64 meshletInstanceBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
                u32 meshCount;
                u32 padding;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .meshBuffer = meshBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .transformsBuffer = transformsBuffer->address(),
                    .cameraBuffer = cameraBuffer->address(),
                    .meshCount = static_cast<u32>(gpuMeshes.size())
            });
            cmd.dispatchThreads(gpuMeshes.size());
        });
        auto meshCommandAlias = renderGraph.addAlias(meshletCommandBufferIndex);
        auto& writeMeshCommandPass = renderGraph.addPass("write_mesh_command", canta::RenderPass::Type::COMPUTE);
        writeMeshCommandPass.addStorageBufferRead(meshletInstanceBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        writeMeshCommandPass.addStorageBufferWrite(meshCommandAlias, canta::PipelineStage::COMPUTE_SHADER);
        writeMeshCommandPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBufferIndex);
            auto meshletCommandBuffer = graph.getBuffer(meshCommandAlias);

            cmd.bindPipeline(writeMeshCommandPipeline);
            struct Push {
                u64 meshletInstanceBuffer;
                u64 commandBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .commandBuffer = meshletCommandBuffer->address()
            });
            cmd.dispatchWorkgroups();
        });

        auto& cullMeshletsPass = renderGraph.addPass("cull_meshlets", canta::RenderPass::Type::COMPUTE);
        cullMeshletsPass.addIndirectRead(meshCommandAlias);
        cullMeshletsPass.addStorageBufferRead(transformsIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferRead(meshletInstanceBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.addStorageBufferWrite(meshletInstanceBuffer2Index, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshCommandBuffer = graph.getBuffer(meshCommandAlias);
            auto transformsBuffer = graph.getBuffer(transformsIndex);
            auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
            auto meshletInstanceInputBuffer = graph.getBuffer(meshletInstanceBufferIndex);
            auto meshletInstanceOutputBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
            auto cameraBuffer = graph.getBuffer(cameraBufferIndex);

            cmd.bindPipeline(cullMeshletPipeline);
            struct Push {
                u64 meshletBuffer;
                u64 meshletInstanceInputBuffer;
                u64 meshletInstanceOutputBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .meshletBuffer = meshletBuffer->address(),
                .meshletInstanceInputBuffer = meshletInstanceInputBuffer->address(),
                .meshletInstanceOutputBuffer = meshletInstanceOutputBuffer->address(),
                .transformsBuffer = transformsBuffer->address(),
                .cameraBuffer = cameraBuffer->address()
            });
            cmd.dispatchIndirect(meshCommandBuffer, 0);
        });
        auto& writeMeshletCommandPass = renderGraph.addPass("write_meshlet_command", canta::RenderPass::Type::COMPUTE);
        writeMeshletCommandPass.addStorageBufferRead(meshletInstanceBuffer2Index, canta::PipelineStage::COMPUTE_SHADER);
        writeMeshletCommandPass.addStorageBufferWrite(meshletCommandBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        writeMeshletCommandPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
            auto meshletCommandBuffer = graph.getBuffer(meshletCommandBufferIndex);

            cmd.bindPipeline(writeMeshletCommandPipeline);
            struct Push {
                u64 meshletInstanceBuffer;
                u64 commandBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .commandBuffer = meshletCommandBuffer->address()
            });
            cmd.dispatchWorkgroups();
        });

        if (engine.meshShadingEnabled()) {
            auto& geometryPass = renderGraph.addPass("geometry", canta::RenderPass::Type::GRAPHICS);
            geometryPass.addIndirectRead(meshletCommandBufferIndex);
            geometryPass.addStorageBufferRead(transformsIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(vertexBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(indexBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(primitiveBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(meshletInstanceBuffer2Index, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addColourWrite(swapchainIndex);
            geometryPass.addDepthWrite(depthIndex);
            geometryPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto transformsBuffer = graph.getBuffer(transformsIndex);
                auto meshletCommandBuffer = graph.getBuffer(meshletCommandBufferIndex);
                auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
                auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
                auto vertexBuffer = graph.getBuffer(vertexBufferIndex);
                auto indexBuffer = graph.getBuffer(indexBufferIndex);
                auto primitiveBuffer = graph.getBuffer(primitiveBufferIndex);
                auto cameraBuffer = graph.getBuffer(cameraBufferIndex);

                cmd.bindPipeline(engine.pipelineManager().getPipeline({
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
                }));
                cmd.setViewport({ 1920, 1080 });
                struct Push {
                    u64 meshletBuffer;
                    u64 meshletInstanceBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 primitiveBuffer;
                    u64 transformsBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::MESH, Push {
                        .meshletBuffer = meshletBuffer->address(),
                        .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                        .vertexBuffer = vertexBuffer->address(),
                        .indexBuffer = indexBuffer->address(),
                        .primitiveBuffer = primitiveBuffer->address(),
                        .transformsBuffer = transformsBuffer->address(),
                        .cameraBuffer = cameraBuffer->address()
                });
//                cmd.drawMeshTasksWorkgroups(meshlets.size(), 1, 1);
                cmd.drawMeshTasksIndirect(meshletCommandBuffer, 0, 1);
            });
        } else {
            auto outputIndicesIndex = renderGraph.addBuffer({
                .size = static_cast<u32>(meshlets.size() * gpuMeshes.size() * 64 * 3 * sizeof(u32)),
                .name = "output_indices_buffer"
            });
            auto drawCommandsIndex = renderGraph.addBuffer({
                .size = static_cast<u32>(sizeof(u32) + meshlets.size() * gpuMeshes.size() * sizeof(VkDrawIndexedIndirectCommand)),
                .name = "draw_commands_buffer"
            });

            auto& outputIndexBufferPass = renderGraph.addPass("output_index_buffer", canta::RenderPass::Type::COMPUTE);
            outputIndexBufferPass.addIndirectRead(meshletCommandBufferIndex);
            outputIndexBufferPass.addStorageBufferRead(primitiveBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(meshletInstanceBuffer2Index, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(outputIndicesIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(drawCommandsIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.setExecuteFunction([&, outputIndicesIndex, drawCommandsIndex](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto meshletCommandBuffer = graph.getBuffer(meshletCommandBufferIndex);
                auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
                auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
                auto primitiveBuffer = graph.getBuffer(primitiveBufferIndex);
                auto outputIndexBuffer = graph.getBuffer(outputIndicesIndex);
                auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);

                cmd.bindPipeline(outputIndicesPipeline);
                struct Push {
                    u64 meshletBuffer;
                    u64 meshletInstanceBuffer;
                    u64 primitiveBuffer;
                    u64 outputIndexBuffer;
                    u64 drawCommandsBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                        .meshletBuffer = meshletBuffer->address(),
                        .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                        .primitiveBuffer = primitiveBuffer->address(),
                        .outputIndexBuffer = outputIndexBuffer->address(),
                        .drawCommandsBuffer = drawCommandsBuffer->address()
                });
                cmd.dispatchIndirect(meshletCommandBuffer, 0);
            });

            auto& geometryPass = renderGraph.addPass("geometry", canta::RenderPass::Type::GRAPHICS);
            geometryPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addStorageBufferRead(meshletInstanceBuffer2Index, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addStorageBufferRead(vertexBufferIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addStorageBufferRead(indexBufferIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addStorageBufferRead(outputIndicesIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addStorageBufferRead(transformsIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addIndirectRead(drawCommandsIndex);
            geometryPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::VERTEX_SHADER);
            geometryPass.addColourWrite(swapchainIndex);
            geometryPass.addDepthWrite(depthIndex);
            geometryPass.setExecuteFunction([&, outputIndicesIndex, drawCommandsIndex] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
                auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
                auto transformsBuffer = graph.getBuffer(transformsIndex);
                auto vertexBuffer = graph.getBuffer(vertexBufferIndex);
                auto indexBuffer = graph.getBuffer(indexBufferIndex);
                auto meshletIndexBuffer = graph.getBuffer(outputIndicesIndex);
                auto cameraBuffer = graph.getBuffer(cameraBufferIndex);
                auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);

                cmd.bindPipeline(vertexPipeline);
                cmd.setViewport({ 1920, 1080 });
                struct Push {
                    u64 meshletBuffer;
                    u64 meshletInstanceBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 meshletIndexBuffer;
                    u64 transformsBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::VERTEX, Push {
                    .meshletBuffer = meshletBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .vertexBuffer = vertexBuffer->address(),
                    .indexBuffer = indexBuffer->address(),
                    .meshletIndexBuffer = meshletIndexBuffer->address(),
                    .transformsBuffer = transformsBuffer->address(),
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

        milliseconds = engine.device()->endFrame();
        dt = milliseconds / 1000.f;

    }
    engine.device()->waitIdle();
    return 0;
}