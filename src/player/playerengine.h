#pragma once

#include <QAudioFormat>
#include <QImage>
#include <QObject>
#include <QThread>
#include <atomic>

class QAudioSink;
class QIODevice;

class DecoderWorker final : public QThread
{
    Q_OBJECT
public:
    explicit DecoderWorker(QObject *parent = nullptr);
    ~DecoderWorker() override;

    void open(const QString &source);
    void requestStop();
    void requestPause(bool pause);
    void requestSeek(qint64 milliseconds);
    void setSpeed(double speed);

signals:
    void mediaOpened(qint64 durationMs, bool hasAudio, bool hasVideo, const QString &description);
    void audioFormatReady(int sampleRate, int channels);
    void audioDataReady(const QByteArray &pcm);
    void videoFrameReady(const QImage &image);
    void positionChanged(qint64 milliseconds);
    void bufferingChanged(bool buffering, int percent);
    void seekabilityChanged(bool seekable);
    void retrying(int attempt, int maximum, int delayMs);
    void playbackFinished();
    void playbackError(const QString &message, const QString &details);

protected:
    void run() override;

private:
    QString m_source;
    std::atomic_bool m_stop{false};
    std::atomic_bool m_paused{false};
    std::atomic<qint64> m_seekMs{-1};
    std::atomic<double> m_speed{1.0};
};

class PlayerEngine final : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Opening, Buffering, Playing, Paused, Stopped, Ended, Error };
    Q_ENUM(State)

    explicit PlayerEngine(QObject *parent = nullptr);
    ~PlayerEngine() override;

    void open(const QString &source);
    void play();
    void pause();
    void stop();
    void seek(qint64 milliseconds);
    void setVolume(float volume);
    void setMuted(bool muted);
    void setSpeed(double speed);

    State state() const { return m_state; }
    qint64 duration() const { return m_durationMs; }
    qint64 position() const { return m_positionMs; }
    bool hasVideo() const { return m_hasVideo; }
    bool hasAudio() const { return m_hasAudio; }
    bool isSeekable() const { return m_seekable; }

signals:
    void stateChanged(PlayerEngine::State state);
    void mediaOpened(qint64 durationMs, bool hasAudio, bool hasVideo, const QString &description);
    void positionChanged(qint64 milliseconds);
    void videoFrameReady(const QImage &image);
    void bufferingChanged(bool buffering, int percent);
    void seekabilityChanged(bool seekable);
    void retrying(int attempt, int maximum, int delayMs);
    void audioOutputReady(const QString &deviceName);
    void errorOccurred(const QString &message, const QString &details);
    void finished();

private:
    void setState(State state);
    void resetAudio();
    void configureAudio(int sampleRate, int channels);
    void reportAudioError(const QString &message);

    DecoderWorker *m_worker = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioDevice = nullptr;
    State m_state = State::Idle;
    qint64 m_durationMs = 0;
    qint64 m_positionMs = 0;
    bool m_hasAudio = false;
    bool m_hasVideo = false;
    bool m_seekable = false;
    bool m_muted = false;
    float m_volume = 0.7f;
};
