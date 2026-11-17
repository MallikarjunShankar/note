#include "application/document_workflow.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) throw std::runtime_error("temporary directory unavailable");
    const auto path = temporary.filePath(QStringLiteral("fifty_mib.txt"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) throw std::runtime_error("could not create fixture");
    QByteArray block(1024 * 1024, 'x');
    for (qsizetype i = 79; i < block.size(); i += 80) block[i] = '\n';
    constexpr qsizetype target = 50 * 1024 * 1024;
    for (qsizetype written = 0; written < target; written += block.size()) {
        if (file.write(block) != block.size()) throw std::runtime_error("could not write fixture");
    }
    file.close();

    note::application::DocumentWorkflow workflow;
    QEventLoop loop;
    QTimer heartbeat;
    heartbeat.setInterval(1);
    QElapsedTimer timer;
    qint64 lastBeat = 0;
    qint64 maximumBeatGap = 0;
    std::size_t heartbeatCount = 0;
    qint64 firstProgress = -1;
    qint64 completion = -1;
    QObject::connect(&heartbeat, &QTimer::timeout, &loop, [&] {
        const auto now = timer.elapsed();
        if (heartbeatCount != 0) maximumBeatGap = std::max(maximumBeatGap, now - lastBeat);
        lastBeat = now;
        ++heartbeatCount;
    });
    heartbeat.start();
    timer.start();
    const auto request = workflow.openAsync(std::filesystem::path(path.toStdWString()),
        [&](auto, note::documents::OpenProgress update) {
            if (!update.buildingIndex && firstProgress < 0) firstProgress = timer.elapsed();
        },
        [&](auto, note::application::WorkflowResult result) {
            if (!result) std::cerr << "open error: " << result.error << '\n';
            completion = timer.elapsed();
            loop.quit();
        });
    if (request == 0) throw std::runtime_error("open request was not accepted");
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(180000);
    loop.exec();
    heartbeat.stop();
    if (completion < 0) throw std::runtime_error("50 MiB open timed out");
    std::cout << "fixture_bytes=" << target << '\n'
              << "qt_version=" << qVersion() << '\n'
              << "first_progress_ms=" << firstProgress << '\n'
              << "open_complete_ms=" << completion << '\n'
              << "heartbeat_count=" << heartbeatCount << '\n'
              << "max_heartbeat_gap_ms=" << maximumBeatGap << '\n'
              << "documents_published=" << workflow.documentCount() << '\n';
    workflow.shutdown();
}
