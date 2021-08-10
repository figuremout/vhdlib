all:
	gcc vhd.c -o vhd
	chmod a+x vhd

clean:
	rm -f vhd
