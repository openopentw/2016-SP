# b04902053 鄭淵仁 #

all: hw4.o
	gcc -lpthread hw4.o -o hw4
%.o: %.c
	gcc -std=c11 -Wall $< -c

.PHONY: clean run
clean:
	@rm -rf *.o
run:
	./hw4 -data ../data -output ./submission.csv -tree 50 -thread 50
