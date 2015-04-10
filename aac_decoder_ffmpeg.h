#ifndef __AAC_DECODER_FFMPEG__
#define __AAC_DECODER_FFMPEG__

#include <stdio.h>

int aac_decoder(unsigned char *buf, int size);

int aac_decoder_init();

#endif
