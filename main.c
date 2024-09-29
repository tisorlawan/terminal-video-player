#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <stdio.h>

#define H 60
#define W 80

#define check_ffmpeg_err(context)  \
    do {                           \
        if (ret < 0) {             \
            err_context = context; \
            goto end;              \
        }                          \
    } while (0);

void usage();

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

void encoder_free(Encoder *e) {
    avformat_close_input(&e->in_avfc);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *ifname = argv[1];

    int ret = 0;
    const char *err_context = "";

    Encoder e = {0};
    ret = encoder_init_from_file(&e, ifname);
    check_ffmpeg_err("encoder_init_from_file");

    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();

    const char ascii_chars[] = " .:-=+*#%@";
    int num_chars = sizeof(ascii_chars) - 1;

    char ascii[(W + 1) * (H) + 1];

    while (av_read_frame(e.in_avfc, packet) >= 0) {
        if (packet->stream_index == e.video_idx) {
            ret = avcodec_send_packet(e.video_codec_context, packet);
            check_ffmpeg_err("avcodec_send_packet");

            if (packet->pts >= 20000) {
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(e.video_codec_context, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    ret = 0;
                    break;
                } else if (ret < 0) {
                    check_ffmpeg_err("avcodec_receive_frame");
                    break;
                }

                int box_width = frame->width / W;
                int box_height = frame->height / H;

                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        int sum = 0;
                        for (int by = 0; by < box_height; by++) {
                            int y_offset = (y * box_height + by) * frame->width;
                            for (int bx = 0; bx < box_width; bx++) {
                                int idx = y_offset + (x * box_width + bx);
                                sum += frame->data[0][idx];
                            }
                        }

                        int avg = sum / (box_width * box_height);
                        int char_index = avg * num_chars / 256;
                        ascii[y * W + x] = ascii_chars[char_index];
                    }
                    ascii[y * W] = '\n';
                }
                ascii[(W + 1) * H] = '\0';
                printf("%s", ascii);
            }
        }
        av_packet_unref(packet);
    }

end:
    av_frame_free(&frame);
    av_packet_free(&packet);

    encoder_free(&e);

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
