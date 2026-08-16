#include "player/playerengine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) {
        qCritical() << "Usage: media_smoke <media-source>";
        return 64;
    }

    DecoderWorker worker;
    bool opened = false;
    bool expectedAudio = false;
    bool expectedVideo = false;
    bool producedAudio = false;
    bool producedVideo = false;
    bool stopping = false;
    auto completeIfReady = [&] {
        if (!opened || stopping) return;
        if ((expectedAudio && !producedAudio) || (expectedVideo && !producedVideo)) return;
        stopping = true;
        worker.requestStop();
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    };

    QObject::connect(&worker, &DecoderWorker::mediaOpened, &app,
        [&](qint64 duration, bool audio, bool video, const QString &description) {
            opened = audio || video;
            expectedAudio = audio;
            expectedVideo = video;
            const QByteArray line = QStringLiteral("opened duration=%1 audio=%2 video=%3 %4\n")
                .arg(duration).arg(audio).arg(video).arg(description).toUtf8();
            std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
            std::fflush(stdout);
            completeIfReady();
        });
    QObject::connect(&worker, &DecoderWorker::audioDataReady, &app, [&](const QByteArray &data) {
        producedAudio = producedAudio || !data.isEmpty(); completeIfReady();
    });
    QObject::connect(&worker, &DecoderWorker::videoFrameReady, &app, [&](const QImage &image) {
        producedVideo = producedVideo || !image.isNull(); completeIfReady();
    });
    QObject::connect(&worker, &DecoderWorker::playbackError, &app, [&](const QString &message, const QString &details) {
        qCritical().noquote() << message << details; stopping = true; app.exit(2);
    });
    QObject::connect(&worker, &DecoderWorker::playbackFinished, &app, [&] {
        if (!opened || (expectedAudio && !producedAudio) || (expectedVideo && !producedVideo)) { stopping = true; app.exit(3); }
        else { stopping = true; app.quit(); }
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        qCritical() << "media smoke timeout"; stopping = true; worker.requestStop(); app.exit(4);
    });
    timeout.start(10000);
    worker.open(app.arguments().at(1));
    const int result = app.exec();
    worker.requestStop(); worker.wait(2000);
    return result;
}
