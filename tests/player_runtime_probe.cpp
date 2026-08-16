#include "player/playerengine.h"

#include <QCoreApplication>
#include <QImage>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) {
        QTextStream(stderr) << "Usage: player_runtime_probe <media-source>\n";
        return 64;
    }

    PlayerEngine engine;
    QTimer timeout;
    timeout.setSingleShot(true);
    int videoFrames = 0;
    int framesAtPause = 0;
    qint64 pausedPosition = -1;
    bool pauseRequested = false;
    bool resumeRequested = false;
    bool failed = false;

    QObject::connect(&engine, &PlayerEngine::videoFrameReady, &app, [&](const QImage &image) {
        if (!image.isNull()) ++videoFrames;
    });
    QObject::connect(&engine, &PlayerEngine::errorOccurred, &app,
        [&](const QString &message, const QString &details) {
            QTextStream(stderr) << message << ": " << details << '\n';
            failed = true;
            app.exit(2);
        });
    QObject::connect(&engine, &PlayerEngine::positionChanged, &app, [&](qint64 position) {
        if (!pauseRequested && videoFrames > 0 && position >= 2000) {
            pauseRequested = true;
            engine.pause();
            pausedPosition = engine.position();
            framesAtPause = videoFrames;
            QTimer::singleShot(700, &app, [&] {
                const qint64 drift = qAbs(engine.position() - pausedPosition);
                if (engine.state() != PlayerEngine::State::Paused || drift > 250
                    || videoFrames != framesAtPause || pausedPosition <= 0) {
                    QTextStream(stderr) << "pause failed position=" << pausedPosition
                        << " current=" << engine.position() << " drift=" << drift
                        << " frames=" << framesAtPause << "->" << videoFrames << '\n';
                    failed = true;
                    app.exit(3);
                    return;
                }
                resumeRequested = true;
                engine.play();
            });
            return;
        }
        if (resumeRequested && position >= pausedPosition + 500 && videoFrames > framesAtPause) {
            QTextStream(stdout) << "passed videoFrames=" << videoFrames
                << " pausedPosition=" << pausedPosition
                << " resumedPosition=" << position << '\n';
            app.quit();
        }
    });
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        QTextStream(stderr) << "runtime probe timeout state=" << int(engine.state())
            << " position=" << engine.position() << " videoFrames=" << videoFrames << '\n';
        failed = true;
        app.exit(4);
    });

    timeout.start(20000);
    engine.open(app.arguments().at(1));
    const int result = app.exec();
    engine.stop();
    return failed ? (result == 0 ? 1 : result) : result;
}
