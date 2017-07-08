# b04902053 鄭淵仁 #

all: big_judge.o judge.o player.o
	gcc big_judge.o -o big_judge
	gcc judge.o -o judge
	gcc player.o -o player
%.o: %.c
	gcc $< -c

.PHONY: clean
clean:
	@rm -rf *.o

