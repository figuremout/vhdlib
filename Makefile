CC := gcc
CFLAGS := -O2 -std=gnu99
LDFLAGS := -luuid

all: compile

prepare:
	bash -c "if [[ ! -d ./bin ]]; then mkdir ./bin; fi"

compile: prepare
	$(CC) vhd.c -o ./bin/vhd $(CFLAGS) $(LDFLAGS)
	chmod a+x ./bin/vhd

clean:
	rm -rf ./bin

.PHONY: all prepare clean
