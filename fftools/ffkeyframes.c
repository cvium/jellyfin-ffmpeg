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

#include "cmdutils.h"
#include "libavformat/avformat.h"
#include "libavformat/internal.h"

#include "libavutil/common.h"
#include "libavutil/avutil.h"

#define TIME_BASE_TICKS 10000000
#define TIME_BASE_Q_TICKS (AVRational){1, TIME_BASE_TICKS}

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
    int nb_keyframes = 0;
    int (*read_packet)(struct AVFormatContext *, AVPacket *pkt) = NULL;
    AVPacket *pkt;
    AVInputFormat *iformat;
    AVFormatContext *fmt_ctx = NULL;

    AVRational time_base;
    int ret, i = 0, video_stream_idx = -1;
    int64_t scaled_interval, timestamp = 0, last_pts = 0, scaled_pts = -1;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <video> <interval>\n", argv[0]);
        exit(1);
    }

    src_filename = argv[1];
    interval = strtol(argv[2], NULL, 10);

    iformat = av_find_input_format(src_filename);

    if ((ret = avformat_open_input(&fmt_ctx, src_filename, iformat, NULL)) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        goto end;
    }

    video_stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    // discard all non-video streams to make it slightly faster
    for (i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (video_stream_idx == i) {
            // video_stream_idx = i;
            fmt_ctx->streams[i]->discard = AVDISCARD_NONKEY;
            time_base = fmt_ctx->streams[video_stream_idx]->time_base;
        } else {
            fmt_ctx->streams[i]->discard = AVDISCARD_ALL;
        }
    }

    // exit if we didn't find a video stream
    if ((ret = video_stream_idx) < 0) {
        fprintf(stderr, "Could not find video stream\n");
        goto end;
    }

    pkt = av_packet_alloc();
    scaled_interval = av_rescale(
        interval,
        time_base.den,
        time_base.num
    );
    
    /* retrieve stream information */
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }

    if (fmt_ctx->streams[video_stream_idx]->duration != AV_NOPTS_VALUE) {
        printf("duration:%ld\n", av_rescale_q(fmt_ctx->streams[video_stream_idx]->duration, time_base, TIME_BASE_Q_TICKS));
    } else {
        printf("duration:%ld\n", av_rescale_q(fmt_ctx->duration, AV_TIME_BASE_Q, TIME_BASE_Q_TICKS));
    }

    do {
        // set up the read_packet pointer
        if (read_packet == NULL) {
            if (ff_read_packet(fmt_ctx, pkt) < 0) {
                break;
            }

            // if pts is AV_NOPTS_VALUE we fall back to the slower, but more accurate av_read_frame
            read_packet = pkt->pts == AV_NOPTS_VALUE ? av_read_frame : ff_read_packet;
        } else if (read_packet(fmt_ctx, pkt) < 0) {
            break;
        }

        scaled_pts = av_rescale_q(pkt->pts, time_base, TIME_BASE_Q_TICKS);

        if (pkt->flags & AV_PKT_FLAG_KEY && last_pts < scaled_pts) {
            nb_keyframes++;
            last_pts = scaled_pts;
            timestamp = pkt->pts + 1;

            printf("%ld\n", last_pts);
        } else {
            timestamp = FFMAX(timestamp, pkt->pts) + scaled_interval;
        }

        av_packet_unref(pkt);
    } while (av_seek_frame(fmt_ctx, video_stream_idx, timestamp, 0) >= 0);

    printf("keyframes_count:%d\n", nb_keyframes);

    // it is assumed to have been successful if we found at least one keyframe
    ret = nb_keyframes > 0;

end:
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);

    return ret < 0;
}
