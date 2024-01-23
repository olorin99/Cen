#include "../include/Cen/Engine.h"

auto cen::Engine::create(CreateInfo info) -> Engine {
    Engine engine = {};

    engine._device = canta::Device::create({
        .applicationName = "Cen",
        .instanceExtensions = info.window->requiredExtensions()
    }).value();

    return engine;
}