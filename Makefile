all: flvdump

flvdump: flvdump.c
	gcc -O3 -Wall -o flvdump flvdump.c aac_decoder_ffmpeg.c -lm -I/usr/local/include -L/usr/local/lib -lswresample -lavfilter -lswscale -lavutil -lavformat -lavcodec

clean:
	rm -f flvdump
