#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <ncurses.h>

#ifdef _WIN32
#include <windows.h>

void sleep_ms(int milliseconds) {
    Sleep(milliseconds);
}
#else
#include <unistd.h>

void sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}
#endif

#define check_ffmpeg_err(context)  \
    do {                           \
        if (ret < 0) {             \
            err_context = context; \
            goto end;              \
        }                          \
    } while (0);

void usage();

void init_ncurses() {
    initscr();
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Your terminal does not support color\n");
        exit(1);
    }
    cbreak();
    noecho();
    start_color();
    curs_set(0);
    for (int i = 0; i < 256; i++) {
        init_pair(i + 1, i, COLOR_BLACK);
    }
    clear();
}

typedef struct {
    AVFormatContext *in_avfc;
    int nb_streams;

    int video_idx;
    AVStream *video_stream;
    const AVCodec *video_codec;
    AVCodecContext *video_codec_context;

    int audio_idx;
    AVStream *audio_stream;
    const AVCodec *audio_codec;
} Encoder;

int encoder_init_from_file(Encoder *e, const char *fname) {
    int ret;

    ret = avformat_open_input(&e->in_avfc, fname, NULL, NULL);
    if (ret < 0) return ret;

    ret = avformat_find_stream_info(e->in_avfc, NULL);
    if (ret < 0) return ret;

    e->nb_streams = e->in_avfc->nb_streams;

    for (int i = 0; i < e->nb_streams; i++) {
        enum AVMediaType codec_type = e->in_avfc->streams[i]->codecpar->codec_type;

        if (codec_type != AVMEDIA_TYPE_AUDIO && codec_type != AVMEDIA_TYPE_VIDEO) {
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_VIDEO) {
            e->video_idx = i;
            e->video_stream = e->in_avfc->streams[i];
            e->video_codec = avcodec_find_decoder(e->video_stream->codecpar->codec_id);

            e->video_codec_context = avcodec_alloc_context3(e->video_codec);
            ret = avcodec_parameters_to_context(e->video_codec_context, e->video_stream->codecpar);
            if (ret < 0) return ret;

            ret = avcodec_open2(e->video_codec_context, e->video_codec, NULL);
            if (ret < 0) return ret;
        } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
            e->audio_idx = i;
            e->audio_stream = e->in_avfc->streams[i];
            e->audio_codec = avcodec_find_decoder(e->in_avfc->streams[i]->codecpar->codec_id);
        }
    }

    return ret;
}

typedef struct {
    uint8_t y;
    uint8_t u;
    uint8_t v;
} Point;

void encoder_free(Encoder *e) {
    avformat_close_input(&e->in_avfc);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *ifname = argv[1];

    init_ncurses();

    int ret = 0;
    const char *err_context = "";

    Encoder e = {0};
    ret = encoder_init_from_file(&e, ifname);
    check_ffmpeg_err("encoder_init_from_file");

    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();

    /* const char ascii_chars[] = " .:-=+*#%@"; */
    const char ascii_chars[] =
        " .'`^\",:;Il!i><~+_-?][}{1)(|/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
    int num_chars = sizeof(ascii_chars) - 1;

    Point *points = malloc(LINES * COLS * sizeof(Point));

    double frame_ms =
        1000.0 * e.video_stream->avg_frame_rate.den / e.video_stream->avg_frame_rate.num;

    while (av_read_frame(e.in_avfc, packet) >= 0) {
        if (packet->stream_index == e.video_idx) {
            ret = avcodec_send_packet(e.video_codec_context, packet);
            check_ffmpeg_err("avcodec_send_packet");

            while (ret >= 0) {
                ret = avcodec_receive_frame(e.video_codec_context, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    ret = 0;
                    break;
                } else if (ret < 0) {
                    check_ffmpeg_err("avcodec_receive_frame");
                    break;
                }

                clock_t start = clock();
                int box_width = frame->width / COLS;
                int box_height = frame->height / LINES;

                for (int y = 0; y < LINES; y++) {
                    for (int x = 0; x < COLS; x++) {
                        int y_sum = 0;
                        int u_sum = 0;
                        int v_sum = 0;

                        int uv_count = 0;

                        for (int by = 0; by < box_height; by++) {
                            int y_offset = (y * box_height + by) * frame->width;
                            for (int bx = 0; bx < box_width; bx++) {
                                int idx = y_offset + (x * box_width + bx);
                                y_sum += frame->data[0][idx];

                                if (by % 2 == 0 && bx % 2 == 0) {
                                    int uv_x = (x * box_width + bx) / 2;
                                    int uv_y = (y * box_height + by) / 2;

                                    u_sum += frame->data[1][uv_y * (frame->width / 2) + uv_x];
                                    v_sum += frame->data[2][uv_y * (frame->width / 2) + uv_x];
                                    uv_count += 1;
                                }
                            }
                        }

                        points[y * COLS + x] = (Point){
                            .y = y_sum / (box_width * box_height),
                            .u = u_sum / uv_count,
                            .v = v_sum / uv_count,
                        };
                    }
                }

                double real_elapsed = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000;

                for (int y = 0; y < LINES; y++) {
                    for (int x = 0; x < COLS; x++) {
                        Point p = points[y * COLS + x];

                        int c = p.y - 16;
                        int d = p.u - 128;
                        int e = p.v - 128;

                        int r = (298 * c + 409 * e + 128) >> 8;
                        int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                        int b = (298 * c + 516 * d + 128) >> 8;

                        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                        b = (b < 0) ? 0 : (b > 255) ? 255 : b;

                        int char_index = p.y * num_chars / 256;
                        int color = (r / 32 * 36) + (g / 32 * 6) + (b / 32) + 16;
                        move(y, x);
                        attron(COLOR_PAIR(color + 1));
                        addch(ascii_chars[char_index]);
                        attroff(COLOR_PAIR(color + 1));
                    }
                }
                refresh();
                sleep_ms(frame_ms - real_elapsed);
            }
        }
        av_packet_unref(packet);
    }

end:
    av_frame_free(&frame);
    av_packet_free(&packet);
    encoder_free(&e);

    endwin();

    free(points);

    if (ret < 0 && ret != AVERROR_EOF) {
        if (err_context) {
            fprintf(stderr, "[Error] ffmpeg <%s>: %s\n", err_context, av_err2str(ret));
        } else {
            fprintf(stderr, "[Error] ffmpeg: %s\n", av_err2str(ret));
        }
        return 1;
    }
}

void usage() {
    fprintf(stderr, "Usage: ./main <input>\n");
}
