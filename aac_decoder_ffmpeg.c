
#include "aac_decoder_ffmpeg.h"

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

#define ADTS_HEADER_SIZE 7

typedef struct PutBitContext {
	uint32_t bit_buf;
	int bit_left;
	uint8_t *buf, *buf_ptr, *buf_end;
	int size_in_bits;
} PutBitContext;

int aac_decoder_init()
{
	AVCodec *m_AvCodec_audio = NULL;
	AVCodecContext *m_AvCodecContext_audio = NULL;
	avcodec_register_all();

	m_AvCodec_audio = avcodec_find_decoder(CODEC_ID_AAC);
	if (!m_AvCodec_audio) {
		return -1;
	}

	m_AvCodecContext_audio = avcodec_alloc_context3(m_AvCodec_audio);
	if (!m_AvCodecContext_audio) {
		return -1;
	}


	//init

	if (avcodec_open2(m_AvCodecContext_audio, m_AvCodec_audio, NULL) < 0) {
		return -1;
	}

	return 0;
}

static void init_put_bits(PutBitContext *s, uint8_t *buffer,
		int buffer_size) {
	if (buffer_size < 0) {
		buffer_size = 0;
		buffer = NULL;
	}

	s->size_in_bits = 8 * buffer_size;
	s->buf = buffer;
	s->buf_end = s->buf + buffer_size;
	s->buf_ptr = s->buf;
	s->bit_left = 32;
	s->bit_buf = 0;
}

static void put_bits(PutBitContext *s, int n, unsigned int value) {
	unsigned int bit_buf;
	int bit_left;

	bit_buf = s->bit_buf;
	bit_left = s->bit_left;

	/* XXX: optimize */
#ifdef BITSTREAM_WRITER_LE
	bit_buf |= value << (32 - bit_left);
	if (n >= bit_left) {
		s->buf_ptr += 4;
		bit_buf = (bit_left == 32) ? 0 : value >> bit_left;
		bit_left += 32;
	}
	bit_left -= n;
#else
	if (n < bit_left) {
		bit_buf = (bit_buf << n) | value;
		bit_left -= n;
	} else {
		bit_buf <<= bit_left;
		bit_buf |= value >> (n - bit_left);
		s->buf_ptr += 4;
		bit_left += 32 - n;

		bit_buf = value;
	}
#endif

	s->bit_buf = bit_buf;
	s->bit_left = bit_left;
}

static void flush_put_bits(PutBitContext *s) {
#ifndef BITSTREAM_WRITER_LE
	if (s->bit_left < 32)
		s->bit_buf <<= s->bit_left;
#endif
	while (s->bit_left < 32) {
		/* XXX: should test end of buffer */
#ifdef BITSTREAM_WRITER_LE
		*s->buf_ptr++ = s->bit_buf;
		s->bit_buf >>= 8;
#else
		*s->buf_ptr++ = s->bit_buf >> 24;
		s->bit_buf <<= 8;
#endif
		s->bit_left += 8;
	}
	s->bit_left = 32;
	s->bit_buf = 0;
}

int ff_adts_write_frame_header(int objecttype, int sample_rate_index,
		int channel_conf, uint8_t *buf, int size) {
	PutBitContext pb;

	init_put_bits(&pb, buf, ADTS_HEADER_SIZE);

	/* adts_fixed_header */
	put_bits(&pb, 12, 0xfff); /* syncword */
	put_bits(&pb, 1, 0); /* ID */
	put_bits(&pb, 2, 0); /* layer */
	put_bits(&pb, 1, 1); /* protection_absent */
	put_bits(&pb, 2, objecttype); /* profile_objecttype */
	put_bits(&pb, 4, sample_rate_index);
	put_bits(&pb, 1, 0); /* private_bit */
	put_bits(&pb, 3, channel_conf); /* channel_configuration */
	put_bits(&pb, 1, 0); /* original_copy */
	put_bits(&pb, 1, 0); /* home */

	/* adts_variable_header */
	put_bits(&pb, 1, 0); /* copyright_identification_bit */
	put_bits(&pb, 1, 0); /* copyright_identification_start */
	put_bits(&pb, 13, size); /* aac_frame_length */
	put_bits(&pb, 11, 0x7ff); /* adts_buffer_fullness */
	put_bits(&pb, 2, 0); /* number_of_raw_data_blocks_in_frame */

	flush_put_bits(&pb);

	return 0;
}

int aac_decoder(unsigned char *buf, int size)
{
	printf("hello aac decoder\n");



	return 0;
}


