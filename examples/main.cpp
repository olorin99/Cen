
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


    auto renderGraph = canta::RenderGraph::create({
        .device = engine->device(),
        .name = "RenderGraph"
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
    settingsWindow.renderGraph = &renderGraph;
    settingsWindow.swapchain = &swapchain.value();
    settingsWindow.name = "Settings";

    cen::ui::StatisticsWindow statisticsWindow = {};
    statisticsWindow.engine = engine.get();
    statisticsWindow.renderGraph = &renderGraph;
    statisticsWindow.name = "Statistics";

    cen::ui::SceneWindow sceneWindow = {};
    sceneWindow.scene = &scene;
    sceneWindow.name = "Scene";

    guiWorkspace.addWindow(&settingsWindow);
    guiWorkspace.addWindow(&statisticsWindow);
    guiWorkspace.addWindow(&sceneWindow);

    ende::thread::ThreadPool threadPool;

    cen::Model* model = nullptr;
    threadPool.addJob([&engine, &model, gltfPath] (u64 id) {
        model = engine->assetManager().loadModel(gltfPath);
    });

    threadPool.wait();
    f32 scale = 4;
    for (u32 i = 0; i < 2; i++) {
        for (u32 j = 0; j < 2; j++) {
            for (u32 k = 0; k < 1; k++) {
                for (auto& mesh : model->meshes) {
                    scene.addMesh(std::format("Mesh: ({}, {}, {})", i, j, k), mesh, cen::Transform::create({
                        .position = { static_cast<f32>(i) * scale, static_cast<f32>(j) * scale, static_cast<f32>(k) * scale }
                    }));
                }
            }
        }
    }

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

    engine->uploadBuffer().flushStagedData();
    engine->uploadBuffer().wait();
    engine->uploadBuffer().clearSubmitted();

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
                case SDL_DROPFILE:
                    char* droppedFile = event.drop.file;
                    std::filesystem::path assetPath = droppedFile;
                    threadPool.addJob([&engine, &scene, assetPath] (u64 id) {
                        auto asset = engine->assetManager().loadModel(assetPath);
                        engine->uploadBuffer().flushStagedData().wait();
                        if (!asset) return;
                        scene.addModel(assetPath.string(), *asset, cen::Transform::create({}));
                    });
                    SDL_free(droppedFile);
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
        engine->device()->gc();
        scene.prepare();

        statisticsWindow.dt = dt;
        statisticsWindow.milliseconds = milliseconds;
        guiWorkspace.render();

        renderer.render({
            .meshBuffer = scene._meshBuffer[engine->device()->flyingIndex()],
            .transformBuffer = scene._transformBuffer[engine->device()->flyingIndex()],
            .cameraBuffer = scene._cameraBuffer[engine->device()->flyingIndex()],
            .meshCount = scene.meshCount(),
            .cameraCount = (u32)scene._cameras.size(),
            .primaryCamera = (u32)scene._primaryCamera,
            .cullingCamera = (u32)scene._cullingCamera
        }, &swapchain.value(), &guiWorkspace);

        milliseconds = engine->device()->endFrame();
        dt = milliseconds / 1000.f;

    }
    engine->device()->waitIdle();
    return 0;
}