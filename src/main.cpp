#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("StreamBox"));
    QApplication::setOrganizationName(QStringLiteral("StreamBox"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont font(QStringLiteral("Microsoft YaHei UI"), 9);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    MainWindow window;
    window.show();
    const int speedIndex = app.arguments().indexOf(QStringLiteral("--speed"));
    if (speedIndex >= 0 && speedIndex + 1 < app.arguments().size())
        window.setPlaybackSpeed(app.arguments().at(speedIndex + 1).toDouble());
    const int openIndex = app.arguments().indexOf(QStringLiteral("--open"));
    if (openIndex >= 0 && openIndex + 1 < app.arguments().size())
        QTimer::singleShot(0, &window, [&window, &app, openIndex] { window.openMedia(app.arguments().at(openIndex + 1)); });
    const int seekIndex = app.arguments().indexOf(QStringLiteral("--seek"));
    if (seekIndex >= 0 && seekIndex + 1 < app.arguments().size()) {
        const qint64 seekMs = app.arguments().at(seekIndex + 1).toLongLong();
        QTimer::singleShot(900, &window, [&window, seekMs] { window.seekTo(seekMs); });
    }
    if (app.arguments().contains(QStringLiteral("--fullscreen")))
        QTimer::singleShot(1000, &window, [&window] { window.setPlayerFullscreen(true); });
    const int quitIndex = app.arguments().indexOf(QStringLiteral("--quit-after"));
    if (quitIndex >= 0 && quitIndex + 1 < app.arguments().size())
        QTimer::singleShot(app.arguments().at(quitIndex + 1).toInt(), &app, &QCoreApplication::quit);
    const QByteArray screenshotTarget = qgetenv("STREAMBOX_SCREENSHOT");
    if (!screenshotTarget.isEmpty() || app.arguments().contains(QStringLiteral("--screenshot"))) {
        const int index = app.arguments().indexOf(QStringLiteral("--screenshot"));
        const QString path = !screenshotTarget.isEmpty()
            ? QString::fromLocal8Bit(screenshotTarget)
            : (index + 1 < app.arguments().size() ? app.arguments().at(index + 1) : QDir::current().filePath(QStringLiteral("streambox-ui-preview.png")));
        const int delayIndex = app.arguments().indexOf(QStringLiteral("--screenshot-delay"));
        const int delay = delayIndex >= 0 && delayIndex + 1 < app.arguments().size()
            ? app.arguments().at(delayIndex + 1).toInt() : (openIndex >= 0 ? 1800 : 350);
        QTimer::singleShot(delay, &app, [&app, &window, path] {
            window.grab().save(path);
            app.quit();
        });
    }
    return app.exec();
}
