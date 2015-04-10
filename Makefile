all: flvdump

flvdump: flvdump.c
	gcc -O3 -Wall -o flvdump flvdump.c aac_decoder_ffmpeg.c -lm -I/usr/local/include /usr/local/lib/libswresample.a /usr/local/lib/libavfilter.a /usr/local/lib/libswscale.a /usr/local/lib/libavutil.a /usr/local/lib/libavformat.a /usr/local/lib/libavcodec.a

clean:
	rm -f flvdump
