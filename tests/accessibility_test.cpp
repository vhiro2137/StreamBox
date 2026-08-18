#include "mainwindow.h"
#include "playerwidgets.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QListWidget>
#include <QtTest>

class AccessibilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void allInteractiveControlsHaveAccessibleNames()
    {
        MainWindow window;
        const auto buttons = window.findChildren<QAbstractButton *>();
        const auto sliders = window.findChildren<QAbstractSlider *>();
        const auto lists = window.findChildren<QListWidget *>();

        QVERIFY(!buttons.isEmpty());
        QVERIFY(!sliders.isEmpty());
        QVERIFY(!lists.isEmpty());
        for (const QAbstractButton *button : buttons)
            if (button->focusPolicy() != Qt::NoFocus) QVERIFY2(!button->accessibleName().trimmed().isEmpty(), qPrintable(button->objectName()));
        for (const QAbstractSlider *slider : sliders)
            if (slider->focusPolicy() != Qt::NoFocus) QVERIFY2(!slider->accessibleName().trimmed().isEmpty(), qPrintable(slider->objectName()));
        for (const QListWidget *list : lists)
            if (list->focusPolicy() != Qt::NoFocus) QVERIFY2(!list->accessibleName().trimmed().isEmpty(), qPrintable(list->objectName()));
    }

    void canvasIsNotAnEmptyTabStop()
    {
        MainWindow window;
        auto *canvas = window.findChild<VideoCanvas *>(QStringLiteral("videoCanvas"));
        QVERIFY(canvas);
        QCOMPARE(canvas->focusPolicy(), Qt::NoFocus);
    }

    void speedNameTracksProgrammaticChange()
    {
        MainWindow window;
        window.setPlaybackSpeed(1.25);
        auto *speed = window.findChild<QPushButton *>(QStringLiteral("speedButton"));
        QVERIFY(speed);
        QCOMPARE(speed->accessibleName(), QStringLiteral("播放速度 1.25 倍"));
    }
};

QTEST_MAIN(AccessibilityTest)
#include "accessibility_test.moc"
