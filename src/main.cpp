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
    const int openIndex = app.arguments().indexOf(QStringLiteral("--open"));
    if (openIndex >= 0 && openIndex + 1 < app.arguments().size())
        QTimer::singleShot(0, &window, [&window, &app, openIndex] { window.openMedia(app.arguments().at(openIndex + 1)); });
    const QByteArray screenshotTarget = qgetenv("STREAMBOX_SCREENSHOT");
    if (!screenshotTarget.isEmpty() || app.arguments().contains(QStringLiteral("--screenshot"))) {
        const int index = app.arguments().indexOf(QStringLiteral("--screenshot"));
        const QString path = !screenshotTarget.isEmpty()
            ? QString::fromLocal8Bit(screenshotTarget)
            : (index + 1 < app.arguments().size() ? app.arguments().at(index + 1) : QDir::current().filePath(QStringLiteral("streambox-ui-preview.png")));
        QTimer::singleShot(openIndex >= 0 ? 1800 : 350, &app, [&app, &window, path] {
            window.grab().save(path);
            app.quit();
        });
    }
    return app.exec();
}
