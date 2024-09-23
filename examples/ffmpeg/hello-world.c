#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);
static void logging(const char *fmt, ...);

int main(int argc, char **args) {
    if (argc < 2) {
        printf("Usage: ./main <path>");
        exit(1);
    }

    const char *fname = args[1];

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        logging("[ERROR] could not allocate memory for Format Context");
        return -1;
    }

    if (avformat_open_input(&format_ctx, fname, NULL, NULL) != 0) {
        logging("[ERROR] could not open the file");
        return -1;
    }

    logging("Format: %s", format_ctx->iformat->long_name);
    logging("Duration: %ld", format_ctx->duration);

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        logging("[ERROR] could not get the stream info");
        return -1;
    }

    logging("Number of streams: %d", format_ctx->nb_streams);

    const AVCodec *video_codec = NULL;
    AVCodecParameters *vcodec_params = NULL;
    int vstream_index = -1;

    for (size_t i = 0; i < format_ctx->nb_streams; i++) {
        AVCodecParameters *local_codec_param = format_ctx->streams[i]->codecpar;

        const AVCodec *local_codec = avcodec_find_decoder(local_codec_param->codec_id);
        if (!local_codec) {
            continue;
        }

        if (local_codec_param->codec_type == AVMEDIA_TYPE_VIDEO) {
            logging("Video: %dx%d", local_codec_param->width, local_codec_param->height);
            if (vstream_index == -1) {
                vstream_index = i;
                video_codec = local_codec;
                vcodec_params = local_codec_param;
            }
            logging("AVRationale Stream: %d/%d", format_ctx->streams[i]->time_base.num,
                    format_ctx->streams[i]->time_base.den);
        } else if (local_codec_param->codec_type == AVMEDIA_TYPE_AUDIO) {
            logging("Audio: %d channels, sample rate %d", local_codec_param->ch_layout.nb_channels,
                    local_codec_param->sample_rate);
        }

        logging("Codec: %s, Id: %d, BitRate: %ld", local_codec->long_name, local_codec->id,
                local_codec_param->bit_rate);
        printf("\n");
    }

    if (vstream_index == -1) {
        logging("File %s does not contain a video stream", fname);
        return -1;
    }

    AVCodecContext *vcodec_ctx = avcodec_alloc_context3(video_codec);
    if (!vcodec_ctx) {
        logging("[ERROR]: failed to allocated memory for AVCodecContext");
        return -1;
    }
    if (avcodec_parameters_to_context(vcodec_ctx, vcodec_params) < 0) {
        logging("[ERROR]: failed to copy params to codec context");
        return -1;
    }
    if (avcodec_open2(vcodec_ctx, video_codec, NULL) < 0) {
        logging("[ERROR]: failed to open codec through avcodec_open2");
        return -1;
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        logging("[ERROR]: failed to allocate memory for AVPacket");
        return -1;
    }
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        logging("[ERROR]: failed to allocate memory for AVFrame");
        return -1;
    }

    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == vstream_index) {
            int response = avcodec_send_packet(vcodec_ctx, packet);
            if (response < 0) {
                logging("Error sending packet: %s", av_err2str(response));
                return -1;
            }

            while (response >= 0) {
                response = avcodec_receive_frame(vcodec_ctx, frame);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                    break;
                } else if (response < 0) {
                    logging("Error while receiving a frame from the decoder: %s",
                            av_err2str(response));
                    return -1;
                }

                logging("Frame %" PRId64
                        " [%c] [size=%d] [%dx%d] (%d, %d, %d) (%s) (keyframe = %d)",
                        vcodec_ctx->frame_num, av_get_picture_type_char(frame->pict_type),
                        packet->size, frame->width, frame->height, frame->linesize[0],
                        frame->linesize[1], frame->linesize[2], av_get_pix_fmt_name(frame->format),
                        frame->flags & AV_FRAME_FLAG_KEY);

                char frame_fname[1024];
                snprintf(frame_fname, sizeof(frame_fname), "frames/%s-%ld.pgm", "frame",
                         vcodec_ctx->frame_num);
                if (frame->format != AV_PIX_FMT_YUV420P) {
                    printf("Warning: the generated file may not be a grayscale image, but could "
                           "e.g. be just the R component if the video format is RGB\n");
                }

                save_gray_frame(frame->data[0], frame->linesize[0], frame->width, frame->height,
                                frame_fname);
            }
            av_packet_unref(packet);
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&vcodec_ctx);
    avformat_close_input(&format_ctx);
}

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename) {
    FILE *f;
    int i;
    f = fopen(filename, "w");
    if (f == NULL) {
        perror("error file");
        exit(1);
    }
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++) {
        fwrite(buf + i * wrap, 1, xsize, f);
    }
    fclose(f);
}

static void logging(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "LOG: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
