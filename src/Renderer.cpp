#include <Cen/Renderer.h>
#include <Cen/Engine.h>
#include <cstring>
#include <Cen/ui/GuiWorkspace.h>

#include <passes/MeshletDrawPass.h>
#include <passes/MeshletsCullPass.h>
#include <passes/DebugPasses.h>
#include <passes/BloomPass.h>

#include <stb_image_write.h>

auto cen::Renderer::create(cen::Renderer::CreateInfo info) -> Renderer {
    Renderer renderer = {};

    renderer._engine = info.engine;
    renderer._renderGraph = canta::RenderGraph::create({
        .device = info.engine->device(),
        .timingMode = canta::RenderGraph::TimingMode::PER_GROUP,
        .name = "RenderGraph"
    });

    renderer._globalData = {
        .maxMeshCount = 0,
        .maxMeshletCount = MAX_MESHLET_INSTANCE,
        .maxIndirectIndexCount = 10000000 * 3,
        .screenSize = { 1920, 1080 }
    };

    renderer._textureSampler = info.engine->device()->createSampler({
        .filter = canta::Filter::LINEAR,
        .addressMode = canta::AddressMode::REPEAT,
        .anisotropy = true,
        .maxAnisotropy = info.engine->device()->limits().maxSamplerAnisotropy
    });
    renderer._depthSampler = info.engine->device()->createSampler({
        .filter = canta::Filter::LINEAR,
        .addressMode = canta::AddressMode::CLAMP_TO_BORDER,
        .anisotropy = false,
        .borderColour = canta::BorderColour::OPAQUE_BLACK_FLOAT
    });
    renderer._bilinearSampler = info.engine->device()->createSampler({
        .filter = canta::Filter::LINEAR,
        .addressMode = canta::AddressMode::CLAMP_TO_EDGE
    });

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
        })},
        .name = "cull_meshes"
    });
    renderer._writeMeshletCullCommandPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "write_mesh_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "write_mesh_command"
    });
    renderer._cullMeshletsPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "cull_meshlets.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "cull_meshlets"
    });
    renderer._writeMeshletDrawCommandPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "write_meshlet_command.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "write_meshlet_command"
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
            .compareOp = canta::CompareOp::GEQUAL
        },
        .colourFormats = { canta::Format::R32_UINT },
        .depthFormat = canta::Format::D32_SFLOAT,
        .name = "draw_meshlets_mesh_path"
    });
    renderer._drawMeshletsPipelineMeshAlphaPath = info.engine->pipelineManager().getPipeline({
        .fragment = { .module = info.engine->pipelineManager().getShader({
            .path = "visibility_buffer/visibility.frag",
            .macros = {
                canta::Macro{ "ALPHA_TEST", std::to_string(true) }
            },
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
            .compareOp = canta::CompareOp::GEQUAL
        },
        .colourFormats = { canta::Format::R32_UINT },
        .depthFormat = canta::Format::D32_SFLOAT,
        .name = "draw_meshlets_alpha_mesh_path"
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
        })},
        .name = "write_primtives"
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
            .compareOp = canta::CompareOp::GEQUAL
        },
        .colourFormats = { canta::Format::R32_UINT },
        .depthFormat = canta::Format::D32_SFLOAT,
        .name = "draw_meshlets_vertex_path"
    });
    renderer._tonemapPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "tonemap.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "tonemap"
    });
    renderer._bloomDownsamplePipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "bloom/downsample.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "bloom_downsample"
    });
    renderer._bloomUpsamplePipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "bloom/upsample.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "bloom_upsample"
    });
    renderer._bloomCompositePipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "bloom/composite.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "bloom_composite"
    });
    renderer._skyPipeline = info.engine->pipelineManager().getPipeline({
        .compute = { .module = info.engine->pipelineManager().getShader({
            .path = "sky.comp",
            .stage = canta::ShaderStage::COMPUTE
        })},
        .name = "sky"
    });

    return renderer;
}

auto cen::Renderer::render(const cen::SceneInfo &sceneInfo, canta::Swapchain* swapchain, ui::GuiWorkspace* guiWorkspace) -> canta::ImageHandle {
    auto flyingIndex = _engine->device()->flyingIndex();
    _renderGraph.reset();

    bool debugEnabled = _renderSettings.debugMeshletId ||
                        _renderSettings.debugPrimitiveId ||
                        _renderSettings.debugMeshId ||
                        _renderSettings.debugWireframe;

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
        .size = static_cast<u32>((sizeof(u32) * 2) + sizeof(MeshletInstance) * _globalData.maxMeshletCount),
        .name = "meshlet_instance_2_buffer"
    });
    auto commandResource = _renderGraph.addBuffer({
        .size = sizeof(DispatchIndirectCommand) * 2,
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
    auto hdrBackbuffer = _renderGraph.addImage({
        .format = canta::Format::RGBA32_SFLOAT,
        .name = "hdr_backbuffer"
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
        .testAlpha = false,
        .cullMeshesPipeline = _cullMeshesPipeline,
        .writeMeshletCullCommandPipeline = _writeMeshletCullCommandPipeline,
        .culLMeshletsPipeline = _cullMeshletsPipeline,
        .writeMeshletDrawCommandPipeline = _writeMeshletDrawCommandPipeline,
        .name = "cull_meshlets"
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
        .meshShadingAlphaPipeline = _drawMeshletsPipelineMeshAlphaPath,
        .writePrimitivesPipeline = _writePrimitivesPipeline,
        .vertexPipeline = _drawMeshletsPipelineVertexPath,
        .maxMeshletInstancesCount = _globalData.maxMeshletCount,
        .generatedPrimitiveCount = _globalData.maxIndirectIndexCount,
        .name = "draw_meshlets"
    });

    auto [backbufferClear] = _renderGraph.addClearPass("clear_backbuffer", debugEnabled ? backbuffer : hdrBackbuffer)
            .aliasImageOutputs<1>();

    canta::ImageIndex skyBackbuffer = _renderGraph.addAlias(debugEnabled ? backbuffer : hdrBackbuffer);
    _renderGraph.addPass("sky_pass", canta::PassType::COMPUTE)
        .addStorageImageRead(backbufferClear, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageImageWrite(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER)
        .setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto backbufferImage = graph.getImage(skyBackbuffer);
            auto globalBuffer = graph.getBuffer(globalBufferResource);

            cmd.bindPipeline(_skyPipeline);
            struct Push {
                u64 globalBuffer;
                i32 backbufferIndex;
                i32 sunIndex;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .globalBuffer = globalBuffer->address(),
                    .backbufferIndex = backbufferImage->defaultView().index(),
                    .sunIndex = sceneInfo.sunIndex
            });
            cmd.dispatchThreads(backbufferImage->width(), backbufferImage->height());
        });

    if (!debugEnabled) {
        _renderGraph.addPass("material_pass", canta::PassType::COMPUTE)

            .addStorageImageRead(visibilityBuffer, canta::PipelineStage::COMPUTE_SHADER)
            .addSampledRead(depthIndex, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageImageRead(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageBufferRead(globalBufferResource, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageBufferRead(meshletCullingOutputResource, canta::PipelineStage::COMPUTE_SHADER)

            .addStorageImageWrite(hdrBackbuffer, canta::PipelineStage::COMPUTE_SHADER)

            .setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto visibilityBufferImage = graph.getImage(visibilityBuffer);
                auto depthImage = graph.getImage(depthIndex);
                auto backbufferImage = graph.getImage(hdrBackbuffer);
                auto globalBuffer = graph.getBuffer(globalBufferResource);
                auto meshletInstanceBuffer = graph.getBuffer(meshletCullingOutputResource);


                for (auto& material : _engine->assetManager().materials()) {
                    cmd.bindPipeline(material.getVariant(Material::Variant::LIT));

                    struct Push {
                        u64 globalBuffer;
                        u64 meshletInstanceBuffer;
                        u64 materialBuffer;
                        i32 visibilityIndex;
                        i32 depthIndex;
                        i32 backbufferIndex;
                        i32 padding;
                    };
                    cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                            .globalBuffer = globalBuffer->address(),
                            .meshletInstanceBuffer = meshletInstanceBuffer->address(),
                            .materialBuffer = material.buffer()->address(),
                            .visibilityIndex = visibilityBufferImage->defaultView().index(),
                            .depthIndex = depthImage->defaultView().index(),
                            .backbufferIndex = backbufferImage->defaultView().index(),
                    });
                    cmd.dispatchThreads(backbufferImage->width(), backbufferImage->height());
                }
            });

        canta::ImageIndex bloomOutput = {};
        if (_renderSettings.bloom) {
            bloomOutput = passes::bloom(_renderGraph, {
                .mips = _renderSettings.bloomMips,
                .width = swapchain->width(),
                .height = swapchain->height(),
                .hdrBackbuffer = hdrBackbuffer,
                .globalBuffer = globalBufferResource,
                .bilinearSampler = _bilinearSampler,
                .downsamplePipeline = _bloomDownsamplePipeline,
                .upsamplePipeline = _bloomUpsamplePipeline,
                .compositePipeline = _bloomCompositePipeline
            });
        }

        _renderGraph.addPass("tonemap_pass", canta::PassType::COMPUTE)

            .addStorageBufferRead(globalBufferResource, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageImageRead(_renderSettings.bloom ? bloomOutput : hdrBackbuffer, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageImageWrite(backbuffer, canta::PipelineStage::COMPUTE_SHADER)

            .setExecuteFunction([&, bloomOutput, hdrBackbuffer] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto globalBuffer = graph.getBuffer(globalBufferResource);
                auto tonemapInputImage = graph.getImage(_renderSettings.bloom ? bloomOutput : hdrBackbuffer);
                auto backbufferImage = graph.getImage(backbuffer);

                cmd.bindPipeline(_tonemapPipeline);

                struct Push {
                    u64 globalBuffer;
                    i32 tonemapInputImage;
                    i32 backbufferIndex;
                    i32 modeIndex;
                    i32 padding;
                };
                cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .globalBuffer = globalBuffer->address(),
                    .tonemapInputImage = tonemapInputImage->defaultView().index(),
                    .backbufferIndex = backbufferImage->defaultView().index(),
                    .modeIndex = _renderSettings.tonemapModeIndex
                });
                cmd.dispatchThreads(backbufferImage->width(), backbufferImage->height());
            });
    }

    if (_renderSettings.debugMeshletId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_meshletId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .globalBuffer = globalBufferResource,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/meshletId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })},
                .name = "debug_meshletId"
            })
        }).addStorageImageRead(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER);
    }
    if (_renderSettings.debugPrimitiveId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_primitiveId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .globalBuffer = globalBufferResource,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/primitiveId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })},
                .name = "debug_primitiveId"
            })
        }).addStorageImageRead(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER);
    }
    if (_renderSettings.debugMeshId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_meshId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .globalBuffer = globalBufferResource,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/meshId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })},
                .name = "debug_meshId"
            })
        }).addStorageImageRead(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER);
    }
    if (_renderSettings.debugMaterialId) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_materialId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .globalBuffer = globalBufferResource,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/materialId.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })},
                .name = "debug_materialId"
            })
        }).addStorageImageRead(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER);
    }
    if (_renderSettings.debugFrustumIndex >= 0) {
        passes::debugFrustum(_renderGraph, {
            .backbuffer = hdrBackbuffer,
            .depth = depthIndex,
            .globalBuffer = globalBufferResource,
            .cameraBuffer = cameraResource,
            .cameraIndex = _renderSettings.debugFrustumIndex,
            .lineWidth = _renderSettings.debugLineWidth,
            .colour = _renderSettings.debugColour,
            .engine = _engine
        }).addStorageImageRead(skyBackbuffer, canta::PipelineStage::FRAGMENT_SHADER);
    }
    if (_renderSettings.debugWireframe) {
        passes::debugVisibilityBuffer(_renderGraph, {
            .name = "debug_meshId",
            .visibilityBuffer = visibilityBuffer,
            .backbuffer = backbuffer,
            .globalBuffer = globalBufferResource,
            .meshletInstanceBuffer = meshletCullingOutputResource,
            .pipeline = _engine->pipelineManager().getPipeline({
                .compute = { .module = _engine->pipelineManager().getShader({
                    .path = "debug/wireframe_solid.comp",
                    .stage = canta::ShaderStage::COMPUTE
                })},
                .name = "debug_wireframe"
            })
        }).addStorageImageRead(skyBackbuffer, canta::PipelineStage::COMPUTE_SHADER);
    }


    if (_renderSettings.mousePick) {
        _renderGraph.addPass("mouse_pick", canta::PassType::COMPUTE)

            .addStorageImageRead(visibilityBuffer, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageBufferWrite(feedbackIndex, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageImageWrite(hdrBackbuffer, canta::PipelineStage::COMPUTE_SHADER)

            .setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto visibilityBufferImage = graph.getImage(visibilityBuffer);
                auto globalBuffer = graph.getBuffer(globalBufferResource);
                auto meshletInstanceBuffer = graph.getBuffer(meshletCullingOutputResource);

                cmd.bindPipeline(_engine->pipelineManager().getPipeline({
                    .compute = { .module = _engine->pipelineManager().getShader({
                        .path = "util/mouse_pick.comp",
                        .stage = canta::ShaderStage::COMPUTE
                    })},
                    .name = "util_mouse_pick"
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
                    .visibilityBuffer = visibilityBufferImage->defaultView().index(),
                    .mouseX = _renderSettings.mouseX,
                    .mouseY = _renderSettings.mouseY
                });
                cmd.dispatchWorkgroups();
            });
    }

    if (_renderSettings.screenshotImage.id > -1) {
        _renderGraph.addPass("screenshot", canta::PassType::COMPUTE)
            .addTransferRead(_renderSettings.screenshotImage)
            .addTransferWrite(swapchainResource)
            .setExecuteFunction([&] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto image = graph.getImage(_renderSettings.screenshotImage);
                auto tmpBuffer = _engine->device()->createBuffer({
                    .size = image->size(),
                    .usage = canta::BufferUsage::TRANSFER_DST,
                    .type = canta::MemoryType::READBACK,
                    .persistentlyMapped = true
                });
                cmd.copyImageToBuffer({
                    .buffer = tmpBuffer,
                    .image = image,
                    .dstLayout = canta::ImageLayout::TRANSFER_SRC,
                    .srcOffset = 0
                });

                _engine->threadPool().addJob([] (canta::BufferHandle handle, const std::filesystem::path& path, u32 width, u32 height) {
                    sleep(1); // wait to ensure frame is finished rendering //TODO: find better method for waiting. probs add check for timeline value
                    stbi_write_jpg(path.c_str(), width, height, 4, handle->mapped().address(), 100);
                    return true;
                }, tmpBuffer, _renderSettings.screenshotPath, image->width(), image->height());
            });
    }

    if (guiWorkspace) {
        _renderGraph.addPass("ui", canta::PassType::GRAPHICS)
            .addColourRead(backbuffer)

            .addColourWrite(swapchainResource)

            .setExecuteFunction([&guiWorkspace, &swapchain](canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                guiWorkspace->context().render(ImGui::GetDrawData(), cmd, swapchain->format());
            });
    } else {
        _renderGraph.addBlitPass("backbuffer_to_swapchain", backbuffer, swapchainResource);
    }
    _renderGraph.setBackbuffer(swapchainResource, canta::ImageLayout::PRESENT);

    std::memcpy(&_feedbackInfo, _feedbackBuffers[flyingIndex]->mapped().address(), sizeof(FeedbackInfo));
    std::memset(_feedbackBuffers[flyingIndex]->mapped().address(), 0, sizeof(FeedbackInfo));

    _globalData.maxMeshCount = sceneInfo.meshCount;
    _globalData.maxMeshletCount = _globalData.maxMeshletCount = 10000000;
    _globalData.maxIndirectIndexCount = 10000000 * 3;
    _globalData.maxLightCount = sceneInfo.lightCount;
    _globalData.screenSize = { 1920, 1080 };
    _globalData.exposure = 1.0f;
    _globalData.bloomStrength = _renderSettings.bloomStrength;
    _globalData.primaryCamera = sceneInfo.primaryCamera,
    _globalData.cullingCamera = sceneInfo.cullingCamera;
    _globalData.textureSampler = _textureSampler.index();
    _globalData.depthSampler = _depthSampler.index();
    _globalData.meshBufferRef = sceneInfo.meshBuffer->address();
    _globalData.meshletBufferRef = _engine->meshletBuffer()->address();
    _globalData.vertexBufferRef = _engine->vertexBuffer()->address();
    _globalData.indexBufferRef = _engine->indexBuffer()->address();
    _globalData.primitiveBufferRef = _engine->primitiveBuffer()->address();
    _globalData.transformsBufferRef = sceneInfo.transformBuffer->address();
    _globalData.cameraBufferRef = sceneInfo.cameraBuffer->address();
    _globalData.lightBufferRef = sceneInfo.lightBuffer->address();
    _globalData.feedbackInfoRef = _feedbackBuffers[flyingIndex]->address();
    _globalBuffers[flyingIndex]->data(_globalData);

    auto result = _renderGraph.compile();
    if (!result.has_value()) {
        std::printf("cyclical graph found");
        throw "cyclical graph found";
    }

    auto waits = std::to_array({
        { _engine->device()->frameSemaphore(), _engine->device()->framePrevValue() },
        swapchain->acquireSemaphore()->getPair(),
        _engine->uploadBuffer().timeline().getPair()
    });
    auto signals = std::to_array({
        _engine->device()->frameSemaphore()->getPair(),
        swapchain->presentSemaphore()->getPair()
    });
    auto releasedImages = _engine->uploadBuffer().releasedImages();
    _renderGraph.execute(waits, signals, releasedImages);

    swapchain->present();

    _renderSettings.screenshotImage = {};
//    _renderSettings.screenshotPath =

    return _renderGraph.getImage(backbuffer);
}