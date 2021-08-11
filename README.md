# Virtual Hard Disk (VHD)
> According to [MicroSoft VHD Specifications](https://www.microsoft.com/en-us/download/details.aspx?id=23850)

## Usage
1. Compile
```
make
```
2. Check help
```
./bin/vhd -h
```

## Advantage
- Can specify LBA to check VHD content in hex (like `xxd`), but much faster than `xxd` especially when VHD is huge.
- Can easily check VHD footer in fast speed.
- Can easily write binary files into specified LBAs of a VHD.
- Can easily create a specified size of VHD.
