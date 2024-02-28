
#include <Canta/SDLWindow.h>
#include <Canta/RenderGraph.h>
#include <Cen/Engine.h>
#include <Cen/Renderer.h>
#include <Cen/Camera.h>
#include <Cen/Scene.h>

#include <Cen/ui/GuiWorkspace.h>
#include <Cen/ui/SettingsWindow.h>
#include <Cen/ui/StatisticsWindow.h>
#include <Cen/ui/SceneWindow.h>
#include <Cen/ui/ViewportWindow.h>
#include <Cen/ui/RenderGraphWindow.h>
#include <Cen/ui/ProfileWindow.h>
#include <Cen/ui/AssetManagerWindow.h>

#include <Ende/thread/ThreadPool.h>

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
    auto renderer = cen::Renderer::create({
        .engine = engine.get(),
        .swapchainFormat = swapchain->format()
    });
    auto scene = cen::Scene::create({
        .engine = engine.get()
    });

    auto guiWorkspace = cen::ui::GuiWorkspace::create({
        .engine = engine.get(),
        .window = &window
    });
    cen::ui::SettingsWindow settingsWindow = {};
    settingsWindow.engine = engine.get();
    settingsWindow.renderer = &renderer;
    settingsWindow.swapchain = &swapchain.value();
    settingsWindow.name = "Settings";

    cen::ui::StatisticsWindow statisticsWindow = {};
    statisticsWindow.engine = engine.get();
    statisticsWindow.renderer = &renderer;
    statisticsWindow.name = "Statistics";

    cen::ui::SceneWindow sceneWindow = {};
    sceneWindow.scene = &scene;
    sceneWindow.name = "Scene";

    cen::ui::ViewportWindow viewportWindow = {};
    viewportWindow._renderer = &renderer;
    viewportWindow.name = "Viewport";

    cen::ui::RenderGraphWindow renderGraphWindow = {};
    renderGraphWindow.engine = engine.get();
    renderGraphWindow.renderGraph = &renderer.renderGraph();
    renderGraphWindow.name = "RenderGraph";

    cen::ui::ProfileWindow profileWindow = {};
    profileWindow.renderer = &renderer;
    profileWindow.name = "Profiling";

    cen::ui::AssetManagerWindow assetManagerWindow = {};
    assetManagerWindow.assetManager = &engine->assetManager();
    assetManagerWindow.pipelineManager = &engine->pipelineManager();
    assetManagerWindow.name = "Asset Manager";

    guiWorkspace.addWindow(&settingsWindow);
    guiWorkspace.addWindow(&statisticsWindow);
    guiWorkspace.addWindow(&sceneWindow);
    guiWorkspace.addWindow(&renderGraphWindow);
    guiWorkspace.addWindow(&viewportWindow);
    guiWorkspace.addWindow(&profileWindow);
    guiWorkspace.addWindow(&assetManagerWindow);

    auto camera = cen::Camera::create({
        .position = { 0, 0, 2 },
        .rotation = ende::math::Quaternion({ 0, 0, 1 }, ende::math::rad(180)),
        .width = 1920,
        .height = 1080
    });

    camera.updateFrustum();
    scene.addCamera("primary_camera", camera, cen::Transform::create({
        .position = { 0, 0, 2 },
        .rotation = ende::math::Quaternion({ 0, 0, 1 }, ende::math::rad(180))
    }));

    auto secondaryCamera = cen::Camera::create({
        .position = { 0, 0, 2 },
        .rotation = ende::math::Quaternion({ 0, 0, 1 }, ende::math::rad(180)),
        .width = 1920,
        .height = 1080
    });
    scene.addCamera("secondary_camera", secondaryCamera, cen::Transform::create({
        .position = { 0, 0, 2 },
        .rotation = ende::math::Quaternion({ 0, 0, 1 }, ende::math::rad(180))
    }));

    scene.addLight("light", cen::Light::create({
        .intensity = 20,
        .radius = 100,
    }), cen::Transform::create({}));

//    auto material = engine->assetManager().loadMaterial("materials/blinn_phong/blinn_phong.mat");
    auto material = engine->assetManager().loadMaterial("materials/pbr/pbr.mat");

    ende::thread::ThreadPool threadPool;

    cen::Asset<cen::Model> model = {};
    ende::math::Vec3f offset = { 0, -2, 0 };
    threadPool.addJob([&engine, &scene, &model, gltfPath, &material, offset] (u64 id) {
        model = engine->assetManager().loadModel(gltfPath, material);
        auto rootNode = scene.addNode("mesh_root");
        f32 scale = 4;
        for (u32 i = 0; i < 1; i++) {
            for (u32 j = 0; j < 1; j++) {
                for (u32 k = 0; k < 1; k++) {
                    for (auto& mesh : model->meshes) {
                        scene.addMesh(std::format("Mesh: ({}, {}, {})", i, j, k), mesh, cen::Transform::create({
                            .position = ende::math::Vec3f{ static_cast<f32>(i) * scale, static_cast<f32>(j) * scale, static_cast<f32>(k) * scale } + offset
                        }), rootNode);
                    }
                }
            }
        }
    });

//    auto rootNode = scene.addNode("mesh_root");

//    threadPool.wait();
//    f32 scale = 4;
//    for (u32 i = 0; i < 1; i++) {
//        for (u32 j = 0; j < 1; j++) {
//            for (u32 k = 0; k < 1; k++) {
//                for (auto& mesh : model->meshes) {
//                    scene.addMesh(std::format("Mesh: ({}, {}, {})", i, j, k), mesh, cen::Transform::create({
//                        .position = { static_cast<f32>(i) * scale, static_cast<f32>(j) * scale, static_cast<f32>(k) * scale }
//                    }), rootNode);
//                }
//            }
//        }
//    }

    engine->uploadBuffer().flushStagedData();
    engine->uploadBuffer().wait();
    engine->uploadBuffer().clearSubmitted();
    engine->assetManager().uploadMaterials();

    canta::ImageHandle backbufferImage = {};

    cen::ui::GuiWorkspace* guiPointer = &guiWorkspace;

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
                case SDL_DROPFILE: {
                    char* droppedFile = event.drop.file;
                    std::filesystem::path assetPath = droppedFile;
                    threadPool.addJob([&engine, &scene, assetPath, &material] (u64 id) {
                        auto asset = engine->assetManager().loadModel(assetPath, material);
                        engine->uploadBuffer().flushStagedData().wait();
                        if (!asset) return;
                        scene.addModel(assetPath.string(), *asset, cen::Transform::create({}));
                    });
                    SDL_free(droppedFile);
                }
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_U:
                            if (guiPointer)
                                guiPointer = nullptr;
                            else
                                guiPointer = &guiWorkspace;
                            break;
                        case SDL_SCANCODE_P:
                            if (backbufferImage) {
                                engine->saveImageToDisk(backbufferImage, "backbuffer_screenshot.jpg", canta::ImageLayout::COLOUR_ATTACHMENT);
                            }
                            break;
                    }
                    break;
            }
            guiWorkspace.context().processEvent(&event);
        }

        {
            auto cameraPosition = scene.primaryCamera().position();
            auto cameraRotation = scene.primaryCamera().rotation().unit();
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W])
                scene.primaryCamera().setPosition(cameraPosition + cameraRotation.front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                scene.primaryCamera().setPosition(cameraPosition + cameraRotation.back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                scene.primaryCamera().setPosition(cameraPosition + cameraRotation.left() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                scene.primaryCamera().setPosition(cameraPosition + cameraRotation.right() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                scene.primaryCamera().setPosition(cameraPosition + cameraRotation.down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                scene.primaryCamera().setPosition(cameraPosition + cameraRotation.up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                scene.primaryCamera().setRotation(ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(90) * dt) * cameraRotation);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                scene.primaryCamera().setRotation(ende::math::Quaternion({ 0, 1, 0 }, ende::math::rad(-90) * dt) * cameraRotation);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                scene.primaryCamera().setRotation(ende::math::Quaternion(cameraRotation.right(), ende::math::rad(-45) * dt) * cameraRotation);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                scene.primaryCamera().setRotation(ende::math::Quaternion(cameraRotation.right(), ende::math::rad(45) * dt) * cameraRotation);
        }

        engine->device()->beginFrame();
        engine->gc();
        auto sceneInfo = scene.prepare();

        statisticsWindow.dt = dt;
        statisticsWindow.milliseconds = milliseconds;
        profileWindow.milliseconds = milliseconds;
        viewportWindow.setBackbuffer(backbufferImage);

        settingsWindow.cameraCount = scene._cameras.size();
        guiWorkspace.render();

        backbufferImage = renderer.render(sceneInfo, &swapchain.value(), guiPointer);

        milliseconds = engine->device()->endFrame();
        dt = milliseconds / 1000.f;

    }
    engine->device()->waitIdle();
    return 0;
}