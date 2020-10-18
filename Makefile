.DELETE_ON_ERROR:
.PHONY: all

all: debwarrior

debwarrior: debwarrior.c
	$(CC) -pthread -O2 -o $@ $< -lnotcurses

clean:
	rm -f debwarrior
