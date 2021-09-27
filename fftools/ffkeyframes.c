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

int main (int argc, char **argv)
{
    AVFormatContext *fmt_ctx = NULL;
    const char *src_filename = NULL;
    int interval = NULL;

    int video_stream_idx = -1;
    AVPacket *pkt;
    int ret, i = 0;
    AVRational time_base;
    int64_t scaled_interval, timestamp = 0, last_pts = -1;
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <video> <interval>\n", argv[0]);
        exit(1);
    }

    src_filename = argv[1];
    interval = argv[2];

    if ((ret = avformat_open_input(&fmt_ctx, src_filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        goto end;
    }

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
            printf("%ld\n", pkt->pts);
        }

        timestamp += scaled_interval;
        av_packet_unref(pkt);
    }

    ret = 0;

end:
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);

    return ret < 0;
}
