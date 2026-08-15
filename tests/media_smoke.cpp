#include "player/playerengine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) {
        qCritical() << "Usage: media_smoke <media-source>";
        return 64;
    }

    DecoderWorker worker;
    bool opened = false;
    bool producedMedia = false;
    bool stopping = false;
    auto completeIfReady = [&] {
        if (!opened || !producedMedia || stopping) return;
        stopping = true;
        worker.requestStop();
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    };

    QObject::connect(&worker, &DecoderWorker::mediaOpened, &app,
        [&](qint64 duration, bool audio, bool video, const QString &description) {
            opened = audio || video;
            qInfo().noquote() << QStringLiteral("opened duration=%1 audio=%2 video=%3 %4")
                .arg(duration).arg(audio).arg(video).arg(description);
            completeIfReady();
        });
    QObject::connect(&worker, &DecoderWorker::audioDataReady, &app, [&](const QByteArray &data) {
        producedMedia = producedMedia || !data.isEmpty(); completeIfReady();
    });
    QObject::connect(&worker, &DecoderWorker::videoFrameReady, &app, [&](const QImage &image) {
        producedMedia = producedMedia || !image.isNull(); completeIfReady();
    });
    QObject::connect(&worker, &DecoderWorker::playbackError, &app, [&](const QString &message, const QString &details) {
        qCritical().noquote() << message << details; stopping = true; app.exit(2);
    });
    QObject::connect(&worker, &DecoderWorker::playbackFinished, &app, [&] {
        if (!opened || !producedMedia) { stopping = true; app.exit(3); }
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
