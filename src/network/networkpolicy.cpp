#include "networkpolicy.h"

#include <QUrl>

extern "C" {
#include <libavutil/dict.h>
}

namespace NetworkPolicy {

UrlValidation validateMediaUrl(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return {false, QStringLiteral("请输入网络媒体地址")};
    if (trimmed.size() > 4096) return {false, QStringLiteral("网络媒体地址过长")};
    for (const QChar character : trimmed) {
        if (character.category() == QChar::Other_Control)
            return {false, QStringLiteral("网络媒体地址包含非法控制字符")};
    }

    const QUrl url(trimmed, QUrl::StrictMode);
    if (!url.isValid() || (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
                           && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0))
        return {false, QStringLiteral("请输入有效的 HTTP 或 HTTPS 地址")};
    if (url.host().isEmpty()) return {false, QStringLiteral("网络媒体地址缺少主机名")};
    if (!url.userInfo().isEmpty()) return {false, QStringLiteral("网络媒体地址不能包含用户名或密码")};
    return {true, {}};
}

void applyFfmpegInputOptions(AVDictionary **options, bool https)
{
    av_dict_set(options, "timeout", "3000000", 0);
    av_dict_set(options, "rw_timeout", "3000000", 0);
    av_dict_set(options, "max_redirects", "8", 0);
    av_dict_set(options, "protocol_whitelist", "http,https,tcp,tls,crypto", 0);
    if (https) av_dict_set(options, "tls_verify", "1", 0);
}

}
