#include "player/playerengine.h"

#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class DecoderWorkerTest final : public QObject
{
    Q_OBJECT

    static QString createWav(QTemporaryDir &directory, int durationMs)
    {
        const int sampleRate = 48000;
        const int channels = 2;
        const int sampleCount = sampleRate * durationMs / 1000;
        QByteArray pcm(sampleCount * channels * 2, Qt::Uninitialized);
        auto *samples = reinterpret_cast<qint16 *>(pcm.data());
        for (int i = 0; i < sampleCount; ++i) {
            const qint16 value = qint16(std::sin(2.0 * M_PI * 440.0 * i / sampleRate) * 8000);
            samples[i * 2] = value;
            samples[i * 2 + 1] = value;
        }

        const QString path = directory.filePath(QStringLiteral("测试 音频.wav"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        file.write("RIFF", 4); out << quint32(36 + pcm.size()); file.write("WAVE", 4);
        file.write("fmt ", 4); out << quint32(16) << quint16(1) << quint16(channels)
            << quint32(sampleRate) << quint32(sampleRate * channels * 2)
            << quint16(channels * 2) << quint16(16);
        file.write("data", 4); out << quint32(pcm.size()); file.write(pcm);
        return path;
    }

private slots:
    void opensDecodesAndAppliesTempo()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = createWav(directory, 1200);
        QVERIFY(!path.isEmpty());

        DecoderWorker worker;
        worker.setSpeed(2.0);
        QSignalSpy opened(&worker, &DecoderWorker::mediaOpened);
        QSignalSpy audio(&worker, &DecoderWorker::audioDataReady);
        QSignalSpy positions(&worker, &DecoderWorker::positionChanged);
        QSignalSpy finished(&worker, &DecoderWorker::playbackFinished);
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        worker.open(path);

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QCOMPARE(errors.count(), 0);
        QCOMPARE(opened.count(), 1);
        QVERIFY(!audio.isEmpty());
        QVERIFY(!positions.isEmpty());
        qint64 decodedBytes = 0;
        for (const auto &entry : audio) {
            const QByteArray pcm = entry.at(0).toByteArray();
            QVERIFY(!pcm.isEmpty());
            QCOMPARE(pcm.size() % 4, 0);
            decodedBytes += pcm.size();
            const auto *samples = reinterpret_cast<const qint16 *>(pcm.constData());
            for (int i = 0; i + 1 < pcm.size() / 2; i += 2)
                QCOMPARE(samples[i], samples[i + 1]);
        }
        QVERIFY(decodedBytes > 100000);
        QVERIFY(decodedBytes < 130000);
        const auto metadata = opened.takeFirst();
        QVERIFY(metadata.at(0).toLongLong() >= 1100);
        QVERIFY(metadata.at(1).toBool());
        QVERIFY(!metadata.at(2).toBool());
    }

    void pauseResumeSeekAndStop()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = createWav(directory, 5000);
        QVERIFY(!path.isEmpty());

        DecoderWorker worker;
        QSignalSpy opened(&worker, &DecoderWorker::mediaOpened);
        QSignalSpy audio(&worker, &DecoderWorker::audioDataReady);
        QSignalSpy positions(&worker, &DecoderWorker::positionChanged);
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        worker.open(path);
        QTRY_COMPARE_WITH_TIMEOUT(opened.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(audio.count() > 1, 3000);

        worker.requestPause(true);
        QTest::qWait(150);
        const int pausedCount = audio.count();
        QTest::qWait(200);
        QVERIFY(audio.count() <= pausedCount + 1);

        worker.requestSeek(3000);
        worker.requestPause(false);
        QTRY_VERIFY_WITH_TIMEOUT([&positions] {
            for (const auto &entry : positions)
                if (entry.at(0).toLongLong() >= 3000) return true;
            return false;
        }(), 4000);

        worker.requestStop();
        QVERIFY(worker.wait(2000));
        QCOMPARE(errors.count(), 0);
    }

    void httpPlayback()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile media(createWav(directory, 700));
        QVERIFY(media.open(QIODevice::ReadOnly));
        const QByteArray wav = media.readAll();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        connect(&server, &QTcpServer::newConnection, &server, [&server, wav] {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                const QByteArray header = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: ")
                    + QByteArray::number(wav.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
                socket->write(header); socket->write(wav); socket->flush();
                socket->waitForBytesWritten(1000); socket->disconnectFromHost();
            }
        });

        DecoderWorker worker;
        QSignalSpy opened(&worker, &DecoderWorker::mediaOpened);
        QSignalSpy buffering(&worker, &DecoderWorker::bufferingChanged);
        QSignalSpy finished(&worker, &DecoderWorker::playbackFinished);
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        worker.open(QStringLiteral("http://127.0.0.1:%1/test.wav").arg(server.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QCOMPARE(errors.count(), 0);
        QCOMPARE(opened.count(), 1);
        QVERIFY(buffering.count() >= 2);
    }

    void retryIsCancelable()
    {
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const quint16 unusedPort = reservation.serverPort();
        reservation.close();

        DecoderWorker worker;
        QSignalSpy retries(&worker, &DecoderWorker::retrying);
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        worker.open(QStringLiteral("http://127.0.0.1:%1/missing.wav").arg(unusedPort));
        QTRY_VERIFY_WITH_TIMEOUT(retries.count() >= 1, 5000);
        QElapsedTimer timer; timer.start();
        worker.requestStop();
        QVERIFY(worker.wait(1000));
        QVERIFY(timer.elapsed() < 1000);
        QCOMPARE(errors.count(), 0);
    }

    void rejectsUnsupportedProtocol()
    {
        DecoderWorker worker;
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        worker.open(QStringLiteral("ftp://example.com/media.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 1000);
        QCOMPARE(errors.first().at(0).toString(), QStringLiteral("不支持的媒体协议"));
        QVERIFY(worker.wait(1000));
    }

    void rejectsRedirectToUnsupportedProtocol()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        connect(&server, &QTcpServer::newConnection, &server, [&server] {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                socket->write("HTTP/1.1 302 Found\r\nLocation: ftp://example.com/media.mp4\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->flush();
                socket->waitForBytesWritten(1000);
                socket->disconnectFromHost();
            }
        });

        DecoderWorker worker;
        QSignalSpy opened(&worker, &DecoderWorker::mediaOpened);
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        worker.open(QStringLiteral("http://127.0.0.1:%1/redirect").arg(server.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 15000);
        QCOMPARE(opened.count(), 0);
        QVERIFY(worker.wait(1000));
    }

    void switchMedia100Times()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = createWav(directory, 250);
        QVERIFY(!path.isEmpty());

        DecoderWorker worker;
        worker.setSpeed(2.0);
        QSignalSpy errors(&worker, &DecoderWorker::playbackError);
        QElapsedTimer timer; timer.start();
        for (int i = 0; i < 100; ++i) {
            worker.open(path);
            QTest::qWait(5);
        }
        worker.requestStop();
        QVERIFY(worker.wait(2000));
        QCOMPARE(errors.count(), 0);
        QVERIFY(timer.elapsed() < 15000);
    }
};

QTEST_GUILESS_MAIN(DecoderWorkerTest)
#include "decoderworker_test.moc"
