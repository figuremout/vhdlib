TODO
- tools like mkvhd, vhdinfo / readvhd
- behave like readelf -h
- behave like readvhd -b test.vhd | xxd
- add comment for macro
- make vhdlib install to dir like /usr/include/, then change `#include ""` to `#include <>`
- delete all printf from vhdlib, them should be kept in the program not library
- see if need static function

# Virtual Hard Disk (VHD)
> A tool to r/w VHD on Linux, according to [MicroSoft VHD Specifications](https://www.microsoft.com/en-us/download/details.aspx?id=23850)

## Usage
1. Compile
```
make
```
2. Check help `./bin/vhder -h`
```
usage: vhder -d [vhdfile]                       show vhdfile footer info
   or: vhder -d [vhdfile] -w[LBA] -b [binfile]  write bin into specified LBA
   or: vhder -d [vhdfile] -r[LBA]               output specified LBA
   or: vhder -d [vhdfile] -s[size]              create vhdfile
```

## Advantage
- Specify LBA to check VHD content in hex (like `xxd`), but much faster than `xxd` especially when VHD is huge.
- Easily check VHD footer in fast speed.
- Easily write binary files into specified LBAs of a VHD.
- Easily create a specified size of VHD (34KB - 4GB).
