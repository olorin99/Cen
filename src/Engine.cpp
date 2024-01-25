#include "../include/Cen/Engine.h"

auto cen::Engine::create(CreateInfo info) -> Engine {
    Engine engine = {};

    engine._device = canta::Device::create({
        .applicationName = "Cen",
        .enableMeshShading = true,
        .instanceExtensions = info.window->requiredExtensions(),
    }).value();
    engine._pipelineManager = canta::PipelineManager::create({
        .device = engine.device(),
        .rootPath = info.assetPath
    });

    return engine;
}