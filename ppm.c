
#include "ppm.h"

void SaveFrame(AVFrame *pFrame, int width, int height, int count) {
	FILE *pFile;
	char szFilename[32];
	int y;

	// Open file
	sprintf(szFilename, "gv%d.ppm", count);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
	return;

	// Write header  three line
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
	fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}
