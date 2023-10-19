CC := gcc
CFLAGS := -Wall -O2 -std=gnu11 -DDEBUG=1
LDFLAGS := -luuid

.PHONY: all
all: compile

.PHONY: prepare
prepare:
	bash -c "if [[ ! -d ./bin ]]; then mkdir ./bin; fi"

.PHONY: compile
compile: prepare
	$(CC) vhder.c vhdlib.c -o ./bin/vhder $(CFLAGS) $(LDFLAGS)
	chmod a+x ./bin/vhder

.PHONY: clean
clean:
	rm -rf ./bin

.PHONY: install
install:
	#install vhdlib
