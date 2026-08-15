#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QTimer>
#include <cstdio>
#include <cmath>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        std::fprintf(stderr, "No default audio output device.\n");
        return 2;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    std::fprintf(stdout, "Default output: %s\n", device.description().toUtf8().constData());
    std::fprintf(stdout, "Requested format: 48000 Hz, stereo, S16\n");
    if (!device.isFormatSupported(format)) {
        std::fprintf(stderr, "The default endpoint does not support the StreamBox output format.\n");
        return 3;
    }

    constexpr int sampleRate = 48000;
    constexpr int durationSeconds = 3;
    QByteArray pcm(sampleRate * durationSeconds * 2 * 2, Qt::Uninitialized);
    auto *samples = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < sampleRate * durationSeconds; ++i) {
        const bool audible = (i / (sampleRate / 4)) % 2 == 0;
        const qint16 value = audible
            ? qint16(std::sin(2.0 * 3.141592653589793 * 1000.0 * i / sampleRate) * 10000) : 0;
        samples[i * 2] = value;
        samples[i * 2 + 1] = value;
    }

    QBuffer source(&pcm);
    source.open(QIODevice::ReadOnly);
    QAudioSink sink(device, format);
    sink.setVolume(1.0);
    bool ioError = false;
    QObject::connect(&sink, &QAudioSink::stateChanged, &app, [&](QtAudio::State state) {
        if (state == QtAudio::StoppedState && sink.error() != QtAudio::NoError) {
            ioError = true; app.quit();
        } else if (state == QtAudio::IdleState && source.atEnd()) {
            app.quit();
        }
    });
    QTimer::singleShot(5000, &app, &QCoreApplication::quit);
    sink.start(&source);
    const int eventResult = app.exec();
    const qint64 processed = sink.processedUSecs();
    const auto error = sink.error();
    sink.stop();
    std::fprintf(stdout, "Processed audio: %.2f seconds; sink error: %d\n",
        processed / 1000000.0, int(error));
    if (eventResult != 0 || ioError || error != QtAudio::NoError || processed < 2500000) return 4;
    return 0;
}
