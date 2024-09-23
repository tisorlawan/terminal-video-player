#include <libavformat/avformat.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: ./main <input_path> <output_path>\n");
        return -1;
    }
    const char *input_fname = argv[1];
    const char *output_fname = argv[2];
    printf("Input Fname: %s\n", input_fname);

    int ffmpeg_ret = 0;
    char *err_context = NULL;
    int *stream_list = NULL;

    AVFormatContext *input_format_ctx = NULL;
    AVFormatContext *output_format_ctx = NULL;

    ffmpeg_ret = avformat_open_input(&input_format_ctx, input_fname, NULL, NULL);
    if (ffmpeg_ret < 0) {
        err_context = "avformat_open_input";
        goto end;
    }

    ffmpeg_ret = avformat_find_stream_info(input_format_ctx, NULL);
    if (ffmpeg_ret < 0) {
        err_context = "avformat_find_stream_info";
        goto end;
    }

    ffmpeg_ret = avformat_alloc_output_context2(&output_format_ctx, NULL, NULL, output_fname);
    if (ffmpeg_ret < 0) {
        err_context = "avformat_alloc_output_context2 | could not create output context";
        goto end;
    }

    int nb_streams = input_format_ctx->nb_streams;
    stream_list = av_malloc_array(nb_streams, sizeof(*stream_list));
    if (!stream_list) {
        ffmpeg_ret = AVERROR(ENOMEM);
        goto end;
    }

    printf("Nb streams: %d\n", nb_streams);

    int stream_index = 0;
    for (int i = 0; i < nb_streams; i++) {
        AVStream *in_stream = input_format_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            stream_list[i] = -1;
            continue;
        }

        stream_list[i] = stream_index++;
        AVStream *out_stream = avformat_new_stream(output_format_ctx, NULL);
        if (!out_stream) {
            ffmpeg_ret = AVERROR_UNKNOWN;
            err_context = "avformat_new_stream | could not create output stream";
            goto end;
        }

        ffmpeg_ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ffmpeg_ret < 0) {
            err_context = "avcodec_parameters_copy | could not copy parameters";
            goto end;
        }
    }

    av_dump_format(output_format_ctx, 0, output_fname, 1);

    if (!(output_format_ctx->oformat->flags & AVFMT_NOFILE)) {
        ffmpeg_ret = avio_open(&output_format_ctx->pb, output_fname, AVIO_FLAG_WRITE);
        if (ffmpeg_ret < 0) {
            err_context = "avio_open";
            goto end;
        }
    }

    AVDictionary *opts = NULL;
    ffmpeg_ret = avformat_write_header(output_format_ctx, &opts);
    if (ffmpeg_ret < 0) {
        err_context = "avformat_write_header";
        goto end;
    }

    AVPacket packet;
    int i = 0;
    while (1) {
        AVStream *in_stream, *out_stream;
        ffmpeg_ret = av_read_frame(input_format_ctx, &packet);

        if (ffmpeg_ret < 0) {
            break;
        }
        i += 1;

        in_stream = input_format_ctx->streams[packet.stream_index];
        if (packet.stream_index >= nb_streams || stream_list[packet.stream_index] < 0) {
            av_packet_unref(&packet);
            continue;
        }

        packet.stream_index = stream_list[packet.stream_index];
        out_stream = output_format_ctx->streams[packet.stream_index];

        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        packet.duration =
            av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        packet.pos = -1;

        ffmpeg_ret = av_interleaved_write_frame(output_format_ctx, &packet);
        if (ffmpeg_ret < 0) {
            err_context = "error muxing";
            break;
        }

        av_packet_unref(&packet);
    }
    av_write_trailer(output_format_ctx);

end:
    if (output_format_ctx && !(output_format_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_format_ctx->pb);
    }
    avformat_free_context(output_format_ctx);
    if (stream_list) av_free(stream_list);

    if (ffmpeg_ret < 0 && ffmpeg_ret != AVERROR_EOF) {
        if (err_context) {
            fprintf(stderr, "[Error] ffmpeg <%s>: %s\n", err_context, av_err2str(ffmpeg_ret));
        } else {
            fprintf(stderr, "[Error] ffmpeg: %s\n", av_err2str(ffmpeg_ret));
        }
        return 1;
    }

    return 0;
}
