#include <Cen/Renderer.h>
#include <Cen/Engine.h>
#include <cstring>
#include <Cen/ui/GuiWorkspace.h>

#include <passes/MeshletDrawPass.h>
#include <passes/MeshletsCullPass.h>
#include <passes/DebugPasses.h>

auto cen::Renderer::create(cen::Renderer::CreateInfo info) -> Renderer {
    Renderer renderer = {};

    renderer._engine = info.engine;
    renderer._renderGraph = canta::RenderGraph::create({
        .device = info.engine->device(),
        .name = "RenderGraph"
    });

    renderer._globalData = {
        .maxMeshCount = 0,
        .maxMeshletCount = MAX_MESHLET_INSTANCE,
        .maxIndirectIndexCount = 10000000 * 3,
        .screenSize = { 1920, 1080 }
    };

    for (u32 i = 0; auto& buffer : renderer._globalBuffers) {
        buffer = info.engine->device()->createBuffer({
            .size = sizeof(GlobalData),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::STAGING,
            .persistentlyMapped = true,
            .name = std::format("global_buffer_{}", i++)
        });
    }
    for (u32 i = 0; auto& buffer : renderer._feedbackBuffers) {
        buffer = info.engine->device()->createBuffer({
            .size = sizeof(FeedbackInfo),
            .usage = canta::BufferUsage::STORAGE,
            .type = canta::MemoryType::READBACK,
            .persistentlyMapped = true,
            .name = std::format("feedback_buffer_{}", i++)
        });
    }

    renderer._cullMeshesPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "cull_meshes.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    renderer._writeMeshletCullCommandPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "write_mesh_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    renderer._cullMeshletsPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "cull_meshlets.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    renderer._writeMeshletDrawCommandPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "write_meshlet_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    renderer._drawMeshletsPipelineMeshPath = info.engine->pipelineManager().getPipeline({
        .fragment = { .module = info.engine->pipelineManager().getShader({
            .path = "visibility_buffer/visibility.frag",
            .stage = canta::ShaderStage::FRAGMENT
        })},
        .mesh = { .module = info.engine->pipelineManager().getShader({
            .path = "default.mesh",
            .macros = {
                canta::Macro{ "WORKGROUP_SIZE_X", std::to_string(64) },
                canta::Macro{ "MAX_MESHLET_VERTICES", std::to_string(cen::MAX_MESHLET_VERTICES) },
                canta::Macro{ "MAX_MESHLET_PRIMTIVES", std::to_string(cen::MAX_MESHLET_PRIMTIVES) }
            },
            .stage = canta::ShaderStage::MESH
        })},
        .rasterState = {
            .cullMode = canta::CullMode::NONE
        },
        .depthState = {
            .test = true,
            .write = true,
            .compareOp = canta::CompareOp::LEQUAL
        },
        .colourFormats = { canta::Format::R32_UINT },
        .depthFormat = canta::Format::D32_SFLOAT
    });
    renderer._writePrimitivesPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "output_indirect.comp",
            .macros = {
                canta::Macro{ "WORKGROUP_SIZE_X", std::to_string(64) },
                canta::Macro{ "MAX_MESHLET_VERTICES", std::to_string(cen::MAX_MESHLET_VERTICES) },
                canta::Macro{ "MAX_MESHLET_PRIMTIVES", std::to_string(cen::MAX_MESHLET_PRIMTIVES) }
            },
            .stage = canta::ShaderStage::COMPUTE
        })}
    });
    renderer._drawMeshletsPipelineVertexPath = info.engine->pipelineManager().getPipeline({
        .vertex = { .module = info.engine->pipelineManager().getShader({
            .path = "default.vert",
            .stage = canta::ShaderStage::VERTEX
        })},
        .fragment = { .module = info.engine->pipelineManager().getShader({
            .path = "visibility_buffer/visibility.frag",
            .stage = canta::ShaderStage::FRAGMENT
        })},
        .rasterState = {
            .cullMode = canta::CullMode::NONE
        },
        .depthState = {
            .test = true,
            .write = true,
            .compareOp = canta::CompareOp::LEQUAL
        },
        .colourFormats = { canta::Format::R32_UINT },
        .depthFormat = canta::Format::D32_SFLOAT
    });

    return renderer;
}

void cen::Renderer::render(const cen::SceneInfo &sceneInfo, canta::Swapchain* swapchain, ui::GuiWorkspace* guiWorkspace) {
    auto flyingIndex = _engine->device()->flyingIndex();
    _renderGraph.reset();

    auto swapchainImage = swapchain->acquire();
    auto swapchainResource = _renderGraph.addImage({
        .handle = swapchainImage.value(),
        .name = "swapchain_image"
    });

    auto globalBufferResource = _renderGraph.addBuffer({
        .handle = _globalBuffers[flyingIndex],
        .name = "global_buffer"
    });
    auto vertexBufferResource = _renderGraph.addBuffer({
        .handle = _engine->vertexBuffer(),
        .name = "vertex_buffer"
    });
    auto indexBufferResource = _renderGraph.addBuffer({
        .handle = _engine->indexBuffer(),
        .name = "index_buffer"
    });
    auto primitiveBufferResource = _renderGraph.addBuffer({
        .handle = _engine->primitiveBuffer(),
        .name = "primitive_buffer"
    });
    auto meshBufferResource = _renderGraph.addBuffer({
        .handle = sceneInfo.meshBuffer,
        .name = "mesh_buffer"
    });
    auto meshletBufferResource = _renderGraph.addBuffer({
        .handle = _engine->meshletBuffer(),
        .name = "meshlet_buffer"
    });
    auto meshletCullingOutputResource = _renderGraph.addBuffer({
        .size = static_cast<u32>(sizeof(u32) + sizeof(MeshletInstance) * _globalData.maxMeshletCount),
        .name = "meshlet_instance_2_buffer"
    });
    auto commandResource = _renderGraph.addBuffer({
        .size = sizeof(DispatchIndirectCommand),
        .name = "meshlet_command_buffer"
    });
    auto cameraResource = _renderGraph.addBuffer({
        .handle = sceneInfo.cameraBuffer,
        .name = "camera_buffer"
    });
    auto transformsResource = _renderGraph.addBuffer({
        .handle = sceneInfo.transformBuffer,
        .name = "transforms_buffer"
    });
    auto depthIndex = _renderGraph.addImage({
        .matchesBackbuffer = true,
        .format = canta::Format::D32_SFLOAT,
        .name = "depth_image"
    });
    auto feedbackIndex = _renderGraph.addBuffer({
        .handle = _feedbackBuffers[flyingIndex],
        .name = "feedback_buffer"
    });
    auto visibilityBuffer = _renderGraph.addImage({
        .format = canta::Format::R32_UINT,
        .name = "visibility_buffer"
    });
    auto backbuffer = _renderGraph.addImage({
        .format = canta::Format::RGBA8_UNORM,
        .name = "backbuffer"
    });

    passes::cullMeshlets(_renderGraph, {
        .globalBuffer = globalBufferResource,
        .meshBuffer = meshBufferResource,
        .meshletBuffer = meshletBufferResource,
        .meshletInstanceBuffer = meshletCullingOutputResource,
        .transformBuffer = transformsResource,
        .cameraBuffer = cameraResource,
        .feedbackBuffer = feedbackIndex,
        .outputCommand = commandResource,
        .maxMeshletInstancesCount = _globalData.maxMeshletCount,
        .meshCount = sceneInfo.meshCount,
        .cameraIndex = static_cast<i32>(sceneInfo.cullingCamera),
        .cullMeshesPipeline = _cullMeshesPipeline,
        .writeMeshletCullCommandPipeline = _writeMeshletCullCommandPipeline,
        .culLMeshletsPipeline = _cullMeshletsPipeline,
        .writeMeshletDrawCommandPipeline = _writeMeshletDrawCommandPipeline
    });

    passes::drawMeshlets(_renderGraph, {
        .command = commandResource,
        .globalBuffer = globalBufferResource,
        .vertexBuffer = vertexBufferResource,
        .indexBuffer = indexBufferResource,
        .primitiveBuffer = primitiveBufferResource,
        .meshletBuffer = meshletBufferResource,
        .meshletInstanceBuffer = meshletCullingOutputResource,
        .transformBuffer = transformsResource,
        .cameraBuffer = cameraResource,
        .feedbackBuffer = feedbackIndex,
        .backbufferImage = visibilityBuffer,
        .depthImage = depthIndex,
        .useMeshShading = _engine->meshShadingEnabled(),
        .meshShadingPipeline = _drawMeshletsPipelineMeshPath,
        .writePrimitivesPipeline = _writePrimitivesPipeline,
        .vertexPipeline = _drawMeshletsPipelineVertexPath,
        .maxMeshletInstancesCount = _globalData.maxMeshletCount,
        .generatedPrimitiveCount = _globalData.maxIndirectIndexCount
    });

    auto backbufferClear = _renderGraph.addAlias(backbuffer);
    _renderGraph.addClearPass("clear_backbuffer", backbufferClear);

    _renderSettings.debugMeshletId = (!_renderSettings.debugPrimitiveId && !_renderSettings.debugMeshId);

    if (_renderSettings.debugMeshletId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_meshletId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/meshletId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })}
            })
        }).addStorageImageRead(backbufferClear, canta::PipelineStage::COMPUTE_SHADER);
    }
    if (_renderSettings.debugPrimitiveId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_primitiveId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/primitiveId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })}
            })
        }).addStorageImageRead(backbufferClear, canta::PipelineStage::COMPUTE_SHADER);
    }
    if (_renderSettings.debugMeshId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_meshId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/meshId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })}
            })
        }).addStorageImageRead(backbufferClear, canta::PipelineStage::COMPUTE_SHADER);
    }


    if (_renderSettings.mousePick) {
        auto& mousePickPass = _renderGraph.addPass("mouse_pick", canta::RenderPass::Type::COMPUTE);

        mousePickPass.addStorageImageRead(visibilityBuffer, canta::PipelineStage::COMPUTE_SHADER);
        mousePickPass.addStorageBufferWrite(feedbackIndex, canta::PipelineStage::COMPUTE_SHADER);
        mousePickPass.addStorageImageWrite(backbuffer, canta::PipelineStage::COMPUTE_SHADER);

        mousePickPass.setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto visibilityBufferImage = graph.getImage(visibilityBuffer);
            auto globalBuffer = graph.getBuffer(globalBufferResource);
            auto meshletInstanceBuffer = graph.getBuffer(meshletCullingOutputResource);

            cmd.bindPipeline(_engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "util/mouse_pick.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })}
            }));

            struct Push {
                u64 globalBuffer;
                u64 meshletInstancesBuffer;
                i32 visibilityBuffer;
                i32 mouseX;
                i32 mouseY;
                i32 padding;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalBuffer = globalBuffer->address(),
                .meshletInstancesBuffer = meshletInstanceBuffer->address(),
                .visibilityBuffer = visibilityBufferImage.index(),
                .mouseX = _renderSettings.mouseX,
                .mouseY = _renderSettings.mouseY
            });
            cmd.dispatchWorkgroups();
        });
    }


    _renderGraph.addBlitPass("backbuffer_to_swapchain", backbuffer, swapchainResource);

    if (guiWorkspace) {
        auto uiSwapchainIndex = _renderGraph.addAlias(swapchainResource);

        auto& uiPass = _renderGraph.addPass("ui", canta::RenderPass::Type::GRAPHICS);

        uiPass.addColourRead(swapchainResource);

        uiPass.addColourWrite(uiSwapchainIndex);

        uiPass.setExecuteFunction([&guiWorkspace, &swapchain](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            guiWorkspace->context().render(ImGui::GetDrawData(), cmd, swapchain->format());
        });

        _renderGraph.setBackbuffer(uiSwapchainIndex);
    } else
        _renderGraph.setBackbuffer(swapchainResource);

    std::memcpy(&_feedbackInfo, _feedbackBuffers[flyingIndex]->mapped().address(), sizeof(FeedbackInfo));
    std::memset(_feedbackBuffers[flyingIndex]->mapped().address(), 0, sizeof(FeedbackInfo));

    _globalData.maxMeshCount = sceneInfo.meshCount;
    _globalData.maxMeshletCount = _globalData.maxMeshletCount = 10000000;
    _globalData.maxIndirectIndexCount = 10000000 * 3;
    _globalData.screenSize = { 1920, 1080 };
    _globalData.primaryCamera = sceneInfo.primaryCamera,
    _globalData.cullingCamera = sceneInfo.cullingCamera;
    _globalData.feedbackInfoRef = _feedbackBuffers[flyingIndex]->address();
    _globalBuffers[flyingIndex]->data(_globalData);

    auto result = _renderGraph.compile();
    if (!result.has_value())
        std::printf("cyclical graph found");

    auto waits = std::to_array({
        { _engine->device()->frameSemaphore(), _engine->device()->framePrevValue() },
        swapchain->acquireSemaphore()->getPair(),
        _engine->uploadBuffer().timeline().getPair()
    });
    auto signals = std::to_array({
        _engine->device()->frameSemaphore()->getPair(),
        swapchain->presentSemaphore()->getPair()
    });
    _renderGraph.execute(waits, signals, true);

    swapchain->present();
}