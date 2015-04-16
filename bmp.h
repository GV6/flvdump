
#ifndef __BMP_H__
#define __BMP_H__

#include <stdio.h>
#include <libswscale/swscale.h>
#include <arpa/inet.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

int SaveFrameToBMP(int count, int nWidth, int nHeight, int nBitCount, AVFrame *pFrame);

#endif
