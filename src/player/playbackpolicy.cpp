#include "playbackpolicy.h"

#include <QtGlobal>

namespace PlaybackPolicy {

int retryDelayMs(int attempt)
{
    if (attempt <= 0) return 0;
    return 1000 << qMin(attempt - 1, 2);
}

int bufferPercent(qint64 queuedMs, qint64 targetMs)
{
    if (targetMs <= 0) return -1;
    return int(qBound<qint64>(qint64(0), queuedMs * 100 / targetMs, qint64(100)));
}

bool isSeekable(qint64 durationMs, bool ioSeekable)
{
    return durationMs > 0 && ioSeekable;
}

VideoDecision videoDecision(qint64 videoPtsMs, qint64 masterClockMs)
{
    const qint64 delta = videoPtsMs - masterClockMs;
    if (delta < -80) return {VideoAction::Drop, 0};
    if (delta > 10) return {VideoAction::Delay, qMin<qint64>(delta, 100)};
    return {VideoAction::Display, 0};
}

int nextIndex(int current, int count, PlaylistMode mode)
{
    if (count <= 0 || current < 0 || current >= count) return -1;
    if (mode == PlaylistMode::RepeatOne) return current;
    if (current + 1 < count) return current + 1;
    return mode == PlaylistMode::RepeatAll ? 0 : -1;
}

int previousIndex(int current, int count, qint64 positionMs, PlaylistMode mode)
{
    if (count <= 0 || current < 0 || current >= count) return -1;
    if (positionMs > 3000 || mode == PlaylistMode::RepeatOne) return current;
    if (current > 0) return current - 1;
    return mode == PlaylistMode::RepeatAll ? count - 1 : 0;
}

}
