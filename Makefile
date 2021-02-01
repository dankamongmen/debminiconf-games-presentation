.DELETE_ON_ERROR:
.PHONY: all

all: debwarrior

debwarrior: debwarrior.c
	$(CC) -pthread -O2 -o $@ $< $(shell pkg-config --libs notcurses)

clean:
	rm -f debwarrior
