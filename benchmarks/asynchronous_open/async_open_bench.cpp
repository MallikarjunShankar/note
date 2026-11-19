#include "application/document_workflow.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {

struct ProcessMemory {
    std::uint64_t workingSetBytes{};
    std::uint64_t privateBytes{};
    bool available{};
};

ProcessMemory processMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)))
        return {};
    return {counters.WorkingSetSize, counters.PrivateUsage, true};
#else
    return {};
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::uint64_t fixtureMiB = 50;
    if (argc > 2) throw std::runtime_error("usage: note_async_open_bench [size_mib]");
    if (argc == 2) {
        std::size_t parsed = 0;
        fixtureMiB = std::stoull(argv[1], &parsed);
        if (argv[1][parsed] != '\0' || fixtureMiB == 0 || fixtureMiB > 1024)
            throw std::runtime_error("size_mib must be between 1 and 1024");
    }

    QTemporaryDir temporary;
    if (!temporary.isValid()) throw std::runtime_error("temporary directory unavailable");
    const auto path = temporary.filePath(QStringLiteral("fixture.txt"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) throw std::runtime_error("could not create fixture");
    QByteArray block(1024 * 1024, 'x');
    for (qsizetype i = 79; i < block.size(); i += 80) block[i] = '\n';
    const auto target = static_cast<qsizetype>(fixtureMiB * 1024 * 1024);
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
    std::size_t heartbeatCountAtCompletion = 0;
    qint64 firstProgress = -1;
    qint64 completion = -1;
    bool openSucceeded = false;
    std::string openError;
    ProcessMemory peakMemory;
    const auto baselineMemory = processMemory();
    const auto samplePeakMemory = [&] {
        const auto sample = processMemory();
        if (sample.available &&
            (!peakMemory.available ||
             sample.workingSetBytes > peakMemory.workingSetBytes ||
             sample.privateBytes > peakMemory.privateBytes)) {
            peakMemory.workingSetBytes =
                std::max(peakMemory.workingSetBytes, sample.workingSetBytes);
            peakMemory.privateBytes =
                std::max(peakMemory.privateBytes, sample.privateBytes);
            peakMemory.available = true;
        }
    };
    QObject::connect(&heartbeat, &QTimer::timeout, &loop, [&] {
        const auto now = timer.elapsed();
        if (completion < 0 && heartbeatCount != 0)
            maximumBeatGap = std::max(maximumBeatGap, now - lastBeat);
        lastBeat = now;
        ++heartbeatCount;
    });
    QTimer memorySampler;
    memorySampler.setInterval(20);
    QObject::connect(&memorySampler, &QTimer::timeout, &loop, samplePeakMemory);
    heartbeat.start();
    memorySampler.start();
    timer.start();
    const auto request = workflow.openAsync(std::filesystem::path(path.toStdWString()),
        [&](auto, note::documents::OpenProgress update) {
            if (!update.buildingIndex && firstProgress < 0) firstProgress = timer.elapsed();
        },
        [&](auto, note::application::WorkflowResult result) {
            openSucceeded = static_cast<bool>(result);
            if (!openSucceeded) openError = result.error;
            completion = timer.elapsed();
            heartbeatCountAtCompletion = heartbeatCount;
            loop.quit();
        });
    if (request == 0) throw std::runtime_error("open request was not accepted");
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(180000);
    loop.exec();
    if (completion < 0) throw std::runtime_error("fixture open timed out");
    if (!openSucceeded) throw std::runtime_error("fixture open failed: " + openError);
    if (workflow.documentCount() != 1)
        throw std::runtime_error("fixture open did not publish exactly one document");
    QEventLoop settlingLoop;
    QTimer::singleShot(500, &settlingLoop, &QEventLoop::quit);
    settlingLoop.exec();
    heartbeat.stop();
    memorySampler.stop();
    const auto finalMemory = processMemory();
    std::size_t bufferStorageBytes = 0;
    if (const auto* document = workflow.activeDocument())
        bufferStorageBytes = document->buffer().estimatedStorageBytes();

    std::cout << "fixture_mib=" << fixtureMiB << '\n'
              << "fixture_bytes=" << target << '\n'
              << "qt_version=" << qVersion() << '\n'
              << "first_progress_ms=" << firstProgress << '\n'
              << "open_complete_ms=" << completion << '\n'
              << "heartbeat_count_during_open=" << heartbeatCountAtCompletion << '\n'
              << "max_heartbeat_gap_ms=" << maximumBeatGap << '\n'
              << "documents_published=" << workflow.documentCount() << '\n'
              << "buffer_estimated_storage_bytes=" << bufferStorageBytes << '\n';
    if (baselineMemory.available && peakMemory.available && finalMemory.available) {
        std::cout << "working_set_baseline_bytes=" << baselineMemory.workingSetBytes << '\n'
                  << "working_set_peak_bytes=" << peakMemory.workingSetBytes << '\n'
                  << "working_set_after_settle_bytes=" << finalMemory.workingSetBytes << '\n'
                  << "private_bytes_baseline=" << baselineMemory.privateBytes << '\n'
                  << "private_bytes_peak=" << peakMemory.privateBytes << '\n'
                  << "private_bytes_after_settle=" << finalMemory.privateBytes << '\n';
    } else {
        std::cout << "process_memory=unavailable\n";
    }
    workflow.shutdown();
}
