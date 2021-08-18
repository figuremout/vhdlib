# Virtual Hard Disk (VHD)
> A tool to r/w VHD on Linux, according to [MicroSoft VHD Specifications](https://www.microsoft.com/en-us/download/details.aspx?id=23850)

## Usage
1. Compile
```
make
```
2. Check help `./bin/vhd -h`
```
usage: vhd -d [vhdfile]                       show vhdfile footer info
   or: vhd -d [vhdfile] -w[LBA] -b [binfile]  write bin into specified LBA
   or: vhd -d [vhdfile] -r[LBA]               output specified LBA
   or: vhd -d [vhdfile] -s[size]              create vhdfile
```

## Advantage
- Can specify LBA to check VHD content in hex (like `xxd`), but much faster than `xxd` especially when VHD is huge.
- Can easily check VHD footer in fast speed.
- Can easily write binary files into specified LBAs of a VHD.
- Can easily create a specified size of VHD (34KB - 4GB).
