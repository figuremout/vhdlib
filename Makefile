CC := gcc
CFLAGS := -O2
LDFLAGS := -luuid

all: compile

prepare:
	bash -c "if [[ ! -d ./bin ]]; then mkdir ./bin; fi"

compile: prepare
	$(CC) $(CFLAGS) $(LDFLAGS) vhd.c -o ./bin/vhd
	chmod a+x ./bin/vhd

clean:
	rm -rf ./bin

.PHONY: all prepare clean
