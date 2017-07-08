# b04902053 鄭淵仁 #

all: server.o file_reader.o
	gcc server.o -o server
	gcc file_reader.o -o file_reader
%.o: %.c
	gcc -std=c11 -Wall $< -c

.PHONY: clean
clean:
	@rm -rf *.o

