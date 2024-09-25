#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <stdio.h>

#define check_ffmpeg_err(context)  \
    do {                           \
        if (ret < 0) {             \
            err_context = context; \
            goto end;              \
        }                          \
    } while (0);

void usage();

int main(int argc, char **argv) {
    if (argc < 3) {
        usage();
        return 1;
    }

    const char *ifname = argv[1];
    const char *ofname = argv[2];

    int ret = 0;
    const char *err_context = "";

    AVFormatContext *in_avfc = NULL;

    ret = avformat_open_input(&in_avfc, ifname, NULL, NULL);
    check_ffmpeg_err("avformat_open_input");

    ret = avformat_find_stream_info(in_avfc, NULL);
    check_ffmpeg_err("avformat_find_stream_info");

    int nb_streams = in_avfc->nb_streams;
    printf("* NB Streams: %d\n", nb_streams);

    AVStream *in_video_stream = NULL, *in_audio_stream = NULL;
    int in_video_idx = -1;
    int in_audio_idx = -1;

    for (int i = 0; i < nb_streams; i++) {
        AVStream *avs = in_avfc->streams[i];
        enum AVMediaType codec_type = avs->codecpar->codec_type;
        if (codec_type != AVMEDIA_TYPE_VIDEO && codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_VIDEO) {
            in_video_stream = avs;
            in_video_idx = i;
        } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
            in_audio_stream = avs;
            in_audio_idx = i;
        }

        const AVCodec *av_codec = avcodec_find_decoder(avs->codecpar->codec_id);
        AVCodecContext *av_codec_ctx = avcodec_alloc_context3(av_codec);
        avcodec_parameters_to_context(av_codec_ctx, avs->codecpar);
        avcodec_open2(av_codec_ctx, av_codec, NULL);
    }

    AVFormatContext *out_avfc = NULL;
    ret = avformat_alloc_output_context2(&out_avfc, NULL, NULL, ofname);
    check_ffmpeg_err("avformat_alloc_output_context2");

    printf("* Decoder audio index: %d\n", in_audio_idx);
    printf("* Decoder video index: %d\n", in_video_idx);

    AVStream *out_video_stream = avformat_new_stream(out_avfc, NULL);
    ret = avcodec_parameters_copy(out_video_stream->codecpar, in_video_stream->codecpar);
    check_ffmpeg_err("avcodec_parameters_copy");

    if (out_avfc->oformat->flags & AVFMT_GLOBALHEADER) {
        out_avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary *muxer_opts = NULL;
    avio_open(&out_avfc->pb, ofname, AVIO_FLAG_WRITE);
    ret = avformat_write_header(out_avfc, &muxer_opts);
    check_ffmpeg_err("avformat_write_header");

end:
    avformat_close_input(&in_avfc);

    if (ret < 0) {
        if (err_context) {
            fprintf(stderr, "ffmpeg error [%s]: %s\n", err_context, av_err2str(ret));
        } else {

            fprintf(stderr, "ffmpeg error: %s\n", av_err2str(ret));
        }
        return 1;
    }
    return 0;
}

void usage() {
    fprintf(stderr, "Usage: ./main <input> <output>\n");
}
