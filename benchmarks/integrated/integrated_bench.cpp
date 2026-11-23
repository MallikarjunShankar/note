// Integrated MainWindow benchmark: drives the real window, editor view, renderer,
// workflow and file service. Run from a Release build. With the default platform
// plugin it paints a real on-screen window; times are CPU-side event+paint costs,
// not input-to-photon latency.
#include "application/document_workflow.hpp"
#include "ui/editor_view.hpp"
#include "ui/main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double privateMiB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)))
        return static_cast<double>(counters.PrivateUsage) / (1024.0 * 1024.0);
#endif
    return -1.0;
}

double peakWorkingSetMiB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return static_cast<double>(counters.PeakWorkingSetSize) / (1024.0 * 1024.0);
#endif
    return -1.0;
}

void report(const char* name, std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const auto at = [&](double q) {
        return samples[std::min(samples.size() - 1,
                                static_cast<std::size_t>(q * static_cast<double>(samples.size())))];
    };
    std::printf("%s: n=%zu median=%.3f ms p95=%.3f ms p99=%.3f ms max=%.3f ms\n", name,
                samples.size(), at(0.5), at(0.95), at(0.99), samples.back());
}

note::ui::EditorView* activeView(note::ui::MainWindow& window) {
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("editorStack"));
    return stack ? qobject_cast<note::ui::EditorView*>(stack->currentWidget()) : nullptr;
}

bool waitFor(const std::function<bool()>& done, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (timer.elapsed() > timeoutMs) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QStandardPaths::setTestModeEnabled(true); // keep the user's settings untouched
    QApplication app(argc, argv);
    const int mib = argc > 1 ? std::max(1, std::atoi(argv[1])) : 50;
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "temporary directory");
        const auto path = directory.filePath(QStringLiteral("fixture.txt"));
        {
            // Deterministic mixed-width source-like lines, LF endings.
            QFile file(path);
            require(file.open(QIODevice::WriteOnly), "fixture open");
            const QByteArray line = "    int value_%1 = compute(alpha, beta) + 42; // comment text\n";
            QByteArray block;
            for (int i = 0; i < 1000; ++i) block += QByteArray(line).replace("%1", QByteArray::number(i));
            const qint64 target = static_cast<qint64>(mib) * 1024 * 1024;
            while (file.size() < target) file.write(block);
            file.write("unique_tail_marker\n"); // only match, at the very end
        }

        note::application::DocumentWorkflow workflow;
        note::ui::MainWindow window(workflow);
        window.resize(1280, 800);
        window.show();
        require(waitFor([&] { return window.isVisible(); }, 5000), "window visible");
        const double baseline = privateMiB();

        QElapsedTimer timer;
        timer.start();
        window.openPath(std::filesystem::path(path.toStdWString()));
        require(waitFor([&] { return workflow.documentCount() == 1 && activeView(window); }, 120000),
                "open completes");
        auto* view = activeView(window);
        view->renderer()->viewport()->repaint();
        std::printf("open %d MiB to first painted editor: %.1f ms\n", mib,
                    static_cast<double>(timer.nsecsElapsed()) / 1e6);
        std::printf("private bytes: baseline %.1f MiB, after open %.1f MiB\n", baseline,
                    privateMiB());

        view->setFocus();
        view->restoreCursorPosition(workflow.activeDocument()->buffer().sizeBytes() / 2);
        std::vector<double> keys, dispatch, paint;
        for (int i = 0; i < 300; ++i) {
            QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier,
                            QString(QChar(u'a' + i % 26)));
            timer.restart();
            QApplication::sendEvent(view->renderer(), &press);
            const auto edited = timer.nsecsElapsed();
            view->renderer()->viewport()->repaint();
            const auto total = timer.nsecsElapsed();
            keys.push_back(static_cast<double>(total) / 1e6);
            dispatch.push_back(static_cast<double>(edited) / 1e6);
            paint.push_back(static_cast<double>(total - edited) / 1e6);
        }
        report("keystroke edit+repaint (mid-document)", keys);
        report("  of which edit/model/UI refresh", dispatch);
        report("  of which viewport repaint", paint);

        // Find panel open: the UI-thread cost of typing into the pattern field
        // (one search slice) and the time until the last-line match is found.
        auto* find = window.findChild<QAction*>(QStringLiteral("search_findAction"));
        require(find, "find action");
        find->trigger();
        auto* panel = window.findChild<QDialog*>(QStringLiteral("searchPanel"));
        QLineEdit* pattern = panel ? panel->findChild<QLineEdit*>() : nullptr;
        require(panel && pattern, "search panel");
        QLabel* searchStatus = nullptr;
        for (auto* label : panel->findChildren<QLabel*>()) searchStatus = label;
        auto* regexBox = panel->findChildren<QCheckBox*>().value(2);
        require(searchStatus && regexBox, "search status and regex option");
        for (const bool regex : {false, true}) {
            regexBox->setChecked(regex);
            std::vector<double> typed, complete;
            for (int i = 0; i < 5; ++i) {
                pattern->clear();
                timer.restart();
                pattern->setText(regex ? QStringLiteral("unique_tail_mark(er)")
                                       : QStringLiteral("unique_tail_marker"));
                typed.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
                require(waitFor([&] { return !searchStatus->text().startsWith("Searching"); },
                                120000), "search completes");
                complete.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
                require(searchStatus->text() == QStringLiteral("1 match(es)"),
                        "search finds the last-line match");
            }
            report(regex ? "find pattern keystroke (regex), UI-thread time"
                         : "find pattern keystroke (plain), UI-thread time", typed);
            report(regex ? "find to last-line match (regex)" : "find to last-line match (plain)",
                   complete);
        }
        regexBox->setChecked(false);
        pattern->setText(QStringLiteral("compute"));
        panel->hide();
        view->setFocus();
        std::vector<double> searchedKeys;
        for (int i = 0; i < 100; ++i) {
            QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier,
                            QString(QChar(u'a' + i % 26)));
            timer.restart();
            QApplication::sendEvent(view->renderer(), &press);
            searchedKeys.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }
        report("keystroke edit/UI refresh with a closed search pattern set", searchedKeys);

        std::vector<double> still;
        for (int i = 0; i < 100; ++i) {
            timer.restart();
            view->renderer()->viewport()->repaint();
            still.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }
        report("unchanged repaint at the edit position", still);

        auto* bar = view->renderer()->verticalScrollBar();
        std::vector<double> scrolls;
        for (int i = 0; i < 300; ++i) {
            timer.restart();
            bar->setValue((bar->value() + bar->pageStep()) % std::max(1, bar->maximum()));
            view->renderer()->viewport()->repaint();
            scrolls.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }
        report("page scroll+repaint", scrolls);

        auto* wrap = window.findChild<QAction*>(QStringLiteral("view_wordWrapAction"));
        auto* lines = window.findChild<QAction*>(QStringLiteral("view_lineNumbersAction"));
        require(wrap && lines, "presentation actions");
        std::vector<double> toggles;
        for (int i = 0; i < 20; ++i) {
            timer.restart();
            lines->trigger();
            view->renderer()->viewport()->repaint();
            toggles.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }
        report("line-number setting toggle+repaint", toggles);
        timer.restart();
        wrap->trigger();
        view->renderer()->viewport()->repaint();
        std::printf("enable word wrap+repaint: %.1f ms\n",
                    static_cast<double>(timer.nsecsElapsed()) / 1e6);
        wrap->trigger();

        auto* save = window.findChild<QAction*>(QStringLiteral("saveDocumentAction"));
        require(save, "save action");
        timer.restart();
        save->trigger();
        require(waitFor([&] { return !workflow.activeDocument()->modified(); }, 120000),
                "save completes");
        std::printf("save %d MiB: %.1f ms\n", mib, static_cast<double>(timer.nsecsElapsed()) / 1e6);
        std::printf("private bytes after edit/save %.1f MiB, peak working set %.1f MiB\n",
                    privateMiB(), peakWorkingSetMiB());
        window.close();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "integrated benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
