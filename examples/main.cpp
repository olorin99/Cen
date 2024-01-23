
#include <Canta/SDLWindow.h>
#include <Canta/RenderGraph.h>
#include <Canta/ImGuiContext.h>
#include <Cen/Engine.h>

int main() {

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

    std::array<canta::CommandPool, canta::FRAMES_IN_FLIGHT> commandPools = {};
    for (auto& pool : commandPools) {
        pool = engine.device()->createCommandPool({
            .queueType = canta::QueueType::GRAPHICS
        }).value();
    }

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
        auto flyingIndex = engine.device()->flyingIndex();


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

        commandPools[flyingIndex].reset();
        auto& commandBuffer = commandPools[flyingIndex].getBuffer();
        commandBuffer.begin();

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
        renderGraph.execute(commandBuffer);

        commandBuffer.barrier({
            .image = swapchainImage.value(),
            .srcStage = canta::PipelineStage::COLOUR_OUTPUT,
            .dstStage = canta::PipelineStage::BOTTOM,
            .srcAccess = canta::Access::COLOUR_WRITE | canta::Access::COLOUR_READ,
            .dstAccess = canta::Access::MEMORY_WRITE | canta::Access::MEMORY_READ,
            .srcLayout = canta::ImageLayout::COLOUR_ATTACHMENT,
            .dstLayout = canta::ImageLayout::PRESENT
        });

        commandBuffer.end();

        auto waits = std::to_array({
            { engine.device()->frameSemaphore(), engine.device()->framePrevValue() },
            swapchain->acquireSemaphore()->getPair()
        });
        auto signals = std::to_array({
            engine.device()->frameSemaphore()->getPair(),
            swapchain->presentSemaphore()->getPair()
        });
        commandBuffer.submit(waits, signals);
        swapchain->present();

        engine.device()->endFrame();


    }
    engine.device()->waitIdle();
    return 0;
}