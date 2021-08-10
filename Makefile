all: prepare
	gcc vhd.c -o ./bin/vhd
	chmod a+x ./bin/vhd

prepare:
	$(shell if [[ ! -d ./bin ]]; then mkdir ./bin; fi)

clean:
	rm -rf ./bin

.PHONY: all prepare clean