#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

int main(void)
{
    if (avformat_network_init() < 0) {
        fputs("avformat_network_init failed\n", stderr);
        return 1;
    }

    printf("FFmpeg: %s\n", av_version_info());
    printf("libavformat: %u\n", avformat_version());
    printf("libavcodec: %u\n", avcodec_version());
    printf("libavfilter: %u\n", avfilter_version());
    printf("libswresample: %u\n", swresample_version());
    printf("libswscale: %u\n", swscale_version());

    avformat_network_deinit();
    return 0;
}
