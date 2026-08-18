#pragma once

#include <QString>

struct AVDictionary;

namespace NetworkPolicy {

struct UrlValidation {
    bool accepted = false;
    QString error;
};

UrlValidation validateMediaUrl(const QString &text);
void applyFfmpegInputOptions(AVDictionary **options, bool https);

}
