#include "bmp.h"
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>

typedef struct tagBITMAPFILEHEADER
{
	unsigned char buf[14];
	/*unsigned short bfType;      //2 位图文件的类型，必须为“BM”
	unsigned int bfSize;       //4 位图文件的大小，以字节为单位
	unsigned short bfReserved1; //2 位图文件保留字，必须为0
	unsigned short bfReserved2; //2 位图文件保留字，必须为0
	unsigned int bfOffBits;    //4 位图数据的起始位置，以相对于位图文件头的偏移量表示，以字节为单位
*/
} BITMAPFILEHEADER;           //该结构占据14个字节。

typedef struct tagBITMAPINFOHEADER{
	unsigned int biSize;       //4 本结构所占用字节数
	int biWidth;               //4 位图的宽度，以像素为单位
	int biHeight;              //4 位图的高度，以像素为单位
	unsigned short biPlanes;    //2 目标设备的平面数不清，必须为1
	unsigned short biBitCount;  //2 每个像素所需的位数，必须是1(双色), 4(16色)，8(256色)或24(真彩色)之一
	unsigned int biCompression;//4 位图压缩类型，必须是 0(不压缩),1(BI_RLE8压缩类型)或2(BI_RLE4压缩类型)之一
	unsigned int biSizeImage;  //4 位图的大小，以字节为单位
	int biXPelsPerMeter;       //4 位图水平分辨率，每米像素数
	int biYPelsPerMeter;       //4 位图垂直分辨率，每米像素数
	unsigned int biClrUsed;    //4 位图实际使用的颜色表中的颜色数
	unsigned int biClrImportant;//4 位图显示过程中重要的颜色数
} BITMAPINFOHEADER;           //该结构占据40个字节。

int SaveFrameToBMP(int count, int nWidth, int nHeight, int nBitCount, AVFrame *pFrame)
{
	FILE *fp;
	char szFilename[32];
	int y;

	// Open file
	sprintf(szFilename, "gv%d.bmp", count);
	fp = fopen(szFilename, "wb");
	if (fp == NULL)
		return -1;

	BITMAPFILEHEADER bmpheader;  
	BITMAPINFOHEADER bmpinfo;  
	memset(&bmpheader, 0, sizeof(BITMAPFILEHEADER));  
	memset(&bmpinfo, 0, sizeof(BITMAPINFOHEADER));

	// set BITMAPFILEHEADER value  
	bmpheader.buf[0] = 'B';
	bmpheader.buf[1] = 'M';
	int a = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.buf[10] = (a & 0x000000FF);
	bmpheader.buf[11] = (a & 0x0000FF00)>>8;
	bmpheader.buf[12] = (a & 0x00FF0000)>>16;
	bmpheader.buf[13] = (a & 0xFF000000)>>24;

	int b = a + nWidth * nHeight * nBitCount / 8;
	bmpheader.buf[2] = (b & 0x000000FF);
        bmpheader.buf[3] = (b & 0x0000FF00)>>8;
        bmpheader.buf[4] = (b & 0x00FF0000)>>16;
        bmpheader.buf[5] = (b & 0xFF000000)>>24;

/*	bmpheader.bfType = ('M' << 8) | 'B';  
	bmpheader.bfReserved1 = 0;  
	bmpheader.bfReserved2 = 0;  
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
	bmpheader.bfSize = bmpheader.bfOffBits + nWidth * nHeight * nBitCount / 8;	
*/

	// set BITMAPINFOHEADER value  
	bmpinfo.biSize = (sizeof(BITMAPINFOHEADER));  
	bmpinfo.biWidth = (nWidth);  
	bmpinfo.biHeight =(0 - nHeight);  
	bmpinfo.biPlanes = (1);  
	bmpinfo.biBitCount = (nBitCount);  
	bmpinfo.biCompression = (0);  
	bmpinfo.biSizeImage = (0);  
	bmpinfo.biXPelsPerMeter = (100);  
	bmpinfo.biYPelsPerMeter = (100);  
	bmpinfo.biClrUsed = (0);  
	bmpinfo.biClrImportant = (0);

	fwrite(&bmpheader, sizeof(BITMAPFILEHEADER), 1, fp);  
	fwrite(&bmpinfo, sizeof(BITMAPINFOHEADER), 1, fp);  
	for(y = 0; y < nHeight; y++)  
	{  
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, nWidth * nBitCount / 8, fp);  
	} 	

	//fwrite(pFrame->data[0], nWidth * nHeight * nBitCount / 8, 1, fp);  
	fclose(fp);  

	return 0;
}

