
#include "aac_decoder_ffmpeg.h"

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>


int aac_decoder_init()
{
//	AVCodec *m_AvCodec_audio = NULL;

//	avcodec_register_all();

	
	
	return 0;
}

int aac_decoder(unsigned char *buf, int size)
{
	printf("hello aac decoder\n");
	return 0;
}


