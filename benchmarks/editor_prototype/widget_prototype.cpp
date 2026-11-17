#include "text_buffers.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMainWindow>
#include <QPainter>
#include <QMessageLogContext>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextStream>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using note::prototype::PieceTable;
QString logPath;

void prototypeMessageHandler(QtMsgType, const QMessageLogContext&, const QString& message) {
    if (logPath.isEmpty()) return;
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    QTextStream output(&logFile);
    output << message << Qt::endl;
    output.flush();
}

class EditorViewport final : public QWidget {
public:
    explicit EditorViewport(QElapsedTimer& startupClock, bool benchmarkMode, QWidget* parent = nullptr)
        : QWidget(parent), startupClock_(startupClock), benchmarkMode_(benchmarkMode),
          buffer_(std::make_unique<PieceTable>()) {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        lineStarts_.push_back(0);
    }

    void loadFile(const QString& path) {
        file_.setFileName(path);
        if (!file_.open(QIODevice::ReadOnly)) {
            loadError_ = file_.errorString();
            update();
            return;
        }
        fileSize_ = file_.size();
        buffer_ = std::make_unique<PieceTable>(static_cast<std::size_t>(fileSize_));
        loadTimer_.start();
        readChunk();
    }

    bool runInteractionSelfTest() {
        buffer_ = std::make_unique<PieceTable>();
        buffer_->appendOriginal("zero\none\ntwo\n");
        for (int i = 0; i < 200; ++i) buffer_->appendOriginal("line\n");
        lineStarts_.clear();
        lineStarts_.push_back(0);
        for (std::size_t i = 0; i < buffer_->size(); ++i)
            if (buffer_->at(i) == '\n') lineStarts_.push_back(i + 1);
        cursor_ = anchor_ = firstLine_ = 0;
        updateScrollRange();

        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(this, &right);
        QKeyEvent extend(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        QApplication::sendEvent(this, &extend);
        if (cursor_ != 2 || anchor_ != 1) return false;
        QKeyEvent replace(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
        QApplication::sendEvent(this, &replace);
        if (buffer_->copy(0, 4) != "zxro") return false;
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(this, &down);
        if (lineFor(cursor_) != 1) return false;
        const auto originalLineCount = lineStarts_.size();
        moveCursor(1, false);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"));
        QApplication::sendEvent(this, &enter);
        if (lineStarts_.size() != originalLineCount + 1) return false;
        QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QApplication::sendEvent(this, &backspace);
        if (lineStarts_.size() != originalLineCount) return false;

        const int rowHeight = std::max(1, QFontMetrics(font()).lineSpacing());
        const int charWidth = std::max(1, QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')));
        const QPointF pressPoint(72 + charWidth, rowHeight / 2);
        QMouseEvent press(QEvent::MouseButtonPress, pressPoint, pressPoint,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(this, &press);
        const QPointF dragPoint(72 + charWidth * 3, rowHeight / 2);
        QMouseEvent drag(QEvent::MouseMove, dragPoint, dragPoint,
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(this, &drag);
        if (lineFor(cursor_) != 0 || std::min(cursor_, anchor_) == std::max(cursor_, anchor_)) return false;
        setFirstLine(10);
        if (firstLine_ != 10) return false;
        QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(this, &wheel);
        if (firstLine_ != 13) return false;
        repaint();
        return true;
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        firstShowMs_ = startupClock_.elapsed();
    }

    void paintEvent(QPaintEvent*) override {
        QElapsedTimer paintClock;
        paintClock.start();
        QPainter painter(this);
        painter.fillRect(rect(), QColor(24, 25, 28));
        painter.setFont(font());
        painter.setPen(QColor(224, 225, 228));

        const QFontMetrics metrics(font());
        const int rowHeight = std::max(1, metrics.lineSpacing());
        const int charWidth = std::max(1, metrics.horizontalAdvance(QLatin1Char('M')));
        const int visibleRows = height() / rowHeight + 1;
        const auto selectionStart = std::min(cursor_, anchor_);
        const auto selectionEnd = std::max(cursor_, anchor_);

        if (!loadError_.isEmpty()) {
            painter.setPen(QColor(255, 120, 120));
            painter.drawText(12, 24, loadError_);
            return;
        }

        for (int row = 0; row < visibleRows; ++row) {
            const auto line = static_cast<std::size_t>(firstLine_ + row);
            if (line >= lineStarts_.size()) break;
            const auto start = lineStarts_[line];
            const auto end = line + 1 < lineStarts_.size() ? lineStarts_[line + 1] - 1 : buffer_->size();
            const auto text = buffer_->copy(start, end - start);
            const int baseline = (row + 1) * rowHeight - metrics.descent();
            painter.setPen(QColor(112, 116, 124));
            painter.drawText(0, baseline, QString::number(line + 1));
            constexpr int gutterWidth = 72;
            painter.setPen(QColor(224, 225, 228));
            painter.drawText(gutterWidth, baseline, QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())));

            const auto visibleStart = std::max(start, selectionStart);
            const auto visibleEnd = std::min(end, selectionEnd);
            if (visibleStart < visibleEnd) {
                const int x = gutterWidth + static_cast<int>(visibleStart - start) * charWidth;
                const int width = static_cast<int>(visibleEnd - visibleStart) * charWidth;
                painter.fillRect(x, row * rowHeight, width, rowHeight, QColor(60, 89, 132, 180));
                painter.setPen(QColor(245, 246, 248));
                painter.drawText(x, baseline, QString::fromUtf8(
                    text.data() + static_cast<std::ptrdiff_t>(visibleStart - start),
                    static_cast<qsizetype>(visibleEnd - visibleStart)));
            }
            if (cursor_ >= start && cursor_ <= end) {
                const int x = gutterWidth + static_cast<int>(cursor_ - start) * charWidth;
                painter.fillRect(x, row * rowHeight + 2, 1, rowHeight - 4, QColor(255, 210, 80));
            }
        }

        const auto elapsed = paintClock.nsecsElapsed();
        if (paintSamples_.size() < 2000) paintSamples_.push_back(elapsed);
        if (!firstPaintRecorded_) {
            firstPaintRecorded_ = true;
            qInfo("prototype first paint: startup_ms=%lld paint_ms=%.3f",
                  static_cast<long long>(firstShowMs_), elapsed / 1.0e6);
        }
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        updateScrollRange();
    }

    void keyPressEvent(QKeyEvent* event) override {
        const bool extend = event->modifiers().testFlag(Qt::ShiftModifier);
        switch (event->key()) {
        case Qt::Key_Left: moveCursor(cursor_ ? cursor_ - 1 : 0, extend); break;
        case Qt::Key_Right: moveCursor(std::min(cursor_ + 1, buffer_->size()), extend); break;
        case Qt::Key_Up: moveVertical(-1, extend); break;
        case Qt::Key_Down: moveVertical(1, extend); break;
        case Qt::Key_Home: moveCursor(lineStarts_[lineFor(cursor_)], extend); break;
        case Qt::Key_End: moveCursor(lineEnd(lineFor(cursor_)), extend); break;
        case Qt::Key_Backspace:
            if (eraseSelection()) break;
            if (cursor_) { applyEdit(cursor_ - 1, 1, {}); moveCursor(cursor_ - 1, false); }
            break;
        case Qt::Key_Delete:
            if (eraseSelection()) break;
            if (cursor_ < buffer_->size()) applyEdit(cursor_, 1, {});
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter: replaceSelection("\n"); break;
        default:
            if (!event->text().isEmpty() && !event->modifiers().testFlag(Qt::ControlModifier) &&
                !event->modifiers().testFlag(Qt::AltModifier)) {
                const auto utf8 = event->text().toUtf8();
                replaceSelection(std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())));
            } else {
                QWidget::keyPressEvent(event);
            }
            break;
        }
        update();
    }

    void mousePressEvent(QMouseEvent* event) override {
        setFocus();
        const auto position = hitTest(event->position().toPoint());
        cursor_ = anchor_ = position;
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons().testFlag(Qt::LeftButton)) {
            cursor_ = hitTest(event->position().toPoint());
            update();
        }
    }

    void wheelEvent(QWheelEvent* event) override {
        const int delta = event->angleDelta().y();
        const int rows = std::max(1, std::abs(delta) / 120) * 3;
        setFirstLine(delta > 0 ? std::max(0, firstLine_ - rows) : firstLine_ + rows);
    }

private:
    void readChunk() {
        constexpr qint64 chunkSize = 256 * 1024;
        const auto data = file_.read(chunkSize);
        if (!data.isEmpty()) {
            const auto bytes = std::string_view(data.constData(), static_cast<std::size_t>(data.size()));
            const auto base = buffer_->size();
            buffer_->appendOriginal(bytes);
            for (std::size_t i = 0; i < bytes.size(); ++i)
                if (bytes[i] == '\n') lineStarts_.push_back(base + i + 1);
            updateScrollRange();
            update();
            if (firstContentMs_ < 0) {
                firstContentMs_ = startupClock_.elapsed();
                qInfo("prototype first content: startup_ms=%lld", static_cast<long long>(firstContentMs_));
            }
            if (file_.atEnd()) {
                qInfo("prototype progressive load complete: bytes=%lld load_ms=%lld",
                      static_cast<long long>(buffer_->size()), static_cast<long long>(loadTimer_.elapsed()));
                if (benchmarkMode_) QTimer::singleShot(100, this, [this] { runPrototypeBench(); });
                return;
            }
            QTimer::singleShot(0, this, [this] { readChunk(); });
        } else if (file_.atEnd()) {
            qInfo("prototype progressive load complete: bytes=%lld load_ms=%lld",
                  static_cast<long long>(buffer_->size()), static_cast<long long>(loadTimer_.elapsed()));
            if (benchmarkMode_) QTimer::singleShot(100, this, [this] { runPrototypeBench(); });
        } else {
            loadError_ = file_.errorString();
            update();
        }
    }

    std::size_t lineFor(std::size_t position) const {
        const auto it = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), position);
        return it == lineStarts_.begin() ? 0 : static_cast<std::size_t>(it - lineStarts_.begin() - 1);
    }

    std::size_t lineEnd(std::size_t line) const {
        if (line + 1 < lineStarts_.size()) return lineStarts_[line + 1] - 1;
        return buffer_->size();
    }

    void moveCursor(std::size_t position, bool extend) {
        cursor_ = std::min(position, buffer_->size());
        if (!extend) anchor_ = cursor_;
        ensureCursorVisible();
    }

    void moveVertical(int direction, bool extend) {
        const auto line = lineFor(cursor_);
        const auto column = cursor_ - lineStarts_[line];
        const auto targetSigned = static_cast<long long>(line) + direction;
        const auto target = static_cast<std::size_t>(std::clamp<long long>(targetSigned, 0,
            static_cast<long long>(lineStarts_.size() ? lineStarts_.size() - 1 : 0)));
        moveCursor(std::min(lineStarts_[target] + column, lineEnd(target)), extend);
    }

    std::size_t hitTest(const QPoint& point) const {
        const auto rowHeight = std::max(1, QFontMetrics(font()).lineSpacing());
        const auto charWidth = std::max(1, QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')));
        const int row = std::max(0, point.y() / rowHeight);
        const auto line = std::min<std::size_t>(firstLine_ + row, lineStarts_.size() - 1);
        const auto column = static_cast<std::size_t>(std::max(0, point.x() - 72) / charWidth);
        return std::min(lineStarts_[line] + column, lineEnd(line));
    }

    bool eraseSelection() {
        if (cursor_ == anchor_) return false;
        const auto start = std::min(cursor_, anchor_);
        const auto length = std::max(cursor_, anchor_) - start;
        applyEdit(start, length, {});
        cursor_ = anchor_ = start;
        return true;
    }

    void replaceSelection(std::string_view text) {
        const auto start = std::min(cursor_, anchor_);
        const auto length = std::max(cursor_, anchor_) - start;
        applyEdit(start, length, text);
        cursor_ = anchor_ = start + text.size();
        ensureCursorVisible();
    }

    void applyEdit(std::size_t start, std::size_t length, std::string_view replacement) {
        const auto oldEnd = start + length;
        buffer_->erase(start, length);
        buffer_->insert(start, replacement);
        std::vector<std::size_t> updated;
        updated.reserve(lineStarts_.size() + static_cast<std::size_t>(std::count(replacement.begin(), replacement.end(), '\n')));
        const auto beforeEnd = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), start);
        updated.insert(updated.end(), lineStarts_.begin(), beforeEnd);
        for (std::size_t i = 0; i < replacement.size(); ++i)
            if (replacement[i] == '\n') updated.push_back(start + i + 1);
        const auto afterEnd = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), oldEnd);
        for (auto it = afterEnd; it != lineStarts_.end(); ++it)
            updated.push_back(*it - length + replacement.size());
        lineStarts_.swap(updated);
        updateScrollRange();
    }

    void updateScrollRange() {
        const int pageRows = std::max(1, height() / std::max(1, QFontMetrics(font()).lineSpacing()));
        scrollMaximum_ = std::max(0, static_cast<int>(lineStarts_.size()) - pageRows);
        firstLine_ = std::clamp(firstLine_, 0, scrollMaximum_);
    }

    void setFirstLine(int line) {
        firstLine_ = std::clamp(line, 0, scrollMaximum_);
        update();
    }

    void ensureCursorVisible() {
        const auto line = static_cast<int>(lineFor(cursor_));
        const auto pageRows = std::max(1, height() / std::max(1, QFontMetrics(font()).lineSpacing()));
        if (line < firstLine_) setFirstLine(line);
        else if (line >= firstLine_ + pageRows) setFirstLine(line - pageRows + 1);
    }

    static double percentile(std::vector<double> values, double fraction) {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        const auto index = static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1));
        return values[index];
    }

    void runPrototypeBench() {
        const auto lineCount = lineStarts_.size();
        std::vector<double> latencies;
        latencies.reserve(100);
        for (int i = 0; i < 100; ++i) {
            QKeyEvent event(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
            QElapsedTimer timer;
            timer.start();
            QApplication::sendEvent(this, &event);
            repaint();
            latencies.push_back(timer.nsecsElapsed() / 1.0e6);
        }
        qInfo("prototype key_to_paint_ms: samples=%lld median=%.3f p95=%.3f",
              static_cast<long long>(latencies.size()), percentile(latencies, 0.50), percentile(latencies, 0.95));

        if (buffer_->size() >= 1024 * 1024) {
            std::vector<double> scrollLatencies;
            scrollLatencies.reserve(200);
            for (int i = 0; i < 200; ++i) {
                const auto target = (i % 2 == 0) ? scrollMaximum_ : 0;
                QElapsedTimer timer;
                timer.start();
                setFirstLine(target);
                repaint();
                scrollLatencies.push_back(timer.nsecsElapsed() / 1.0e6);
            }
            qInfo("prototype scroll_to_paint_ms: samples=%lld median=%.3f p95=%.3f",
                  static_cast<long long>(scrollLatencies.size()), percentile(scrollLatencies, 0.50),
                  percentile(scrollLatencies, 0.95));
        }
        std::vector<double> paints;
        paints.reserve(paintSamples_.size());
        for (const auto sample : paintSamples_) paints.push_back(sample / 1.0e6);
        qInfo("prototype paint_event_ms: samples=%lld median=%.3f p95=%.3f visible_lines=%lld indexed_lines=%lld",
              static_cast<long long>(paints.size()), percentile(paints, 0.50), percentile(paints, 0.95),
              static_cast<long long>(height() / std::max(1, QFontMetrics(font()).lineSpacing())),
              static_cast<long long>(lineCount));
        QTimer::singleShot(100, qApp, &QCoreApplication::quit);
    }

    QElapsedTimer& startupClock_;
    bool benchmarkMode_{};
    std::unique_ptr<PieceTable> buffer_;
    std::vector<std::size_t> lineStarts_;
    QFile file_;
    QElapsedTimer loadTimer_;
    QString loadError_;
    std::vector<qint64> paintSamples_;
    std::size_t cursor_{};
    std::size_t anchor_{};
    qint64 fileSize_{};
    qint64 firstContentMs_{-1};
    qint64 firstShowMs_{};
    int firstLine_{};
    int scrollMaximum_{};
    bool firstPaintRecorded_{};
};
} // namespace

int main(int argc, char* argv[]) {
    QElapsedTimer startupClock;
    startupClock.start();
    const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const bool benchmarkMode = mode == QStringLiteral("--bench");
    const bool holdMode = mode == QStringLiteral("--hold");
    const bool selfTestMode = mode == QStringLiteral("--self-test");
    const int logArgument = benchmarkMode ? 3 : (holdMode ? 4 : (selfTestMode ? 2 : 0));
    if ((benchmarkMode && argc > 3) || (holdMode && argc > 4) || (selfTestMode && argc > 2)) {
        logPath = QString::fromLocal8Bit(argv[logArgument]);
        QFile initialLog(logPath);
        if (initialLog.open(QIODevice::WriteOnly | QIODevice::Truncate))
            qInstallMessageHandler(prototypeMessageHandler);
    }
    QApplication app(argc, argv);
    QMainWindow window;
    auto* viewport = new EditorViewport(startupClock, benchmarkMode, &window);
    window.setCentralWidget(viewport);
    window.resize(1100, 760);
    window.setWindowTitle(QStringLiteral("Note editor architecture prototype"));
    window.show();
    if (benchmarkMode && argc > 2) viewport->loadFile(QString::fromLocal8Bit(argv[2]));
    else if (selfTestMode) {
        QTimer::singleShot(0, viewport, [viewport] {
            const bool passed = viewport->runInteractionSelfTest();
            qInfo("widget interaction self-test: %s", passed ? "PASS" : "FAIL");
            qApp->exit(passed ? 0 : 1);
        });
    }
    else if (holdMode && argc > 2) {
        viewport->loadFile(QString::fromLocal8Bit(argv[2]));
        const auto holdMs = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 10000;
        QTimer::singleShot(std::max(0, holdMs), &app, &QCoreApplication::quit);
    } else if (!benchmarkMode && !holdMode && argc > 1) {
        viewport->loadFile(QString::fromLocal8Bit(argv[1]));
    }
    return app.exec();
}
