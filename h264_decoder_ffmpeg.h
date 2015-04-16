#ifndef __H264_DECODER_FFMPEG__
#define __H264_DECODER_FFMPEG__

#include <stdio.h>

int h264_decoder_init(int nImageWidth, int nImageHeight);

int h264_decoder(unsigned char *buf, int size);

int decodeVideo(unsigned char *buf, int buf_lenth);

#endif
