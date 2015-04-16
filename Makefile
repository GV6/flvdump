all: flvdump

flvdump: flvdump.c
	gcc -O3 -Wall -o flvdump flvdump.c ppm.c bmp.c h264_decoder_ffmpeg.c -lm -I/usr/local/include -L/usr/local/lib -lswresample -lavfilter -lswscale -lavutil -lavformat -lavcodec

clean:
	rm -f flvdump
