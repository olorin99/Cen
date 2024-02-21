#include <Cen/ui/GuiWorkspace.h>
#include <Cen/Engine.h>

auto cen::ui::GuiWorkspace::create(cen::ui::GuiWorkspace::CreateInfo info) -> GuiWorkspace {
    GuiWorkspace workspace = {};

    workspace._context = canta::ImGuiContext::create({
        .device = info.engine->device(),
        .window = info.window
    });

    return workspace;
}

void cen::ui::GuiWorkspace::render() {
    _context.beginFrame();

    {
        auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));

        ImGui::Begin("Main", nullptr, windowFlags);

        if (ImGui::BeginMainMenuBar()) {

            if (ImGui::BeginMenu("View")) {

                for (auto& window : _windows) {
                    ImGui::MenuItem(window->name.c_str(), nullptr, &window->enabled, true);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceID = ImGui::GetID("dockspace");
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(dockspaceID, {0, 0}, dockspaceFlags);
        ImGui::End();
    }

    for (auto& window : _windows) {
        if (window->enabled)
            window->render();
    }

    ImGui::Render();
}

void cen::ui::GuiWorkspace::addWindow(cen::ui::Window *window) {
    _windows.push_back(window);
}