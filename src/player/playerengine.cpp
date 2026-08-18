#include "playerengine.h"
#include "playbackpolicy.h"
#include "network/networkpolicy.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QDir>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QMutex>
#include <QTimer>
#include <QUrl>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswscale/swscale.h>
}

namespace {
QString ffmpegError(int code)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

int interruptCallback(void *opaque)
{
    return static_cast<std::atomic_bool *>(opaque)->load() ? 1 : 0;
}

AVCodecContext *openDecoder(AVFormatContext *format, int streamIndex, QString &error)
{
    if (streamIndex < 0) return nullptr;
    AVStream *stream = format->streams[streamIndex];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) { error = QStringLiteral("找不到媒体所需的解码器"); return nullptr; }
    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context) { error = QStringLiteral("无法分配解码器上下文"); return nullptr; }
    int result = avcodec_parameters_to_context(context, stream->codecpar);
    if (result >= 0) result = avcodec_open2(context, codec, nullptr);
    if (result < 0) { error = ffmpegError(result); avcodec_free_context(&context); }
    return context;
}

qint64 frameTimeMs(const AVFrame *frame, const AVStream *stream)
{
    const int64_t timestamp = frame->best_effort_timestamp;
    if (timestamp == AV_NOPTS_VALUE) return -1;
    return qRound64(timestamp * av_q2d(stream->time_base) * 1000.0);
}

struct AudioFilter
{
    AVFilterGraph *graph = nullptr;
    AVFilterContext *source = nullptr;
    AVFilterContext *sink = nullptr;
    double speed = 1.0;

    ~AudioFilter() { avfilter_graph_free(&graph); }

    bool rebuild(const AVCodecContext *codec, double requestedSpeed, QString &error)
    {
        avfilter_graph_free(&graph); source = sink = nullptr; speed = requestedSpeed;
        graph = avfilter_graph_alloc();
        if (!graph) { error = QStringLiteral("无法创建音频滤镜"); return false; }

        char layout[128]{};
        av_channel_layout_describe(&codec->ch_layout, layout, sizeof(layout));
        const char *sampleFormat = av_get_sample_fmt_name(codec->sample_fmt);
        const QByteArray sourceArgs = QStringLiteral("time_base=1/%1:sample_rate=%1:sample_fmt=%2:channel_layout=%3")
            .arg(codec->sample_rate).arg(QString::fromLatin1(sampleFormat ? sampleFormat : "s16"), QString::fromLatin1(layout)).toUtf8();

        AVFilterContext *tempo = nullptr;
        AVFilterContext *format = nullptr;
        int result = avfilter_graph_create_filter(&source, avfilter_get_by_name("abuffer"), "audio_source", sourceArgs.constData(), nullptr, graph);
        const QByteArray tempoArgs = QByteArray::number(speed, 'f', 3);
        if (result >= 0) result = avfilter_graph_create_filter(&tempo, avfilter_get_by_name("atempo"), "tempo", tempoArgs.constData(), nullptr, graph);
        if (result >= 0) result = avfilter_graph_create_filter(&format, avfilter_get_by_name("aformat"), "format",
            "sample_fmts=s16:sample_rates=48000:channel_layouts=stereo", nullptr, graph);
        if (result >= 0) result = avfilter_graph_create_filter(&sink, avfilter_get_by_name("abuffersink"), "audio_sink", nullptr, nullptr, graph);
        if (result >= 0) result = avfilter_link(source, 0, tempo, 0);
        if (result >= 0) result = avfilter_link(tempo, 0, format, 0);
        if (result >= 0) result = avfilter_link(format, 0, sink, 0);
        if (result >= 0) result = avfilter_graph_config(graph, nullptr);
        if (result < 0) { error = ffmpegError(result); avfilter_graph_free(&graph); source = sink = nullptr; return false; }
        return true;
    }
};
}

DecoderWorker::DecoderWorker(QObject *parent) : QThread(parent) {}
DecoderWorker::~DecoderWorker() { requestStop(); wait(); }

void DecoderWorker::open(const QString &source)
{
    if (isRunning()) { requestStop(); wait(); }
    m_source = source; m_stop = false; m_paused = false; m_seekMs = -1; start();
}

void DecoderWorker::requestStop() { m_stop = true; m_paused = false; }
void DecoderWorker::requestPause(bool pause) { m_paused = pause; }
void DecoderWorker::requestSeek(qint64 milliseconds) { m_seekMs = milliseconds; }
void DecoderWorker::setSpeed(double speed) { m_speed = qBound(0.5, speed, 2.0); }

void DecoderWorker::run()
{
    avformat_network_init();
    const bool network = m_source.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                      || m_source.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    const bool https = m_source.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    const QUrl parsedSource(m_source, QUrl::StrictMode);
    if (!network && !parsedSource.scheme().isEmpty() && !QDir::isAbsolutePath(m_source)) {
        emit playbackError(QStringLiteral("不支持的媒体协议"), QStringLiteral("仅允许本地文件、HTTP 和 HTTPS 媒体"));
        avformat_network_deinit();
        return;
    }
    const QByteArray source = m_source.toUtf8();
    AVFormatContext *format = nullptr;
    int result = AVERROR_UNKNOWN;
    const int maximumAttempts = network ? 4 : 1;
    for (int attempt = 0; attempt < maximumAttempts && !m_stop; ++attempt) {
        format = avformat_alloc_context();
        if (!format) { emit playbackError(QStringLiteral("无法创建媒体读取器"), {}); avformat_network_deinit(); return; }
        format->interrupt_callback = {interruptCallback, &m_stop};
        AVDictionary *options = nullptr;
        if (network) {
            NetworkPolicy::applyFfmpegInputOptions(&options, https);
            emit bufferingChanged(true, attempt == 0 ? 0 : -1);
        }
        result = avformat_open_input(&format, source.constData(), nullptr, &options);
        av_dict_free(&options);
        if (result >= 0) break;
        avformat_free_context(format); format = nullptr;
        if (network && attempt + 1 < maximumAttempts && !m_stop) {
            const int retry = attempt + 1;
            const int delay = PlaybackPolicy::retryDelayMs(retry);
            emit retrying(retry, 3, delay);
            for (int waited = 0; waited < delay && !m_stop; waited += 50) QThread::msleep(50);
        }
    }
    if (result < 0) {
        const QString details = ffmpegError(result); if (format) avformat_free_context(format);
        if (!m_stop) emit playbackError(network ? QStringLiteral("无法连接到网络媒体") : QStringLiteral("无法打开媒体文件"), details);
        avformat_network_deinit();
        return;
    }
    if (network && format->pb) {
        av_opt_set_int(format->pb, "reconnect", 1, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(format->pb, "reconnect_streamed", 0, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(format->pb, "reconnect_on_network_error", 1, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(format->pb, "reconnect_max_retries", 3, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(format->pb, "reconnect_delay_total_max", 7, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(format->pb, "reconnect_delay_max", 4, AV_OPT_SEARCH_CHILDREN);
    }
    result = avformat_find_stream_info(format, nullptr);
    if (result < 0) {
        const QString details = ffmpegError(result); avformat_close_input(&format);
        emit playbackError(QStringLiteral("无法读取媒体信息"), details); return;
    }

    const int audioIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    const int videoIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    QString decoderError;
    AVCodecContext *audioCodec = openDecoder(format, audioIndex, decoderError);
    AVCodecContext *videoCodec = openDecoder(format, videoIndex, decoderError);
    if (!audioCodec && !videoCodec) {
        avformat_close_input(&format); emit playbackError(QStringLiteral("媒体中没有可播放的音视频流"), decoderError); return;
    }

    const qint64 durationMs = format->duration == AV_NOPTS_VALUE ? 0 : format->duration / (AV_TIME_BASE / 1000);
    const bool seekable = PlaybackPolicy::isSeekable(durationMs, !format->pb || (format->pb->seekable & AVIO_SEEKABLE_NORMAL));
    QStringList description;
    if (videoCodec) description << QStringLiteral("%1×%2 · %3").arg(videoCodec->width).arg(videoCodec->height).arg(QString::fromUtf8(videoCodec->codec->name).toUpper());
    if (audioCodec) description << QString::fromUtf8(audioCodec->codec->name).toUpper();
    emit mediaOpened(durationMs, audioCodec != nullptr, videoCodec != nullptr, description.join(QStringLiteral(" · ")));
    emit seekabilityChanged(seekable);
    if (network) emit bufferingChanged(false, 100);

    constexpr int outputRate = 48000;
    constexpr int outputChannels = 2;
    AudioFilter audioFilter;
    if (audioCodec) {
        if (!audioFilter.rebuild(audioCodec, m_speed.load(), decoderError)) { avcodec_free_context(&audioCodec); audioCodec = nullptr; }
        else emit audioFormatReady(outputRate, outputChannels);
    }

    SwsContext *sws = nullptr;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *filteredFrame = av_frame_alloc();
    QElapsedTimer videoClock; videoClock.start();
    qint64 videoBasePts = -1;
    qint64 lastPosition = -1;
    QElapsedTimer playbackClock;
    qint64 playbackClockBaseMs = -1;
    qint64 audioQueuedUntilMs = -1;
    qint64 audioDiscardUntilMs = -1;
    qint64 videoDiscardUntilMs = -1;
    bool emittedVideoFrame = false;
    bool recoveringFromCorruptPacket = false;

    auto handleFrame = [&](AVCodecContext *codec, AVStream *stream, bool audio) {
        while (!m_stop) {
            const int receive = avcodec_receive_frame(codec, frame);
            if (receive == AVERROR(EAGAIN) || receive == AVERROR_EOF) break;
            if (receive < 0) break;
            const qint64 ptsMs = frameTimeMs(frame, stream);
            qint64 &discardUntil = audio ? audioDiscardUntilMs : videoDiscardUntilMs;
            if (discardUntil >= 0 && ptsMs >= 0) {
                if (ptsMs < discardUntil) { av_frame_unref(frame); continue; }
                discardUntil = -1;
            }
            if (audio && audioFilter.graph) {
                const double requestedSpeed = m_speed.load();
                if (!qFuzzyCompare(audioFilter.speed, requestedSpeed)) audioFilter.rebuild(codec, requestedSpeed, decoderError);
                if (audioFilter.source && av_buffersrc_add_frame_flags(audioFilter.source, frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
                    while (av_buffersink_get_frame(audioFilter.sink, filteredFrame) >= 0) {
                        const int bytes = filteredFrame->nb_samples * outputChannels * 2;
                        emit audioDataReady(QByteArray(reinterpret_cast<const char *>(filteredFrame->data[0]), bytes));
                        const double speed = m_speed.load();
                        const qint64 mediaDurationMs = qMax<qint64>(1,
                            qRound64(1000.0 * filteredFrame->nb_samples * speed / outputRate));
                        if (playbackClockBaseMs < 0) {
                            playbackClockBaseMs = ptsMs >= 0 ? ptsMs : 0;
                            audioQueuedUntilMs = playbackClockBaseMs;
                            playbackClock.start();
                        }
                        audioQueuedUntilMs = qMax(audioQueuedUntilMs, ptsMs >= 0 ? ptsMs : audioQueuedUntilMs)
                                           + mediaDurationMs;
                        while (!m_stop && !m_paused) {
                            const qint64 master = playbackClockBaseMs
                                + qRound64(playbackClock.elapsed() * speed);
                            if (audioQueuedUntilMs - master <= 250) break;
                            QThread::msleep(5);
                        }
                        av_frame_unref(filteredFrame);
                    }
                }
            } else if (!audio) {
                sws = sws_getCachedContext(sws, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                                           frame->width, frame->height, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (sws) {
                    QImage image(frame->width, frame->height, QImage::Format_ARGB32);
                    uint8_t *dst[] = {image.bits()}; int lines[] = {int(image.bytesPerLine())};
                    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, lines);
                    if (emittedVideoFrame && audioCodec && ptsMs >= 0 && playbackClockBaseMs >= 0) {
                        const qint64 masterClockMs = playbackClockBaseMs
                            + qRound64(playbackClock.elapsed() * m_speed.load());
                        const auto decision = PlaybackPolicy::videoDecision(ptsMs, masterClockMs);
                        if (decision.action == PlaybackPolicy::VideoAction::Drop) { av_frame_unref(frame); continue; }
                        if (decision.action == PlaybackPolicy::VideoAction::Delay) QThread::msleep(unsigned(decision.delayMs));
                    } else if (!audioCodec && ptsMs >= 0) {
                        if (videoBasePts < 0) { videoBasePts = ptsMs; videoClock.restart(); }
                        const qint64 target = qRound64((ptsMs - videoBasePts) / m_speed.load());
                        const qint64 waitMs = target - videoClock.elapsed();
                        if (waitMs > 0) QThread::msleep(qMin<qint64>(waitMs, 100));
                    }
                    emit videoFrameReady(image);
                    emittedVideoFrame = true;
                }
            }
            if (ptsMs >= 0 && qAbs(ptsMs - lastPosition) >= 50) { lastPosition = ptsMs; emit positionChanged(ptsMs); }
            av_frame_unref(frame);
        }
    };

    while (!m_stop) {
        if (m_paused && !m_stop) {
            const qint64 pausedClockMs = playbackClockBaseMs >= 0
                ? playbackClockBaseMs + qRound64(playbackClock.elapsed() * m_speed.load()) : -1;
            while (m_paused && !m_stop) QThread::msleep(20);
            if (pausedClockMs >= 0) { playbackClockBaseMs = pausedClockMs; playbackClock.restart(); }
        }
        const qint64 seekMs = m_seekMs.exchange(-1);
        if (seekMs >= 0) {
            const int64_t target = av_rescale(seekMs, AV_TIME_BASE, 1000);
            if (avformat_seek_file(format, -1, INT64_MIN, target, INT64_MAX, AVSEEK_FLAG_BACKWARD) >= 0) {
                if (audioCodec) avcodec_flush_buffers(audioCodec);
                if (videoCodec) avcodec_flush_buffers(videoCodec);
                if (audioCodec) audioFilter.rebuild(audioCodec, m_speed.load(), decoderError);
                playbackClockBaseMs = -1;
                audioQueuedUntilMs = -1;
                audioDiscardUntilMs = audioCodec ? seekMs : -1;
                videoDiscardUntilMs = videoCodec ? seekMs : -1;
                emittedVideoFrame = false;
                videoBasePts = -1; videoClock.restart(); emit positionChanged(seekMs);
            }
        }
        result = av_read_frame(format, packet);
        if (result < 0) break;
        if (packet->flags & AV_PKT_FLAG_CORRUPT) {
            if (!recoveringFromCorruptPacket) emit bufferingChanged(true, -1);
            recoveringFromCorruptPacket = true;
            av_packet_unref(packet);
            continue;
        }
        if (recoveringFromCorruptPacket) {
            recoveringFromCorruptPacket = false;
            emit bufferingChanged(false, 100);
        }
        if (packet->stream_index == audioIndex && audioCodec) {
            if (avcodec_send_packet(audioCodec, packet) >= 0) handleFrame(audioCodec, format->streams[audioIndex], true);
        } else if (packet->stream_index == videoIndex && videoCodec) {
            if (avcodec_send_packet(videoCodec, packet) >= 0) handleFrame(videoCodec, format->streams[videoIndex], false);
        }
        av_packet_unref(packet);
    }

    const bool transportReachedEof = format->pb && format->pb->eof_reached;
    const bool readFailed = result < 0 && result != AVERROR_EOF && !transportReachedEof && !m_stop;
    const QString readError = readFailed ? ffmpegError(result) : QString();
    av_packet_free(&packet); av_frame_free(&frame); av_frame_free(&filteredFrame); sws_freeContext(sws);
    avcodec_free_context(&audioCodec); avcodec_free_context(&videoCodec); avformat_close_input(&format);
    avformat_network_deinit();
    if (readFailed) emit playbackError(network ? QStringLiteral("网络媒体读取中断") : QStringLiteral("媒体读取失败"), readError);
    else if (!m_stop) emit playbackFinished();
}

PlayerEngine::PlayerEngine(QObject *parent) : QObject(parent)
{
    m_worker = new DecoderWorker(this);
    m_audioWriteTimer = new QTimer(this);
    m_audioWriteTimer->setInterval(5);
    connect(m_audioWriteTimer, &QTimer::timeout, this, &PlayerEngine::drainAudio);
    connect(m_worker, &DecoderWorker::mediaOpened, this, [this](qint64 duration, bool audio, bool video, const QString &description) {
        m_durationMs = duration; m_hasAudio = audio; m_hasVideo = video; emit mediaOpened(duration, audio, video, description); setState(State::Playing);
    });
    connect(m_worker, &DecoderWorker::audioFormatReady, this, &PlayerEngine::configureAudio);
    connect(m_worker, &DecoderWorker::audioDataReady, this, [this](const QByteArray &pcm) {
        if (!m_audioDevice) return;
        m_audioPending.append(pcm);
        drainAudio();
    });
    connect(m_worker, &DecoderWorker::videoFrameReady, this, [this](const QImage &image) {
        if (m_state != State::Paused) emit videoFrameReady(image);
    });
    connect(m_worker, &DecoderWorker::positionChanged, this, [this](qint64 position) {
        if (m_state == State::Paused) return;
        m_positionMs = position;
        emit positionChanged(position);
    });
    connect(m_worker, &DecoderWorker::bufferingChanged, this, [this](bool buffering, int percent) {
        if (buffering) setState(State::Buffering);
        else if (m_state == State::Buffering && (m_hasAudio || m_hasVideo)) setState(State::Playing);
        emit bufferingChanged(buffering, percent);
    });
    connect(m_worker, &DecoderWorker::seekabilityChanged, this, [this](bool seekable) { m_seekable = seekable; emit seekabilityChanged(seekable); });
    connect(m_worker, &DecoderWorker::retrying, this, &PlayerEngine::retrying);
    connect(m_worker, &DecoderWorker::playbackFinished, this, [this] {
        if (m_audioSink && (!m_audioPending.isEmpty() || m_audioSink->bytesFree() < m_audioSink->bufferSize())) {
            m_finishPending = true;
            return;
        }
        completePlayback();
    });
    connect(m_worker, &DecoderWorker::playbackError, this, [this](const QString &message, const QString &details) { resetAudio(); setState(State::Error); emit errorOccurred(message, details); });
}

PlayerEngine::~PlayerEngine() { stop(); resetAudio(true); }

void PlayerEngine::open(const QString &source)
{
    if (m_state != State::Ended) stop();
    else { m_finishPending = false; m_audioPending.clear(); }
    m_durationMs = m_positionMs = 0; m_hasAudio = m_hasVideo = m_seekable = false; setState(State::Opening); m_worker->open(source);
}
void PlayerEngine::play() { if (m_state == State::Paused) { m_worker->requestPause(false); if (m_audioSink) m_audioSink->resume(); setState(State::Playing); } }
void PlayerEngine::pause() { if (m_state == State::Playing) { m_worker->requestPause(true); if (m_audioSink) m_audioSink->suspend(); setState(State::Paused); } }
void PlayerEngine::stop() { if (m_worker && m_worker->isRunning()) { m_worker->requestStop(); m_worker->wait(3000); } resetAudio(false); if (m_state != State::Idle) setState(State::Stopped); }
void PlayerEngine::seek(qint64 milliseconds)
{
    if (!m_seekable || !m_worker->isRunning()) return;
    if (m_audioSink) { m_audioSink->reset(); m_audioDevice = m_audioSink->start(); }
    m_worker->requestSeek(qBound<qint64>(0, milliseconds, m_durationMs));
}
void PlayerEngine::setVolume(float volume) { m_volume = qBound(0.0f, volume, 1.0f); if (m_audioSink) m_audioSink->setVolume(m_muted ? 0.0f : m_volume); }
void PlayerEngine::setMuted(bool muted) { m_muted = muted; if (m_audioSink) m_audioSink->setVolume(muted ? 0.0f : m_volume); }
void PlayerEngine::setSpeed(double speed) { m_worker->setSpeed(speed); }
void PlayerEngine::setState(State state) { if (m_state != state) { m_state = state; emit stateChanged(state); } }

void PlayerEngine::resetAudio(bool releaseSink)
{
    m_finishPending = false;
    if (m_audioWriteTimer) m_audioWriteTimer->stop();
    m_audioPending.clear();
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioDevice = nullptr;
        if (releaseSink) { delete m_audioSink; m_audioSink = nullptr; m_audioDeviceId.clear(); }
    }
}

void PlayerEngine::configureAudio(int sampleRate, int channels)
{
    QAudioFormat format; format.setSampleRate(sampleRate); format.setChannelCount(channels); format.setSampleFormat(QAudioFormat::Int16);
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) { reportAudioError(QStringLiteral("未检测到可用的音频输出设备")); return; }
    if (!device.isFormatSupported(format)) { reportAudioError(QStringLiteral("默认音频设备不支持 48 kHz 双声道 S16 输出")); return; }
    if (m_audioSink && m_audioSink->format() == format && m_audioDeviceId == device.id()) {
        m_audioSink->setVolume(m_muted ? 0.0f : m_volume);
        if (!m_audioDevice) m_audioDevice = m_audioSink->start();
        if (!m_audioDevice) { reportAudioError(QStringLiteral("无法继续使用默认音频输出设备")); return; }
        m_audioWriteTimer->start();
        emit audioOutputReady(device.description());
        return;
    }
    resetAudio(true);
    m_audioSink = new QAudioSink(device, format, this); m_audioSink->setBufferSize(sampleRate * channels * 2 / 2);
    m_audioDeviceId = device.id();
    connect(m_audioSink, &QAudioSink::stateChanged, this, [this](QtAudio::State state) {
        if (state == QtAudio::IdleState && m_finishPending && m_audioPending.isEmpty()) {
            QTimer::singleShot(0, this, [this] {
                if (m_finishPending && m_audioPending.isEmpty()) completePlayback();
            });
            return;
        }
        if (state == QtAudio::StoppedState && m_audioSink && m_audioSink->error() != QtAudio::NoError) {
            const int errorCode = int(m_audioSink->error());
            QTimer::singleShot(0, this, [this, errorCode] {
                if (m_audioSink && m_audioSink->error() != QtAudio::NoError)
                    reportAudioError(QStringLiteral("音频设备输出失败，错误码 %1").arg(errorCode));
            });
        }
    });
    m_audioSink->setVolume(m_muted ? 0.0f : m_volume); m_audioDevice = m_audioSink->start();
    if (!m_audioDevice) { reportAudioError(QStringLiteral("无法启动默认音频输出设备")); return; }
    m_audioWriteTimer->start();
    emit audioOutputReady(device.description());
}

void PlayerEngine::drainAudio()
{
    if (!m_audioSink || !m_audioDevice || m_state == State::Paused) return;
    while (!m_audioPending.isEmpty()) {
        const qint64 available = m_audioSink->bytesFree();
        if (available <= 0) break;
        const qint64 requested = qMin<qint64>(available, m_audioPending.size());
        const qint64 written = m_audioDevice->write(m_audioPending.constData(), requested);
        if (written <= 0) break;
        m_audioPending.remove(0, qsizetype(written));
    }
    if (m_finishPending && m_audioPending.isEmpty()
        && m_audioSink->bytesFree() >= m_audioSink->bufferSize())
        completePlayback();
}

void PlayerEngine::completePlayback()
{
    m_finishPending = false;
    m_audioPending.clear();
    if (m_audioWriteTimer) m_audioWriteTimer->stop();
    setState(State::Ended);
    emit finished();
}

void PlayerEngine::reportAudioError(const QString &message)
{
    resetAudio(true); setState(State::Error); emit errorOccurred(message, QStringLiteral("请检查 Windows 默认输出设备、音量和独占模式设置。"));
}
