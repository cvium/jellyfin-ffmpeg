/*
 * Copyright (c) 2021 Claus Vium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * simple video keyframes extractor based on the FFmpeg libraries
 */

#include "libavformat/avformat.h"

const char program_name[] = "ffkeyframes";
const int program_birth_year = 2021;

void show_help_default(const char *opt, const char *arg)
{
    av_log(NULL, AV_LOG_INFO, "Simple video keyframe extractor\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s input_file interval\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

int main (int argc, char **argv)
{
    const char *src_filename = NULL;
    int interval = 0;

    AVPacket *pkt;
    AVFormatContext *fmt_ctx = NULL;

    AVRational time_base;
    int ret, i = 0, video_stream_idx = -1;
    int64_t scaled_interval, timestamp = 0, last_pts = -1;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <video> <interval>\n", argv[0]);
        exit(1);
    }

    src_filename = argv[1];
    interval = strtol(argv[2], NULL, 10);

    if ((ret = avformat_open_input(&fmt_ctx, src_filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        goto end;
    }

    // discard all non-video streams to make it slightly faster
    for (i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx < 0) {
            video_stream_idx = i;
            fmt_ctx->streams[i]->discard = AVDISCARD_NONKEY;
            time_base = fmt_ctx->streams[video_stream_idx]->time_base;
        } else {
            fmt_ctx->streams[i]->discard = AVDISCARD_ALL;
        }
    }

    // exit if we didn't find a video stream
    if ((ret = video_stream_idx) < 0) {
        goto end;
    }

    pkt = av_packet_alloc();
    scaled_interval = av_rescale(
        interval,
        time_base.den,
        time_base.num
    );

    while (avformat_seek_file(fmt_ctx, video_stream_idx, timestamp, timestamp, timestamp, 0) >= 0) {
        if (av_read_frame(fmt_ctx, pkt) < 0) {
            break;
        }

        if (pkt->flags & AV_PKT_FLAG_KEY && last_pts < pkt->pts) {
            last_pts = pkt->pts;
            timestamp = pkt->pts;
            printf("%lld\n", pkt->pts);
        }

        timestamp += scaled_interval;
        av_packet_unref(pkt);
    }

    // it is assumed to have been successful if we found at least one keyframe
    ret = timestamp > 0;

end:
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);

    return ret < 0;
}
