#pragma once

#include <QtGlobal>

namespace PlaybackPolicy {

enum class VideoAction { Display, Delay, Drop };
enum class PlaylistMode { Ordered, RepeatAll, RepeatOne };

struct VideoDecision {
    VideoAction action = VideoAction::Display;
    qint64 delayMs = 0;
};

int retryDelayMs(int attempt);
int bufferPercent(qint64 queuedMs, qint64 targetMs);
bool isSeekable(qint64 durationMs, bool ioSeekable);
VideoDecision videoDecision(qint64 videoPtsMs, qint64 masterClockMs);
int nextIndex(int current, int count, PlaylistMode mode);
int previousIndex(int current, int count, qint64 positionMs, PlaylistMode mode);

}
