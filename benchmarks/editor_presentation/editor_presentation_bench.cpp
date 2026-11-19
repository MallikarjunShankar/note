#include "editor/cursor_manager.hpp"
#include "editor/selection_manager.hpp"
#include "editor/text_buffer.hpp"
#include "ui/editor_renderer.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    constexpr std::size_t lineCount = 100000;
    constexpr int warmupFrames = 10;
    constexpr int measuredFrames = 100;

    std::string contents;
    contents.reserve(lineCount * 48);
    for (std::size_t line = 0; line < lineCount; ++line)
        contents += "    const value = alpha + beta;\t// alpha note\n";

    note::editor::TextBuffer buffer(contents);
    note::editor::CursorManager cursors(buffer);
    note::editor::SelectionManager selections(buffer);
    cursors.setPrimaryCursor(4);
    note::ui::EditorRenderer renderer(buffer, cursors, selections);
    renderer.resize(1024, 768);
    auto configuration = renderer.configuration();
    configuration.indentationGuides = true;
    configuration.showWhitespace = true;
    configuration.matchingOccurrences = true;
    configuration.wordWrap = true;
    renderer.setConfiguration(configuration);
    renderer.show();
    application.processEvents();

    QImage frame(renderer.viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    for (int frameIndex = 0; frameIndex < warmupFrames; ++frameIndex)
        renderer.viewport()->render(&frame);

    std::vector<qint64> durations;
    durations.reserve(measuredFrames);
    for (int frameIndex = 0; frameIndex < measuredFrames; ++frameIndex) {
        QElapsedTimer timer;
        timer.start();
        renderer.viewport()->render(&frame);
        durations.push_back(timer.nsecsElapsed());
    }
    std::sort(durations.begin(), durations.end());

    const auto percentile = [&durations](double fraction) {
        const auto index = static_cast<std::size_t>(
            fraction * static_cast<double>(durations.size() - 1));
        return static_cast<double>(durations[index]) / 1'000'000.0;
    };
    std::cout << "lines=" << buffer.lineCount()
              << " bytes=" << buffer.sizeBytes()
              << " viewport_rows=" << renderer.lastPaintedVisualRowCount()
              << " frames=" << measuredFrames
              << " median_ms=" << percentile(0.50)
              << " p95_ms=" << percentile(0.95) << '\n';
}
