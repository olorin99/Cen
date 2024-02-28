#include <Cen/ui/ProfileWindow.h>
#include <imgui.h>
#include <implot.h>
#include <Cen/Renderer.h>

cen::ui::ProfileWindow::ProfileWindow() {
    ImPlot::CreateContext();
}

void simpleGraph(std::string_view label, std::span<f32> times, u32 offset, f32 height = 60, f32 width = -1) {
    auto [min, max] = std::minmax_element(times.begin(), times.end());
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    ImGui::PlotLines("", times.data(), times.size(), offset, label.data(), 0, *max, ImVec2{ width, height });
    ImGui::PopStyleColor();
}

void plotGraph(std::string_view label, std::span<f32> times, u32 offset, f32 height = 60, f32 width = -1) {
    auto [min, max] = std::minmax_element(times.begin(), times.end());
    if (ImPlot::BeginPlot(label.data(), ImVec2{width, height})) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, *max, ImPlotCond_Always);
        ImPlot::PlotLine("", times.data(), times.size(), 1, 0, 0, offset);
        ImPlot::EndPlot();
    }
}

void cen::ui::ProfileWindow::render() {
    if (_frameTime.size() != _windowFrameCount)
        _frameTime.resize(_windowFrameCount);
    _frameTime[_frameOffset] = milliseconds;

    auto timers = renderer->renderGraph().timers();
    for (auto& timer : timers) {
        u64 time = timer.second.result().value();
        auto it = _times.find(timer.first);
        if (it == _times.end()) {
            _times.insert(std::make_pair(timer.first.c_str(), std::vector<f32>()));
        } else {
            if (it.value().size() != _windowFrameCount)
                it.value().resize(_windowFrameCount);

            f32 value = time / 1e6;
            auto& data = it.value();
            if (time == 0 || value == 0.f)
                value = data[(_frameOffset - 1) % _windowFrameCount];
            data[_frameOffset] = value;
        }
    }


    if (ImGui::Begin("Profile")) {
        ImGui::Text("Frame Offset: %d", _frameOffset);
        auto timingEnabled = renderer->renderGraph().timingEnabled();
        if (ImGui::Checkbox("RenderGraph Timing", &timingEnabled))
            renderer->renderGraph().setTimingEnabled(timingEnabled);
        ImGui::Checkbox("Individual Graphs", &_individualGraphs);
        ImGui::Checkbox("Draw Shaded", &_fill);
        ImGui::Checkbox("Show Legend", &_showLegend);
        ImGui::Checkbox("Allow Zoom", &_allowZoom);
        const char* timingModes[] = { "PER_PASS", "PER_GROUP", "SINGLE" };
        i32 timingModeIndex = static_cast<i32>(renderer->renderGraph().timingMode());
        if (ImGui::Combo("TimingMode", &timingModeIndex, timingModes, 3)) {
            switch (timingModeIndex) {
                case 0:
                    renderer->renderGraph().setTimingMode(canta::RenderGraph::TimingMode::PER_PASS);
                    break;
                case 1:
                    renderer->renderGraph().setTimingMode(canta::RenderGraph::TimingMode::PER_GROUP);
                    break;
                case 2:
                    renderer->renderGraph().setTimingMode(canta::RenderGraph::TimingMode::SINGLE);
                    break;
            }
        }
        ImGui::SliderInt("MaxFrameCount", &_windowFrameCount, 60, 60 * 10);

        ImGui::Text("Milliseconds: %f", milliseconds);
        ImGui::Text("Delta Time: %f", milliseconds / 1000.f);
        if (_individualGraphs) {
            simpleGraph("Milliseconds", _frameTime, _frameOffset, 240);

            for (auto& timer : timers) {
                auto it = _times.find(timer.first);
                if (it == _times.end() || it.value().empty())
                    continue;

                auto& data = it.value();
                simpleGraph(std::format("{} - {} ms", timer.first.c_str(), data[_frameOffset]).c_str(), data, _frameOffset, 32.f);
            }
        } else {
            if (ImPlot::BeginPlot("Frame Times", ImVec2{-1, 240}, _showLegend ? ImPlotFlags_None : ImPlotFlags_NoLegend)) {
                auto [min, max] = std::minmax_element(_frameTime.begin(), _frameTime.end());
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, _allowZoom ? 0 : ImPlotAxisFlags_AutoFit);
                if (!_allowZoom)
                    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, *max, ImPlotCond_Always);

                if (_fill) {
                    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
                    ImPlot::PlotShaded("frame time", _frameTime.data(), _frameTime.size(), 0, 1, 0, 0, _frameOffset);
                    ImPlot::PopStyleVar();
                }
                ImPlot::PlotLine("frame time", _frameTime.data(), _frameTime.size(), 1, 0, 0, _frameOffset);
                for (auto& timer : timers) {
                    auto it = _times.find(timer.first);
                    if (it == _times.end() || it.value().empty())
                        continue;

                    auto& data = it.value();
                    if (_fill) {
                        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
                        ImPlot::PlotShaded(timer.first.c_str(), data.data(), data.size(), 0, 1, 0, 0, _frameOffset);
                        ImPlot::PopStyleVar();
                    }
                    ImPlot::PlotLine(timer.first.c_str(), data.data(), data.size(), 1, 0, 0, _frameOffset);
                }
                ImPlot::EndPlot();
            }
        }
    }
    ImGui::End();

    _frameOffset = (_frameOffset + 1) % _windowFrameCount;
}