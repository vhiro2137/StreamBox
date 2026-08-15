#include "playerengine.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QMutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
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
    AVFormatContext *format = avformat_alloc_context();
    if (!format) { emit playbackError(QStringLiteral("无法创建媒体读取器"), {}); return; }
    format->interrupt_callback = {interruptCallback, &m_stop};

    AVDictionary *options = nullptr;
    const bool network = m_source.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                      || m_source.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    if (network) {
        av_dict_set(&options, "timeout", "10000000", 0);
        av_dict_set(&options, "rw_timeout", "10000000", 0);
        av_dict_set(&options, "reconnect", "1", 0);
        av_dict_set(&options, "reconnect_streamed", "1", 0);
        av_dict_set(&options, "reconnect_delay_max", "4", 0);
        emit bufferingChanged(true, -1);
    }
    const QByteArray source = m_source.toUtf8();
    int result = avformat_open_input(&format, source.constData(), nullptr, &options);
    av_dict_free(&options);
    if (result < 0) {
        const QString details = ffmpegError(result); avformat_free_context(format);
        if (!m_stop) emit playbackError(network ? QStringLiteral("无法连接到网络媒体") : QStringLiteral("无法打开媒体文件"), details);
        return;
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
    QStringList description;
    if (videoCodec) description << QStringLiteral("%1×%2 · %3").arg(videoCodec->width).arg(videoCodec->height).arg(QString::fromUtf8(videoCodec->codec->name).toUpper());
    if (audioCodec) description << QString::fromUtf8(audioCodec->codec->name).toUpper();
    emit mediaOpened(durationMs, audioCodec != nullptr, videoCodec != nullptr, description.join(QStringLiteral(" · ")));
    if (network) emit bufferingChanged(false, 100);

    constexpr int outputRate = 48000;
    constexpr AVSampleFormat outputFormat = AV_SAMPLE_FMT_S16;
    AVChannelLayout outputLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext *swr = nullptr;
    if (audioCodec) {
        result = swr_alloc_set_opts2(&swr, &outputLayout, outputFormat, outputRate,
                                     &audioCodec->ch_layout, audioCodec->sample_fmt, audioCodec->sample_rate, 0, nullptr);
        if (result >= 0) result = swr_init(swr);
        if (result < 0) { swr_free(&swr); avcodec_free_context(&audioCodec); audioCodec = nullptr; }
        else emit audioFormatReady(outputRate, outputLayout.nb_channels);
    }

    SwsContext *sws = nullptr;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    QElapsedTimer videoClock; videoClock.start();
    qint64 videoBasePts = -1;
    qint64 lastPosition = -1;

    auto handleFrame = [&](AVCodecContext *codec, AVStream *stream, bool audio) {
        while (!m_stop) {
            const int receive = avcodec_receive_frame(codec, frame);
            if (receive == AVERROR(EAGAIN) || receive == AVERROR_EOF) break;
            if (receive < 0) break;
            const qint64 ptsMs = frameTimeMs(frame, stream);
            if (audio && swr) {
                const int maxSamples = av_rescale_rnd(swr_get_delay(swr, codec->sample_rate) + frame->nb_samples,
                                                      outputRate, codec->sample_rate, AV_ROUND_UP);
                QByteArray pcm(maxSamples * outputLayout.nb_channels * av_get_bytes_per_sample(outputFormat), Qt::Uninitialized);
                uint8_t *output[] = {reinterpret_cast<uint8_t *>(pcm.data())};
                const int converted = swr_convert(swr, output, maxSamples,
                                                  const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples);
                if (converted > 0) {
                    pcm.resize(converted * outputLayout.nb_channels * av_get_bytes_per_sample(outputFormat));
                    emit audioDataReady(pcm);
                    const int sleepMs = qRound(1000.0 * converted / outputRate / m_speed.load());
                    if (sleepMs > 0) QThread::msleep(qMin(sleepMs, 100));
                }
            } else if (!audio) {
                sws = sws_getCachedContext(sws, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                                           frame->width, frame->height, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (sws) {
                    QImage image(frame->width, frame->height, QImage::Format_ARGB32);
                    uint8_t *dst[] = {image.bits()}; int lines[] = {int(image.bytesPerLine())};
                    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, lines);
                    if (!audioCodec && ptsMs >= 0) {
                        if (videoBasePts < 0) { videoBasePts = ptsMs; videoClock.restart(); }
                        const qint64 target = qRound64((ptsMs - videoBasePts) / m_speed.load());
                        const qint64 waitMs = target - videoClock.elapsed();
                        if (waitMs > 0) QThread::msleep(qMin<qint64>(waitMs, 100));
                    }
                    emit videoFrameReady(image);
                }
            }
            if (ptsMs >= 0 && qAbs(ptsMs - lastPosition) >= 50) { lastPosition = ptsMs; emit positionChanged(ptsMs); }
            av_frame_unref(frame);
        }
    };

    while (!m_stop) {
        while (m_paused && !m_stop) QThread::msleep(20);
        const qint64 seekMs = m_seekMs.exchange(-1);
        if (seekMs >= 0) {
            const int64_t target = av_rescale(seekMs, AV_TIME_BASE, 1000);
            if (avformat_seek_file(format, -1, INT64_MIN, target, INT64_MAX, AVSEEK_FLAG_BACKWARD) >= 0) {
                if (audioCodec) avcodec_flush_buffers(audioCodec);
                if (videoCodec) avcodec_flush_buffers(videoCodec);
                if (swr) { swr_close(swr); swr_init(swr); }
                videoBasePts = -1; videoClock.restart(); emit positionChanged(seekMs);
            }
        }
        result = av_read_frame(format, packet);
        if (result < 0) break;
        if (packet->stream_index == audioIndex && audioCodec) {
            if (avcodec_send_packet(audioCodec, packet) >= 0) handleFrame(audioCodec, format->streams[audioIndex], true);
        } else if (packet->stream_index == videoIndex && videoCodec) {
            if (avcodec_send_packet(videoCodec, packet) >= 0) handleFrame(videoCodec, format->streams[videoIndex], false);
        }
        av_packet_unref(packet);
    }

    av_packet_free(&packet); av_frame_free(&frame); swr_free(&swr); sws_freeContext(sws);
    avcodec_free_context(&audioCodec); avcodec_free_context(&videoCodec); avformat_close_input(&format);
    av_channel_layout_uninit(&outputLayout); avformat_network_deinit();
    if (!m_stop) emit playbackFinished();
}

PlayerEngine::PlayerEngine(QObject *parent) : QObject(parent)
{
    m_worker = new DecoderWorker(this);
    connect(m_worker, &DecoderWorker::mediaOpened, this, [this](qint64 duration, bool audio, bool video, const QString &description) {
        m_durationMs = duration; m_hasAudio = audio; m_hasVideo = video; emit mediaOpened(duration, audio, video, description); setState(State::Playing);
    });
    connect(m_worker, &DecoderWorker::audioFormatReady, this, &PlayerEngine::configureAudio);
    connect(m_worker, &DecoderWorker::audioDataReady, this, [this](const QByteArray &pcm) {
        if (m_audioDevice && m_state != State::Paused) m_audioDevice->write(pcm);
    });
    connect(m_worker, &DecoderWorker::videoFrameReady, this, &PlayerEngine::videoFrameReady);
    connect(m_worker, &DecoderWorker::positionChanged, this, [this](qint64 position) { m_positionMs = position; emit positionChanged(position); });
    connect(m_worker, &DecoderWorker::bufferingChanged, this, [this](bool buffering, int percent) { if (buffering) setState(State::Buffering); emit bufferingChanged(buffering, percent); });
    connect(m_worker, &DecoderWorker::playbackFinished, this, [this] { resetAudio(); setState(State::Ended); emit finished(); });
    connect(m_worker, &DecoderWorker::playbackError, this, [this](const QString &message, const QString &details) { resetAudio(); setState(State::Error); emit errorOccurred(message, details); });
}

PlayerEngine::~PlayerEngine() { stop(); }

void PlayerEngine::open(const QString &source)
{
    stop(); m_durationMs = m_positionMs = 0; m_hasAudio = m_hasVideo = false; setState(State::Opening); m_worker->open(source);
}
void PlayerEngine::play() { if (m_state == State::Paused) { m_worker->requestPause(false); if (m_audioSink) m_audioSink->resume(); setState(State::Playing); } }
void PlayerEngine::pause() { if (m_state == State::Playing) { m_worker->requestPause(true); if (m_audioSink) m_audioSink->suspend(); setState(State::Paused); } }
void PlayerEngine::stop() { if (m_worker && m_worker->isRunning()) { m_worker->requestStop(); m_worker->wait(3000); } resetAudio(); if (m_state != State::Idle) setState(State::Stopped); }
void PlayerEngine::seek(qint64 milliseconds) { if (m_worker->isRunning()) { if (m_audioSink) m_audioSink->reset(); m_worker->requestSeek(qBound<qint64>(0, milliseconds, m_durationMs)); } }
void PlayerEngine::setVolume(float volume) { m_volume = qBound(0.0f, volume, 1.0f); if (m_audioSink) m_audioSink->setVolume(m_muted ? 0.0f : m_volume); }
void PlayerEngine::setMuted(bool muted) { m_muted = muted; if (m_audioSink) m_audioSink->setVolume(muted ? 0.0f : m_volume); }
void PlayerEngine::setSpeed(double speed) { m_worker->setSpeed(speed); }
void PlayerEngine::setState(State state) { if (m_state != state) { m_state = state; emit stateChanged(state); } }

void PlayerEngine::resetAudio()
{
    if (m_audioSink) { m_audioSink->stop(); delete m_audioSink; m_audioSink = nullptr; m_audioDevice = nullptr; }
}

void PlayerEngine::configureAudio(int sampleRate, int channels)
{
    resetAudio(); QAudioFormat format; format.setSampleRate(sampleRate); format.setChannelCount(channels); format.setSampleFormat(QAudioFormat::Int16);
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(format)) format = device.preferredFormat();
    m_audioSink = new QAudioSink(device, format, this); m_audioSink->setBufferSize(sampleRate * channels * 2 / 2);
    m_audioSink->setVolume(m_muted ? 0.0f : m_volume); m_audioDevice = m_audioSink->start();
}
