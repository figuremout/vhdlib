all: compile

prepare:
	bash -c "if [[ ! -d ./bin ]]; then mkdir ./bin; fi"

compile: prepare
	gcc vhd.c -o ./bin/vhd
	chmod a+x ./bin/vhd

clean:
	rm -rf ./bin

.PHONY: all prepare clean
