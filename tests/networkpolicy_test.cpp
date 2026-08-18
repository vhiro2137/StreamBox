#include <QtTest>

extern "C" {
#include <libavutil/dict.h>
}

#include "network/networkpolicy.h"

class NetworkPolicyTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsSupportedMediaUrls()
    {
        QVERIFY(NetworkPolicy::validateMediaUrl(QStringLiteral("http://example.com/media.mp3")).accepted);
        QVERIFY(NetworkPolicy::validateMediaUrl(QStringLiteral("https://example.com/video.mp4?token=abc")).accepted);
    }

    void rejectsUnsafeOrUnsupportedUrls()
    {
        QVERIFY(!NetworkPolicy::validateMediaUrl(QStringLiteral("ftp://example.com/file.mp4")).accepted);
        QVERIFY(!NetworkPolicy::validateMediaUrl(QStringLiteral("file:///C:/secret.mp4")).accepted);
        QVERIFY(!NetworkPolicy::validateMediaUrl(QStringLiteral("https:///missing-host.mp4")).accepted);
        QVERIFY(!NetworkPolicy::validateMediaUrl(QStringLiteral("https://user:password@example.com/file.mp4")).accepted);
        QVERIFY(!NetworkPolicy::validateMediaUrl(QStringLiteral("https://example.com/a\n.mp4")).accepted);
        QVERIFY(!NetworkPolicy::validateMediaUrl(QStringLiteral("https://example.com/") + QString(4096, QLatin1Char('a'))).accepted);
    }

    void appliesBoundedVerifiedFfmpegOptions()
    {
        AVDictionary *options = nullptr;
        NetworkPolicy::applyFfmpegInputOptions(&options, true);
        QCOMPARE(QString::fromUtf8(av_dict_get(options, "tls_verify", nullptr, 0)->value), QStringLiteral("1"));
        QCOMPARE(QString::fromUtf8(av_dict_get(options, "max_redirects", nullptr, 0)->value), QStringLiteral("8"));
        QCOMPARE(QString::fromUtf8(av_dict_get(options, "protocol_whitelist", nullptr, 0)->value), QStringLiteral("http,https,tcp,tls,crypto"));
        av_dict_free(&options);
    }
};

QTEST_APPLESS_MAIN(NetworkPolicyTest)
#include "networkpolicy_test.moc"
