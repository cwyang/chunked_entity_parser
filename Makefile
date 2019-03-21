CC?=gcc
PROVE?=prove

all:

test: test-bin
	$(PROVE) -v ./test-bin

test-bin: chunked_parser.c picotest/picotest.c test.c
	$(CC) -Wall $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f test-bin

.PHONY: test
