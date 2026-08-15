#include "player/playbackpolicy.h"

#include <QtTest>

class PlaybackPolicyTest final : public QObject
{
    Q_OBJECT

private slots:
    void retryBackoff()
    {
        QCOMPARE(PlaybackPolicy::retryDelayMs(1), 1000);
        QCOMPARE(PlaybackPolicy::retryDelayMs(2), 2000);
        QCOMPARE(PlaybackPolicy::retryDelayMs(3), 4000);
        QCOMPARE(PlaybackPolicy::retryDelayMs(9), 4000);
    }

    void buffering()
    {
        QCOMPARE(PlaybackPolicy::bufferPercent(0, 3000), 0);
        QCOMPARE(PlaybackPolicy::bufferPercent(1500, 3000), 50);
        QCOMPARE(PlaybackPolicy::bufferPercent(9000, 3000), 100);
        QCOMPARE(PlaybackPolicy::bufferPercent(1, 0), -1);
    }

    void seekability()
    {
        QVERIFY(PlaybackPolicy::isSeekable(1000, true));
        QVERIFY(!PlaybackPolicy::isSeekable(0, true));
        QVERIFY(!PlaybackPolicy::isSeekable(1000, false));
    }

    void videoSync()
    {
        QCOMPARE(PlaybackPolicy::videoDecision(900, 1000).action, PlaybackPolicy::VideoAction::Drop);
        const auto delayed = PlaybackPolicy::videoDecision(1060, 1000);
        QCOMPARE(delayed.action, PlaybackPolicy::VideoAction::Delay);
        QCOMPARE(delayed.delayMs, 60);
        QCOMPARE(PlaybackPolicy::videoDecision(1005, 1000).action, PlaybackPolicy::VideoAction::Display);
    }

    void playlistNavigation()
    {
        using Mode = PlaybackPolicy::PlaylistMode;
        QCOMPARE(PlaybackPolicy::nextIndex(0, 3, Mode::Ordered), 1);
        QCOMPARE(PlaybackPolicy::nextIndex(2, 3, Mode::Ordered), -1);
        QCOMPARE(PlaybackPolicy::nextIndex(2, 3, Mode::RepeatAll), 0);
        QCOMPARE(PlaybackPolicy::nextIndex(1, 3, Mode::RepeatOne), 1);
        QCOMPARE(PlaybackPolicy::previousIndex(1, 3, 4000, Mode::Ordered), 1);
        QCOMPARE(PlaybackPolicy::previousIndex(0, 3, 0, Mode::Ordered), 0);
        QCOMPARE(PlaybackPolicy::previousIndex(0, 3, 0, Mode::RepeatAll), 2);
        QCOMPARE(PlaybackPolicy::previousIndex(1, 3, 0, Mode::RepeatOne), 1);
        QCOMPARE(PlaybackPolicy::nextIndex(-1, 0, Mode::Ordered), -1);
    }
};

QTEST_APPLESS_MAIN(PlaybackPolicyTest)
#include "playbackpolicy_test.moc"
