#include "h264_decoder_ffmpeg.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ppm.h"
#include "bmp.h"

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "libavutil/intreadwrite.h"
#include "libavutil/avassert.h"
#include "libavutil/pixfmt.h"

#define TAG_DATA_OFFSET 15
#define TAG_HEADER_LENGTH 16

typedef struct video_decodec_s {
	int *run_status;

	int width;
	int height;

	int has_SPS;
	unsigned char SPS_PPS_buf[512];
	unsigned char sps_data[128];
	unsigned char pps_data[128];
	int SPS_PPS_length;
	unsigned char head4[4];

	unsigned char *rgba;
	unsigned char *frame_buffer;
	int frame_buffer_lenth;
	int decode_size;
} video_decodec_t;

AVCodec *m_AvCodec_video = NULL;
AVCodecContext *m_AvCodecContext_video = NULL;
struct SwsContext *m_AvConvertContext = NULL;

AVFrame *m_AvFrame_video;
AVFrame *m_AvFrameRGB;

video_decodec_t *video_decodec;

static AVFrame * icv_alloc_picture_FFMPEG(int pix_fmt, int width, int height,
		int alloc) {
	AVFrame * picture;
	uint8_t * picture_buf;
	int size;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;
	size = avpicture_get_size((enum AVPixelFormat) pix_fmt, width, height);
	if (alloc) {
		picture_buf = (uint8_t *) malloc(size);
		if (!picture_buf) {
			av_frame_free(&picture);
			return NULL;
		}
		avpicture_fill((AVPicture *) picture, picture_buf,
				(enum AVPixelFormat) pix_fmt, width, height);
	}
	return picture;
}

int h264_decoder_init(int nImageWidth, int nImageHeight)
{
	avcodec_register_all();

	m_AvCodec_video = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!m_AvCodec_video)
	{
		printf("Find decoder failed\n");
		return -1;
	}

	m_AvCodecContext_video = avcodec_alloc_context3(m_AvCodec_video);
	m_AvCodecContext_video->width = nImageWidth;
	m_AvCodecContext_video->height = nImageHeight;
	m_AvCodecContext_video->extradata = NULL;
	m_AvCodecContext_video->pix_fmt = AV_PIX_FMT_YUV420P;
	m_AvCodecContext_video->time_base.num = 10;
	m_AvCodecContext_video->bit_rate = 0;
	m_AvCodecContext_video->frame_number = 1;
	m_AvCodecContext_video->codec_type = AVMEDIA_TYPE_VIDEO;
	m_AvConvertContext = sws_getContext(nImageWidth, nImageHeight,
			AV_PIX_FMT_YUV420P, nImageWidth, nImageHeight, AV_PIX_FMT_RGBA,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);

	if (avcodec_open2(m_AvCodecContext_video, m_AvCodec_video, NULL) < 0) {
		printf("avcodec_open2 faiied\n");
		return -1;
	}

	m_AvFrame_video = icv_alloc_picture_FFMPEG(AV_PIX_FMT_YUV420P, nImageWidth, nImageHeight, 1);
	m_AvFrameRGB = icv_alloc_picture_FFMPEG(AV_PIX_FMT_RGBA, nImageWidth, nImageHeight, 1);

	if (m_AvFrame_video == NULL || m_AvFrameRGB == NULL)	
	{
		printf("XX\n");
		return -1;
	}

	if (m_AvCodec_video->capabilities & CODEC_CAP_TRUNCATED)
		m_AvCodecContext_video->flags |= CODEC_FLAG_TRUNCATED;

	video_decodec = (video_decodec_t *)calloc(1, sizeof(video_decodec_t));
	video_decodec->head4[0] = 0;
	video_decodec->head4[1] = 0;
	video_decodec->head4[2] = 0;
	video_decodec->head4[3] = 1;
	video_decodec->has_SPS = 0;
	video_decodec->frame_buffer = (unsigned char *) calloc(1, 1024*10*10);
	return 0;	
}

unsigned char * proc_tagData(unsigned char * tagData, int dataLen) {
	int tag_len = (((int) tagData[1] << 16) & 0xff0000)
		| (((int) tagData[2] << 8) & 0xff00) | ((int) tagData[3] & 0xff);

	if (tag_len + TAG_DATA_OFFSET != dataLen) {
		return NULL;
	}

	int slice_len = (((int) tagData[TAG_HEADER_LENGTH] << 24) & 0xff000000)
		| (((int) tagData[TAG_HEADER_LENGTH + 1] << 16) & 0xff0000)
		| (((int) tagData[TAG_HEADER_LENGTH + 2] << 8) & 0xff00)
		| ((int) tagData[TAG_HEADER_LENGTH + 3] & 0xff);

	int current_offset = TAG_HEADER_LENGTH;

	while (current_offset + 4 + slice_len + 4 <= dataLen) {
		int next_slice_len = (((int) tagData[current_offset + 4 + slice_len]
					<< 24) & 0xff000000)
			| (((int) tagData[current_offset + 4 + slice_len + 1] << 16)
					& 0xff0000)
			| (((int) tagData[current_offset + 4 + slice_len + 2] << 8)
					& 0xff00)
			| ((int) tagData[current_offset + 4 + slice_len + 3] & 0xff);

		tagData[current_offset] = 0;
		tagData[current_offset + 1] = 0;
		tagData[current_offset + 2] = 0;
		tagData[current_offset + 3] = 1;

		current_offset += 4 + slice_len;
		slice_len = next_slice_len;
	}

	return tagData;
}

int decodeVideo(unsigned char *buffer, int buf_lenth)
{
	unsigned char *buf = (unsigned char *)calloc(1, 102400);
	memcpy(buf, buffer, buf_lenth);
	if (buf[11] == 0x17) {
		if (buf[12] == 0x00) {
			int sps_offset = 24 - 3;
			if (((int) buf[sps_offset] & 0x0f) != 1) {
				//System.out.println("sps count not 1");
			}

			int sps_len = (((int) buf[sps_offset + 1] << 8) & 0xff00)
				| ((int) buf[sps_offset + 2] & 0xff);

			memcpy(video_decodec->sps_data, buf + sps_offset + 3, sps_len);

			int pps_offset = sps_offset + 3 + sps_len;
			if (((int) buf[pps_offset] & 0x0f) != 1) {
				//System.out.println("pps count not 1");
			}
			int pps_len = (((int) buf[pps_offset + 1] << 8) & 0xff00)
				| ((int) buf[pps_offset + 2] & 0xff);

			memcpy(video_decodec->pps_data, buf + pps_offset + 3, pps_len);

			int data_offset = 0;
			memcpy(video_decodec->SPS_PPS_buf, video_decodec->head4, 4);

			data_offset += 4;
			memcpy(video_decodec->SPS_PPS_buf + data_offset,
					video_decodec->sps_data, sps_len);

			data_offset += sps_len;
			memcpy(video_decodec->SPS_PPS_buf + data_offset,
					video_decodec->head4, 4);

			data_offset += 4;
			memcpy(video_decodec->SPS_PPS_buf + data_offset,
					video_decodec->pps_data, pps_len);

			video_decodec->has_SPS = 1;
			video_decodec->SPS_PPS_length = 4 + sps_len + 4 + pps_len;
			free(buf);
			return 0;
		} else {
			// System.out.println("get a I frame");
			buf = proc_tagData(buf, buf_lenth);
			if (buf == NULL) {
				// todo error
				//System.out.println("error");
			}
			if (video_decodec->has_SPS) {
				video_decodec->frame_buffer_lenth =
					video_decodec->SPS_PPS_length + buf_lenth
					- TAG_HEADER_LENGTH - 4;
				memcpy(video_decodec->frame_buffer, video_decodec->SPS_PPS_buf,
						video_decodec->SPS_PPS_length);

				memcpy(video_decodec->frame_buffer
						+ video_decodec->SPS_PPS_length,
						buf + TAG_DATA_OFFSET,
						buf_lenth - TAG_HEADER_LENGTH - 4);

				video_decodec->has_SPS = 0;
			} else {
				video_decodec->frame_buffer_lenth = buf_lenth
					- TAG_HEADER_LENGTH - 4;
				memcpy(video_decodec->frame_buffer, buf + TAG_DATA_OFFSET + 1,
						buf_lenth - TAG_HEADER_LENGTH - 4);
			}

		}
	} else {
		proc_tagData(buf, buf_lenth);
		if (buf == NULL) {
			// todo error
			//System.out.println("error");
		}

		video_decodec->frame_buffer_lenth = buf_lenth - TAG_DATA_OFFSET - 4 - 1;
		memcpy(video_decodec->frame_buffer, buf + TAG_DATA_OFFSET + 1,
				buf_lenth - TAG_DATA_OFFSET - 4 - 1);
	}

	h264_decoder(video_decodec->frame_buffer, video_decodec->frame_buffer_lenth);
	free(buf);
	return 0;	
}

static int count = 0;
int h264_decoder(unsigned char *buf, int size)
{
	AVPacket av_packet;
	av_init_packet(&av_packet);

	av_packet.data = buf;
	av_packet.size = size;

	int frame_finished = 0;
	if (!avcodec_is_open(m_AvCodecContext_video)) {
		printf("JJ\n");
		exit(1);
		return -1;
	}

	int av_return = avcodec_decode_video2(m_AvCodecContext_video,
			m_AvFrame_video, &frame_finished, &av_packet);
	if (av_return <= 0 || !frame_finished) {	
		printf("Decoder failed\n");
		return -1;
	}
	printf("Decoder Succeed\n");

	int ret = sws_scale(m_AvConvertContext, (const uint8_t **const)m_AvFrame_video->data,
			m_AvFrame_video->linesize, 0, 360, m_AvFrameRGB->data,
			m_AvFrameRGB->linesize);

	printf("%d\n", ret);
	//SaveFrame(m_AvFrameRGB, 480, 360, count++);
	SaveFrameToBMP(++count, 480, 360, 32, m_AvFrameRGB);
	return 0;
}
















