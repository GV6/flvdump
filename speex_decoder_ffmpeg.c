#include "speex_decoder_ffmpeg.h"

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
/*
#include "libswscale/swscale.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"
#include "libavcodec/put_bits.h"
#include "libavcodec/get_bits.h"
#include "libavutil/frame.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
*/
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/avcodec.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixelutils.h"
#include <fcntl.h>
#include <unistd.h>

AVFrame *m_AvFrame_audio = NULL;
AVCodec *m_AvCodec_audio = NULL;
AVCodecContext *m_AvCodecContext_audio = NULL;

AVCodec *encoder_context;
AVCodecContext *encoder_avcodec_context;

int speex_raw_file;

typedef struct FilteringContext {
	AVFilterContext *buffersink_ctx;
	AVFilterContext *buffersrc_ctx;
	AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

static int encode_write_frame(AVFrame *filt_frame, int *got_frame,
		AVPacket *enc_pkt) {
	int ret;
	int got_frame_local;

	if (!got_frame)
		got_frame = &got_frame_local;

	ret = avcodec_encode_audio2(encoder_avcodec_context, enc_pkt, filt_frame,
			got_frame);

	av_frame_free(&filt_frame);
	if (ret < 0)
	{
		printf("JJJJ1\n");
		return ret;
	}
	if (!(*got_frame))
	{		
		printf("JJJJ2\n");
		return 0;
	}

	return enc_pkt->size;
}
static int filter_encode_write_frame(AVFrame *frame, AVPacket *enc_pkt) {
	int ret = 0;
	AVFrame *filt_frame = NULL;
	printf("RRRRR %d %d %d %d\n", frame->sample_rate, 
				frame->channel_layout,
                                av_frame_get_channels(frame), 
				frame->format);
	ret = av_buffersrc_add_frame_flags(filter_ctx->buffersrc_ctx, frame, 0);
	if (ret < 0) {
		return ret;
	}
		printf("RRRRR1\n");

	while (1) {
		filt_frame = av_frame_alloc();
		if (!filt_frame) {
			ret = AVERROR(ENOMEM);
			break;
		}

		ret = av_buffersink_get_frame(filter_ctx->buffersink_ctx, filt_frame);
		if (ret < 0) {
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				ret = 0;
			av_frame_free(&filt_frame);
			break;
		}

		filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
		ret = encode_write_frame(filt_frame, NULL, enc_pkt);
		if (ret < 0) {
			break;
		}
	}
		printf("RRRRR2\n");

	return ret;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
		AVCodecContext *enc_ctx, const char *filter_spec) {
	char args[512] = { 0 };
	int ret = 0;
	AVFilter *buffersrc = NULL;
	AVFilter *buffersink = NULL;
	AVFilterContext *buffersrc_ctx = NULL;
	AVFilterContext *buffersink_ctx = NULL;
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs = avfilter_inout_alloc();
	AVFilterGraph *filter_graph = avfilter_graph_alloc();

	if (!outputs || !inputs || !filter_graph) {
		ret = AVERROR(ENOMEM);
		printf("LLLLL1\n");
		goto end;
	}

	buffersrc = avfilter_get_by_name("abuffer");
	buffersink = avfilter_get_by_name("abuffersink");
	if (!buffersrc || !buffersink) {
		ret = AVERROR_UNKNOWN;
		printf("LLLLL2\n");
		goto end;
	}

//	snprintf(args, sizeof(args),
//			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%d",
//	                dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
//	                av_get_sample_fmt_name(dec_ctx->sample_fmt),
//	                3);

	memcpy(args,
			"time_base=1/32000:sample_rate=32000:sample_fmt=s16:channel_layout=0x3",
			510);

	ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args,
			NULL, filter_graph);

	if (ret < 0) {
		printf("LLLLL3\n");
		goto end;
	}

	ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL,
			NULL, filter_graph);
	if (ret < 0) {
		printf("LLLLL4\n");
		goto end;
	}

	ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
			(uint8_t*) &enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
			AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		printf("LLLLL5\n");
		goto end;
	}

	ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
			(uint8_t*) &enc_ctx->channel_layout,
			sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		printf("LLLLL6\n");
		goto end;
	}

	ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
			(uint8_t*) &enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
			AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		printf("LLLLL7\n");
		goto end;
	}

	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if (!outputs->name || !inputs->name) {
		ret = AVERROR(ENOMEM);
		printf("LLLLL8\n");
		goto end;
	}

	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs,
			&outputs, NULL)) < 0) {
		printf("LLLLL9\n");
		goto end;
	}
	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
		printf("LLLLL10\n");
		goto end;
	}

	fctx->buffersrc_ctx = buffersrc_ctx;
	fctx->buffersink_ctx = buffersink_ctx;
	fctx->filter_graph = filter_graph;

	end: avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return ret;
}

static int init_filters(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx) {
	const char *filter_spec;
	int ret;

	filter_ctx = (FilteringContext *) malloc(sizeof(*filter_ctx));
	if (!filter_ctx)
		return AVERROR(ENOMEM);

	filter_ctx->buffersrc_ctx = NULL;
	filter_ctx->buffersink_ctx = NULL;
	filter_ctx->filter_graph = NULL;

	filter_spec = "anull";

	ret = init_filter(filter_ctx, dec_ctx, enc_ctx, filter_spec);
	if(ret < 0)
		printf("LLLLL\n");
	return ret;
}

int speex_decoder_init()
{
	avcodec_register_all();
	avfilter_register_all();
	m_AvCodec_audio = avcodec_find_decoder(CODEC_ID_SPEEX);
	if (!m_AvCodec_audio) {
		printf("qqqqq1\n");
		return -1;
	}

	m_AvCodecContext_audio = avcodec_alloc_context3(m_AvCodec_audio);
	if (!m_AvCodecContext_audio) {
		printf("qqqqq2\n");
		return -1;
	}

	if (avcodec_open2(m_AvCodecContext_audio, m_AvCodec_audio, NULL) < 0) {
		printf("qqqqq3\n");
		return -1;
	}

	if (!(m_AvFrame_audio = av_frame_alloc())) {
		printf("qqqqq4\n");
		return -1;
	} 

	encoder_context = avcodec_find_encoder(CODEC_ID_PCM_S16LE);
	if (NULL == encoder_context) {
		printf("qqqqq5\n");
		return -1;
	}

	encoder_avcodec_context = avcodec_alloc_context3(NULL);
	if (!encoder_avcodec_context) {
		printf("qqqqq6\n");
		return -1;
	}	

	encoder_avcodec_context->sample_rate = 48000;
	encoder_avcodec_context->channel_layout = AV_CH_LAYOUT_STEREO;
	encoder_avcodec_context->channels = 2;
	encoder_avcodec_context->sample_fmt = AV_SAMPLE_FMT_S16;

	if (avcodec_open2(encoder_avcodec_context, encoder_context, NULL) < 0) {
		printf("qqqqq7\n");
		return -1;
	}

	if (init_filters(m_AvCodecContext_audio, encoder_avcodec_context) < 0) {
		printf("qqqqq8\n");
		return -1;
	}

//	aac_file = open("test.aac", O_CREAT | O_TRUNC | O_RDWR);
//	if(aac_file < 0)
//		return -1;

	speex_raw_file = open("test_raw.speex", O_CREAT | O_TRUNC | O_RDWR);
	if(speex_raw_file < 0)
	{
		perror("OOOOO:");
		return -1;
	}

	return 0;
}

int speex_decoder(unsigned char *buf, int size)
{
	int len;
	int got_frame;
	AVPacket av_packet;
	av_init_packet(&av_packet);
	int ret;
	av_packet.data = buf;
	av_packet.size = size;

	AVPacket enc_pkt;
	av_init_packet(&enc_pkt);
	enc_pkt.data = NULL;
	enc_pkt.size = 0;

	len = avcodec_decode_audio4(m_AvCodecContext_audio, m_AvFrame_audio,
			&got_frame, &av_packet);	
	if(len < 0)
	{
		printf("Len < 0\n");
		return -1;
	}

	if(got_frame)
	{
		ret = filter_encode_write_frame(m_AvFrame_audio, &enc_pkt);
		if (ret < 0) {
			printf("Failed\n");
			return ret;
		}

		printf("Succeed\n");
		write(speex_raw_file, enc_pkt.data, enc_pkt.size);
	}

printf("JJJ %d %d %d %d\n", m_AvCodecContext_audio->channels, m_AvCodecContext_audio->sample_rate,m_AvFrame_audio->nb_samples, m_AvCodecContext_audio->sample_fmt);

	return 0;
}









