
#include <Canta/SDLWindow.h>
#include <Canta/RenderGraph.h>
#include <Canta/ImGuiContext.h>
#include <Canta/UploadBuffer.h>
#include <Cen/Engine.h>
#include <Cen/Camera.h>
#include <Cen/Scene.h>
#include <cstring>
#include <cen.glsl>


std::string numberToWord(u64 number) {
    if (number > 1000000000) {
        return std::to_string(number / 1000000000) + " billion";
    } else if (number > 1000000) {
        return std::to_string(number / 1000000) + " million";
    } else if (number > 1000) {
        return std::to_string(number / 1000) + " thousand";
    }
    return std::to_string(number);
}

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
        .assetPath = std::filesystem::path(CEN_SRC_DIR) / "res",
        .meshShadingEnabled = true
    });
    auto swapchain = engine->device()->createSwapchain({
        .window = &window
    });

    auto renderGraph = canta::RenderGraph::create({
        .device = engine->device(),
        .name = "RenderGraph"
    });
    auto imguiContext = canta::ImGuiContext::create({
        .device = engine->device(),
        .window = &window
    });
    auto scene = cen::Scene::create({
        .engine = engine.get()
    });

    auto model = engine->assetManager().loadModel(gltfPath);

    f32 scale = 4;
    for (u32 i = 0; i < 30; i++) {
        for (u32 j = 0; j < 30; j++) {
            for (u32 k = 0; k < 1; k++) {
                for (auto& mesh : model->meshes) {
                    scene.addMesh(mesh, ende::math::translation<4, f32>({ static_cast<f32>(i) * scale, static_cast<f32>(j) * scale, static_cast<f32>(k) * scale }));
                }
            }
        }
    }

    std::array<canta::BufferHandle, canta::FRAMES_IN_FLIGHT> globalBuffers = {};
    for (u32 i = 0; auto& buffer : globalBuffers) {
        buffer = engine->device()->createBuffer({
            .size = sizeof(GlobalData),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = std::format("global_data_buffer: {}", i++)
        });
    }
    std::array<canta::BufferHandle, canta::FRAMES_IN_FLIGHT> feedbackBuffers = {};
    for (u32 i = 0; auto& buffer : feedbackBuffers) {
        buffer = engine->device()->createBuffer({
            .size = sizeof(FeedbackInfo),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::READBACK,
            .persistentlyMapped = true,
            .name = std::format("feedback_buffer: {}", i++)
        });
    }
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
        buffer = engine->device()->createBuffer({
            .size = sizeof(cen::GPUCamera) * 2,
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = "camera_buffer"
        });
    }

    FeedbackInfo feedbackInfo = {};
    GlobalData globalData = {
            .maxMeshCount = scene.meshCount(),
            .maxMeshletCount = 10000000,
            .maxIndirectIndexCount = 10000000 * 3,
            .screenSize = { 1920, 1080 }
    };

    engine->uploadBuffer().flushStagedData();
    engine->uploadBuffer().wait();
    engine->uploadBuffer().clearSubmitted();

    auto cullMeshPipeline = engine->pipelineManager().getPipeline({
        .compute = { .module = engine->pipelineManager().getShader({
            .path = "shaders/cull_meshes.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    auto writeMeshCommandPipeline = engine->pipelineManager().getPipeline({
        .compute = { .module = engine->pipelineManager().getShader({
            .path = "shaders/write_mesh_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    auto cullMeshletPipeline = engine->pipelineManager().getPipeline({
        .compute = { .module = engine->pipelineManager().getShader({
            .path = "shaders/cull_meshlets.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    auto writeMeshletCommandPipeline = engine->pipelineManager().getPipeline({
        .compute = { .module = engine->pipelineManager().getShader({
            .path = "shaders/write_meshlet_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });

    auto meshShader = engine->pipelineManager().getShader({
        .path = "shaders/default.mesh",
        .macros = std::to_array({
            canta::Macro{ "WORKGROUP_SIZE_X", std::to_string(64) },
            canta::Macro{ "MAX_MESHLET_VERTICES", std::to_string(cen::MAX_MESHLET_VERTICES) },
            canta::Macro{ "MAX_MESHLET_PRIMTIVES", std::to_string(cen::MAX_MESHLET_PRIMTIVES) }
        }),
        .stage = canta::ShaderStage::MESH
    });
    auto fragmentShader = engine->pipelineManager().getShader({
        .path = "shaders/default.frag",
        .stage = canta::ShaderStage::FRAGMENT
    });

    auto outputIndicesShader = engine->pipelineManager().getShader({
        .path = "shaders/output_indirect.comp",
        .macros = std::to_array({
            canta::Macro{ "WORKGROUP_SIZE_X", std::to_string(64) },
            canta::Macro{ "MAX_MESHLET_VERTICES", std::to_string(cen::MAX_MESHLET_VERTICES) },
            canta::Macro{ "MAX_MESHLET_PRIMTIVES", std::to_string(cen::MAX_MESHLET_PRIMTIVES) }
        }),
        .stage = canta::ShaderStage::COMPUTE
    });
    auto outputIndicesPipeline = engine->pipelineManager().getPipeline({
        .compute = { .module = outputIndicesShader },
    });
    auto vertexShader = engine->pipelineManager().getShader({
        .path = "shaders/default.vert",
        .stage = canta::ShaderStage::VERTEX
    });
    auto vertexPipeline = engine->pipelineManager().getPipeline({
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
    bool numericalStats = true;

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
            auto cameraRotation = camera.rotation().unit();
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W])
                camera.setPosition(cameraPosition + camera.rotation().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                camera.setPosition(cameraPosition + camera.rotation().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                camera.setPosition(cameraPosition + camera.rotation().left() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                camera.setPosition(cameraPosition + camera.rotation().right() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                camera.setPosition(cameraPosition + camera.rotation().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                camera.setPosition(cameraPosition + camera.rotation().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                camera.setRotation(ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(90) * dt) * cameraRotation);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                camera.setRotation(ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(-90) * dt) * cameraRotation);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                camera.setRotation(ende::math::Quaternion(cameraRotation.right(), ende::math::rad(-45) * dt) * cameraRotation);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                camera.setRotation(ende::math::Quaternion(cameraRotation.right(), ende::math::rad(45) * dt) * cameraRotation);
        }


        engine->device()->beginFrame();
        engine->device()->gc();
        scene.prepare();
        std::memcpy(&feedbackInfo, feedbackBuffers[engine->device()->flyingIndex()]->mapped().address(), sizeof(FeedbackInfo));
        std::memset(feedbackBuffers[engine->device()->flyingIndex()]->mapped().address(), 0, sizeof(FeedbackInfo));
        globalData.feedbackInfoRef = feedbackBuffers[engine->device()->flyingIndex()]->address();
        globalBuffers[engine->device()->flyingIndex()]->data(globalData);
        renderGraph.reset();

        camera.updateFrustum();
        auto gpuCamera = camera.gpuCamera();
        cameraBuffers[engine->device()->flyingIndex()]->data(std::span<const u8>(reinterpret_cast<const u8*>(&gpuCamera), sizeof(gpuCamera)));
        if (culling)
            cameraBuffers[engine->device()->flyingIndex()]->data(std::span<const u8>(reinterpret_cast<const u8*>(&gpuCamera), sizeof(gpuCamera)), sizeof(gpuCamera));

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

                if (ImGui::TreeNode("Feedback")) {
                    ImGui::Checkbox("Numerical Stats", &numericalStats);
                    if (numericalStats) {
                        ImGui::Text("Total Meshlets: %d", scene.totalMeshlets());
//                        ImGui::Text("Total Primitives: %d", scene.totalMeshlets() * primitives.size());

                        ImGui::Text("Drawn Meshes: %d", feedbackInfo.meshesDrawn);
                        ImGui::Text("Culled Meshes: %d", feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn);
                        f32 culledMeshRatio = (static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn) / (static_cast<f32>(feedbackInfo.meshesDrawn) + static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn))) * 100;
                        if (feedbackInfo.meshesDrawn + feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn == 0)
                            culledMeshRatio = 0;
                        ImGui::Text("Culled mesh ratio: %.0f%%", culledMeshRatio);
                        ImGui::Text("Drawn Meshlets: %d", feedbackInfo.meshletsDrawn);
                        ImGui::Text("Culled Meshlets: %d", feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn);
                        f32 culledMeshletRatio = (static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn) / (static_cast<f32>(feedbackInfo.meshletsDrawn) + static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn))) * 100;
                        if (feedbackInfo.meshletsDrawn + feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn == 0)
                            culledMeshletRatio = 0;
                        ImGui::Text("Culled meshlet ratio: %.0f%%", culledMeshletRatio);
                        ImGui::Text("Drawn Triangles %d", feedbackInfo.trianglesDrawn);
                    } else {
                        ImGui::Text("Total Meshlets: %s", numberToWord(scene.totalMeshlets()).c_str());
//                        ImGui::Text("Total Primitives: %s", numberToWord(scene.totalMeshlets() * primitives.size()).c_str());

                        ImGui::Text("Drawn Meshes: %s", numberToWord(feedbackInfo.meshesDrawn).c_str());
                        ImGui::Text("Culled Meshes: %s", numberToWord(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn).c_str());
                        f32 culledMeshRatio = (static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn) / (static_cast<f32>(feedbackInfo.meshesDrawn) + static_cast<f32>(feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn))) * 100;
                        if (feedbackInfo.meshesDrawn + feedbackInfo.meshesTotal - feedbackInfo.meshesDrawn == 0)
                            culledMeshRatio = 0;
                        ImGui::Text("Culled mesh ratio: %.0f%%", culledMeshRatio);
                        ImGui::Text("Drawn Meshlets: %s", numberToWord(feedbackInfo.meshletsDrawn).c_str());
                        ImGui::Text("Culled Meshlets: %s", numberToWord(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn).c_str());
                        f32 culledMeshletRatio = (static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn) / (static_cast<f32>(feedbackInfo.meshletsDrawn) + static_cast<f32>(feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn))) * 100;
                        if (feedbackInfo.meshletsDrawn + feedbackInfo.meshletsTotal - feedbackInfo.meshletsDrawn == 0)
                            culledMeshletRatio = 0;
                        ImGui::Text("Culled meshlet ratio: %.0f%%", culledMeshletRatio);
                        ImGui::Text("Drawn Triangles %s", numberToWord(feedbackInfo.trianglesDrawn).c_str());
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Resource Stats")) {
                    auto resourceStats = engine->device()->resourceStats();
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
                    vmaCalculateStatistics(engine->device()->allocator(), &statistics);
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

                auto meshShadingEnabled = engine->meshShadingEnabled();
                if (ImGui::Checkbox("Mesh Shading", &meshShadingEnabled))
                    engine->setMeshShadingEnabled(meshShadingEnabled);

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
                    engine->device()->waitIdle();
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
            .handle = engine->vertexBuffer(),
            .name = "vertex_buffer"
        });
        auto indexBufferIndex = renderGraph.addBuffer({
            .handle = engine->indexBuffer(),
            .name = "index_buffer"
        });
        auto primitiveBufferIndex = renderGraph.addBuffer({
            .handle = engine->primitiveBuffer(),
            .name = "primitive_buffer"
        });
        auto meshBufferIndex = renderGraph.addBuffer({
            .handle = scene._meshBuffer[engine->device()->flyingIndex()],
            .name = "mesh_buffer"
        });
        auto meshletBufferIndex = renderGraph.addBuffer({
            .handle = engine->meshletBuffer(),
            .name = "meshlet_buffer"
        });
        auto meshletInstanceBufferIndex = renderGraph.addBuffer({
            .size = static_cast<u32>(sizeof(u32) + sizeof(MeshletInstance) * globalData.maxMeshletCount),
            .name = "meshlet_instance_buffer"
        });
        auto meshletInstanceBuffer2Index = renderGraph.addBuffer({
            .size = static_cast<u32>(sizeof(u32) + sizeof(MeshletInstance) * globalData.maxMeshletCount),
            .name = "meshlet_instance_2_buffer"
        });
        auto meshletCommandBufferIndex = renderGraph.addBuffer({
            .size = sizeof(DispatchIndirectCommand),
            .name = "meshlet_command_buffer"
        });
        auto cameraBufferIndex = renderGraph.addBuffer({
            .handle = cameraBuffers[engine->device()->flyingIndex()],
            .name = "camera_buffer"
        });
        auto transformsIndex = renderGraph.addBuffer({
            .handle = scene._transformBuffer[engine->device()->flyingIndex()],
            .name = "transforms_buffer"
        });

        auto depthIndex = renderGraph.addImage({
            .matchesBackbuffer = true,
            .format = canta::Format::D32_SFLOAT,
            .name = "depth_image"
        });
        auto feedbackIndex = renderGraph.addBuffer({
            .handle = feedbackBuffers[engine->device()->flyingIndex()],
            .name = "feedback_bufer"
        });

        auto& cullMeshPass = renderGraph.addPass("cull_meshes", canta::RenderPass::Type::COMPUTE);
        cullMeshPass.addStorageBufferRead(meshBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(transformsIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferWrite(meshletInstanceBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.addStorageBufferWrite(feedbackIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto transformsBuffer = graph.getBuffer(transformsIndex);
            auto meshBuffer = graph.getBuffer(meshBufferIndex);
            auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBufferIndex);
            auto cameraBuffer = graph.getBuffer(cameraBufferIndex);

            cmd.bindPipeline(cullMeshPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshBuffer;
                u64 meshletInstanceBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
                u32 meshCount;
                u32 padding;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffers[engine->device()->flyingIndex()]->address(),
                    .meshBuffer = meshBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .transformsBuffer = transformsBuffer->address(),
                    .cameraBuffer = cameraBuffer->address(),
                    .meshCount = static_cast<u32>(scene.meshCount())
            });
            cmd.dispatchThreads(scene.meshCount());
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
        cullMeshletsPass.addStorageBufferWrite(feedbackIndex, canta::PipelineStage::COMPUTE_SHADER);
        cullMeshletsPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto meshCommandBuffer = graph.getBuffer(meshCommandAlias);
            auto transformsBuffer = graph.getBuffer(transformsIndex);
            auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
            auto meshletInstanceInputBuffer = graph.getBuffer(meshletInstanceBufferIndex);
            auto meshletInstanceOutputBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
            auto cameraBuffer = graph.getBuffer(cameraBufferIndex);

            cmd.bindPipeline(cullMeshletPipeline);
            struct Push {
                u64 globalDataRef;
                u64 meshletBuffer;
                u64 meshletInstanceInputBuffer;
                u64 meshletInstanceOutputBuffer;
                u64 transformsBuffer;
                u64 cameraBuffer;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalDataRef = globalBuffers[engine->device()->flyingIndex()]->address(),
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

        if (engine->meshShadingEnabled()) {
            auto& geometryPass = renderGraph.addPass("geometry", canta::RenderPass::Type::GRAPHICS);
            geometryPass.addIndirectRead(meshletCommandBufferIndex);
            geometryPass.addStorageBufferRead(transformsIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(vertexBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(indexBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(primitiveBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(meshletInstanceBuffer2Index, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::MESH_SHADER);
            geometryPass.addStorageBufferWrite(feedbackIndex, canta::PipelineStage::MESH_SHADER);
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

                cmd.bindPipeline(engine->pipelineManager().getPipeline({
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
                    u64 globalDataRef;
                    u64 meshletBuffer;
                    u64 meshletInstanceBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 primitiveBuffer;
                    u64 transformsBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::MESH, Push {
                    .globalDataRef = globalBuffers[engine->device()->flyingIndex()]->address(),
                        .meshletBuffer = meshletBuffer->address(),
                        .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                        .vertexBuffer = vertexBuffer->address(),
                        .indexBuffer = indexBuffer->address(),
                        .primitiveBuffer = primitiveBuffer->address(),
                        .transformsBuffer = transformsBuffer->address(),
                        .cameraBuffer = cameraBuffer->address()
                });
                cmd.drawMeshTasksIndirect(meshletCommandBuffer, 0, 1);
            });
        } else {
            auto outputIndicesIndex = renderGraph.addBuffer({
                .size = static_cast<u32>(sizeof(u32) + globalData.maxIndirectIndexCount * sizeof(u32)),
                .name = "output_indices_buffer"
            });
            auto drawCommandsIndex = renderGraph.addBuffer({
                .size = static_cast<u32>(sizeof(u32) + globalData.maxMeshletCount * sizeof(VkDrawIndexedIndirectCommand)),
                .name = "draw_commands_buffer"
            });

            auto& clearPass = renderGraph.addPass("clear", canta::RenderPass::Type::TRANSFER);
            auto outputIndicesAlias = renderGraph.addAlias(outputIndicesIndex);
            auto drawCommandsAlias = renderGraph.addAlias(drawCommandsIndex);
            clearPass.addTransferWrite(drawCommandsAlias);
            clearPass.addTransferWrite(outputIndicesAlias);
            clearPass.setExecuteFunction([outputIndicesAlias, drawCommandsAlias] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto outputIndicesBuffer = graph.getBuffer(outputIndicesAlias);
                auto drawCommandBuffer = graph.getBuffer(drawCommandsAlias);
                cmd.clearBuffer(outputIndicesBuffer);
                cmd.clearBuffer(drawCommandBuffer);
            });

            auto& outputIndexBufferPass = renderGraph.addPass("output_index_buffer", canta::RenderPass::Type::COMPUTE);
            outputIndexBufferPass.addIndirectRead(meshletCommandBufferIndex);
            outputIndexBufferPass.addStorageBufferRead(primitiveBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(meshletBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(meshletInstanceBuffer2Index, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(outputIndicesAlias, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(outputIndicesIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(drawCommandsAlias, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(drawCommandsIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferWrite(feedbackIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(cameraBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(vertexBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.addStorageBufferRead(indexBufferIndex, canta::PipelineStage::COMPUTE_SHADER);
            outputIndexBufferPass.setExecuteFunction([&, outputIndicesIndex, drawCommandsIndex](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto meshletCommandBuffer = graph.getBuffer(meshletCommandBufferIndex);
                auto meshletBuffer = graph.getBuffer(meshletBufferIndex);
                auto meshletInstanceBuffer = graph.getBuffer(meshletInstanceBuffer2Index);
                auto primitiveBuffer = graph.getBuffer(primitiveBufferIndex);
                auto outputIndexBuffer = graph.getBuffer(outputIndicesIndex);
                auto drawCommandsBuffer = graph.getBuffer(drawCommandsIndex);
                auto cameraBuffer = graph.getBuffer(cameraBufferIndex);
                auto transformsBuffer = graph.getBuffer(transformsIndex);
                auto vertexBuffer = graph.getBuffer(vertexBufferIndex);
                auto indexBuffer = graph.getBuffer(indexBufferIndex);

                cmd.bindPipeline(outputIndicesPipeline);
                struct Push {
                    u64 globalDataRef;
                    u64 meshletBuffer;
                    u64 meshletInstanceBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 primitiveBuffer;
                    u64 outputIndexBuffer;
                    u64 drawCommandsBuffer;
                    u64 transformBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .globalDataRef = globalBuffers[engine->device()->flyingIndex()]->address(),
                    .meshletBuffer = meshletBuffer->address(),
                    .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                    .vertexBuffer = vertexBuffer->address(),
                    .indexBuffer = indexBuffer->address(),
                    .primitiveBuffer = primitiveBuffer->address(),
                    .outputIndexBuffer = outputIndexBuffer->address(),
                    .drawCommandsBuffer = drawCommandsBuffer->address(),
                    .transformBuffer = transformsBuffer->address(),
                    .cameraBuffer = cameraBuffer->address()
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
                    u64 globalDataRef;
                    u64 meshletBuffer;
                    u64 meshletInstanceBuffer;
                    u64 vertexBuffer;
                    u64 indexBuffer;
                    u64 meshletIndexBuffer;
                    u64 transformsBuffer;
                    u64 cameraBuffer;
                };
                cmd.pushConstants(canta::ShaderStage::VERTEX, Push {
                    .globalDataRef = globalBuffers[engine->device()->flyingIndex()]->address(),
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

//        renderGraph.setBackbuffer(swapchainIndex);
        renderGraph.setBackbuffer(uiSwapchainIndex);
        renderGraph.compile();

        auto waits = std::to_array({
            { engine->device()->frameSemaphore(), engine->device()->framePrevValue() },
            swapchain->acquireSemaphore()->getPair(),
            engine->uploadBuffer().timeline().getPair()
        });
        auto signals = std::to_array({
            engine->device()->frameSemaphore()->getPair(),
            swapchain->presentSemaphore()->getPair()
        });
        renderGraph.execute(waits, signals, true);

        swapchain->present();

        milliseconds = engine->device()->endFrame();
        dt = milliseconds / 1000.f;

    }
    engine->device()->waitIdle();
    return 0;
}