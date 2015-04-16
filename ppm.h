
#ifndef __PPM_H__
#define __PPM_H__

#include <stdio.h>

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int count);

#endif
