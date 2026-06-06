# ABOUTME: Native (host cc) unit tests for MacCode's pure-C SE logic.
# ABOUTME: Compiles wire.c / transcript.c off-device with munit; no Retro68 needed.
CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -I../src -I.
MUNIT = munit.c

all: test_wire test_transcript

test_wire: test_wire.c ../src/wire.c $(MUNIT)
	$(CC) $(CFLAGS) -o $@ $^

test_transcript: test_transcript.c ../src/transcript.c $(MUNIT)
	$(CC) $(CFLAGS) -o $@ $^

check: all
	./test_wire && ./test_transcript

clean:
	rm -f test_wire test_transcript

.PHONY: all check clean
