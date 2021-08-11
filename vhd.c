/*
 * Author: Zhao Jiaming
 * Email: 1344584929@qq.com
 * Describtion: 
 *     This program relies on Virtual Hard Disk Image Format Specification:
 *     https://www.microsoft.com/en-us/download/details.aspx?id=23850
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/*
 * Authority-defined vals in footer
 * Notice: followings are all in big-endian byte order
 */
uint64_t DEFAULT_COOKIE = 0x78697463656e6f63; // conectix

uint32_t FEATURES_NO_FEATURES_ENABLED = 0x00000000;
uint32_t FEATURES_TEMPORARY = 0x01000000;
uint32_t FEATURES_RESERVED = 0x02000000;

uint32_t CREATOR_HOST_OS_Wi2k = 0x6b326957; // Windows
uint32_t CREATOR_HOST_OS_Mac = 0x2063614d; // Macintosh

uint32_t DISK_TYPE_NONE = 0x00000000;
uint32_t DISK_TYPE_RESERVED_DEPRECATED_0 = 0x01000000;
uint32_t DISK_TYPE_FIXED_HARD_DISK = 0x02000000;
uint32_t DISK_TYPE_DYNAMIC_HARD_DISK = 0x03000000;
uint32_t DISK_TYPE_DIFFERENCING_HARD_DISK = 0x04000000;
uint32_t DISK_TYPE_RESERVED_DEPRECATED_1 = 0x05000000;
uint32_t DISK_TYPE_RESERVED_DEPRECATED_2 = 0x06000000;

uint32_t DEFAULT_FILE_FORMAT_VERSION = 0x00000100; // version 1.0

uint64_t FIXED_HARD_DISK_DATA_OFFSET = 0xffffffffffffffff; // this field is useless for fixed disk 

uint32_t CREATOR_APPLICATION = 0x006d6a7a; // zjm
uint32_t CREATOR_VERSION = 0x00000100; // version 1.0 

uint8_t SAVED_STATE_YES = 0x01;
uint8_t SAVED_STATE_NO = 0x00;

/*
 * Footer bits struct
 * footer should be 512 Bytes in total, struct Footer has 85 Bytes, should reserve 427 Bytes zeros after this struct
 * Notice:
 *     - struct must set byte align to keep its size exactly, otherwise GCC will fill in extra bytes
 */
typedef struct {
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectorsPerTrack;
}__attribute__ ((packed)) Disk_geometry;

typedef struct {
    uint64_t cookie;
    uint32_t features; 
    uint32_t file_format_version;
    uint64_t data_offset;
    uint32_t time_stamp; // This is the number of seconds since January 1, 2000 12:00:00 AM in UTC/GMT
    uint32_t creator_application;
    uint32_t creator_version;
    uint32_t creator_host_os;
    uint64_t original_size;
    uint64_t current_size;
    Disk_geometry disk_geometry;
    uint32_t disk_type;
    uint32_t checksum;
    uint64_t unique_id_0;
    uint64_t unique_id_1;
    uint8_t saved_state;
}__attribute__ ((packed)) Footer;

 
// pre-declare functions
void createFixedDisk(char* filepath, uint32_t len_bytes);
Footer readFooter(char* filepath);
void printFooter(Footer* footer);
Disk_geometry calCHS(uint32_t totalSectors);
void fillInChecksum(Footer* footer);
void hex2String(uint64_t hex, char* str, int len_bytes);
uint64_t switchByteOrder(uint64_t origin, uint8_t valid_len);
uint16_t* getVersion(uint32_t version_le);
void writeFixedDiskByLBA(char *binfile, char *vhdfile, uint32_t LBA);
uint32_t parseSize(char *sizeStr);
void printFixedDiskByLBA(char *vhdfile, uint32_t LBA);

/*
 * Global variables
 */
uint32_t VHD_MAX_BYTES = 0xffffffff; // 4 GB
uint32_t VHD_MIN_BYTES = 0x00400000; // 4 MB
uint16_t footerSize = sizeof(Footer);
uint32_t secondsGap = 946699200; // 1970.01.01 00:00:00 - 2000.01.01 12:00:00 946699200s

// In theory, unique_id should be dynamic, which is assured by user's VM to be unique
// this default value is self-defined, user can change it as long as it's unique
uint64_t DEFAULT_DISK_UNIQUE_ID_0 = 0xffffffffffffffff;
uint64_t DEFAULT_DISK_UNIQUE_ID_1 = 0xffffffffffffffff;


/*
 * Main
 */
int main(int argc,char *argv[]){
    int ch;
    opterr = 0;

    uint16_t *creator_versions;
    int r_count=0, w_count=0, b_count=0;
    uint32_t r_args[argc], w_args[argc], s_arg=0;
    char *b_args[argc], *d_arg=NULL;

    while((ch = getopt(argc, argv, "vhr:w:d:b:s:")) != -1) {
        switch(ch) {
            case 'v': creator_versions = getVersion(CREATOR_VERSION);
                      printf("VHD %u.%u - A tool to r/w .vhd\n", creator_versions[0], creator_versions[1]);
                      break;
            case 'h': 
                      creator_versions = getVersion(CREATOR_VERSION);
                      printf("VHD %u.%u - A tool to r/w .vhd\n", creator_versions[0], creator_versions[1]);
                      // usage
                      printf("\nusage: vhd -d [vhdfile]                       show vhdfile info");
                      printf("\n   or: vhd -d [vhdfile] -w[LBA] -b [binfile]  write bin into specified LBA");
                      printf("\n   or: vhd -d [vhdfile] -r[LBA]               output specified LBA");
                      printf("\n   or: vhd -d [vhdfile] -s[size]              create vhdfile");

                      printf("\n\nArguments:\n");
                      printf("   -h\tshow help\n");
                      printf("   -v\tshow version\n");
                      printf("   -s\tspecify create size (B, K/KB, M/MB, G/GB)\n");
                      printf("   -r\tspecify LBA to read\n");
                      printf("   -w\tspecify LBA to write\n");
                      printf("   -d\tspecify vhdfile\n");
                      printf("   -b\tspecify binfile\n");
                      break;
            case 'd': 
                      if (d_arg) {
                          printf("too many option -%c\n");
                          exit(1);
                      } else {
                        d_arg = optarg;
                      }
                      break;
            case 's': 
                      s_arg = parseSize(optarg);
                      break;
            case 'r': r_args[r_count] = atoi(optarg);
                      r_count++;
                      break;
            case 'w': w_args[w_count] = atoi(optarg);
                      w_count++;
                      break;
            case 'b': b_args[b_count] = optarg;
                      b_count++;
                      break;
            default: printf("undefined option: -%c\n", optopt);
        }
    }

    // create vhdfile
    if (s_arg > 0 && d_arg) {
        printf("creating vhd %s\n", d_arg);
        createFixedDisk(d_arg, s_arg);
        printf("Create vhd %s DONE\n", d_arg);
    }

    // print vhdfile's footer
    if (!d_arg) {
        printf("Not specify vhdfile\n");
    } else if (!s_arg && w_count <= 0 && r_count <= 0) {
        printf("*************************\n");
        Footer footer = readFooter(d_arg);
        printf("*************************\n");
        printFooter(&footer);
        printf("************************\n");
    }

    // write bin into vhdfile
    if (w_count > 0 && w_count == b_count && d_arg) {
        for (int i = 0; i < w_count; i++) {
            printf("writing BIN %s into LBA %u of VHD %s\n", b_args[i], w_args[i], d_arg);
            writeFixedDiskByLBA(b_args[i], d_arg, w_args[i]);
            printf("writing BIN %s into LBA %u of VHD %s DONE\n", b_args[i], w_args[i], d_arg);
        }
    }

    // read LBA
    if (r_count > 0 && d_arg) {
        for (int i = 0; i < r_count; i++) {
            printf("************************\n");
            printf("* LBA %u of VHD %s\n", r_args[i], d_arg);
            printf("************************\n");
            printFixedDiskByLBA(d_arg, r_args[i]);
            printf("************************\n");
        }
    }
    return 0;
}

/* 
 * Description:
 *     parse size string into byte num, eg. "1MB" => 1048576
 */
uint32_t parseSize(char *sizeStr) {
    uint32_t sizeNum = 0;
    char sizeUnit = 'B';
    for (int i = 0; sizeStr[i] != '\0'; i++) {
        if (sizeStr[i] >= '0' && sizeStr[i] <= '9') {
            sizeNum = (sizeStr[i] - '0') + sizeNum * 10;
        } else {
            sizeUnit = sizeStr[i];
            break;
        }
    }
    if (sizeUnit == 'B') {
        sizeNum = sizeNum;
    } else if (sizeUnit == 'K') {
        sizeNum *= 1024;
    } else if (sizeUnit == 'M') {
        sizeNum *= 1024 * 1024;
    } else if (sizeUnit == 'G') {
        sizeNum = 1024 * 1024 * 1024;
    } else {
        printf("size %s illegal\n", sizeStr);
        exit(1);
    }
    return sizeNum;
}


/*
 * Description:
 *     read file footer into struct Footer
 */
Footer readFooter(char *filepath) {
    // check if file exists
    if (access(filepath, F_OK) == -1) {
        printf("File %s not exixts\n", filepath);
        exit(1);
    }

    // check file size
    struct stat statbuf;
    stat(filepath, &statbuf);
    int filesize = statbuf.st_size;
    if (filesize < (VHD_MIN_BYTES+512) || filesize > VHD_MAX_BYTES) {
        printf("File %s size %d Bytes, not in 4MB - 4GB\n", filepath, filesize);
        exit(1);
    }

    printf("* File: %s\n", filepath);
    printf("* Size: %d Bytes\n", filesize);
    FILE *fp = fopen(filepath,"rb+");
    // move fp to footer
    fseek(fp, -512, SEEK_END);
    Footer footer;
    fread(&footer, sizeof(footer), 1, fp);
    fclose(fp);
    return footer;
}

/*
 * Description:
 *     print struct Footer info
 */
void printFooter(Footer *footer) {
    // cookie
    char cookie_str[sizeof(footer->cookie)+1];
    hex2String(footer->cookie, cookie_str, sizeof(cookie_str));
    printf("cookie: %s\n", cookie_str);

    // features
    printf("features: ");
    if (footer->features == FEATURES_NO_FEATURES_ENABLED) {
        printf("No features enabled\n");
    } else if (footer->features == FEATURES_TEMPORARY) {
        printf("Temporary\n");
    } else if (footer->features == FEATURES_RESERVED) {
        printf("Reserved\n");
    } else {
        printf("UNDEFINED\n");
    }

    // file format version
    uint16_t *format_versions = getVersion(footer->file_format_version);
    printf("file format version: %u.%u\n", format_versions[0], format_versions[1]);

    // data offset
    printf("data offset: 0x%016lx\n", footer->data_offset);

    // time stamp
    uint32_t unix_timestamp = switchByteOrder(footer->time_stamp, 32) + secondsGap; // get seconds start from 1970
    time_t timer = unix_timestamp;
    struct tm *info = localtime(&timer);
    char buffer[80];
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
    printf("time stamp: 0x%08x (%s)\n", unix_timestamp, buffer);

    // creator application
    char app_str[sizeof(footer->creator_application)+1];
    hex2String(footer->creator_application, app_str, sizeof(app_str));
    printf("creator application: %s\n", app_str);

    // creator version
    uint16_t *creator_versions = getVersion(footer->creator_version);
    printf("creator version: %u.%u\n", creator_versions[0], creator_versions[1]);

    // creator host os
    printf("creator host os: ");
    if (footer->creator_host_os == CREATOR_HOST_OS_Wi2k) {
        printf("Windows\n");
    } else if (footer->features == CREATOR_HOST_OS_Mac) {
        printf("Macintosh\n");
    } else {
        printf("UNDEFINED\n");
    }

    // original size
    printf("original size: ");
    uint64_t original_B = switchByteOrder(footer->original_size, 64);
    uint64_t original_MB, original_GB;
    original_MB = original_B / (1024*1024);
    original_GB = original_MB / 1024;
    if (original_GB > 0) {
        printf("%lu GB\n", original_GB);
    } else if (original_MB > 0){
        printf("%lu MB\n", original_MB);
    } else {
        printf("%lu B\n", original_B);
    }

    // current size
    printf("current size: ");
    uint64_t current_B = switchByteOrder(footer->current_size, 64);
    uint64_t current_MB, current_GB;
    current_MB = current_B / (1024*1024);
    current_GB = current_MB / 1024;
    if (current_GB > 0) {
        printf("%lu GB\n", current_GB);
    } else if (current_MB > 0){
        printf("%lu MB\n", current_MB);
    } else {
        printf("%lu B\n", current_B);
    }

    // disk geometry
    Disk_geometry disk_geometry = footer->disk_geometry;
    printf("Cylinders: %lu\n", switchByteOrder(disk_geometry.cylinders, 16));
    printf("Heads: %d\n", disk_geometry.heads);
    printf("SectorsPerTrack: %d\n", disk_geometry.sectorsPerTrack);

    // disk type
    printf("disk type: ");
    uint32_t disk_type = footer->disk_type;
    if (disk_type == DISK_TYPE_NONE) {
        printf("%s\n", "None");
    } else if (disk_type == DISK_TYPE_RESERVED_DEPRECATED_0 || 
            disk_type == DISK_TYPE_RESERVED_DEPRECATED_1 || 
            disk_type == DISK_TYPE_RESERVED_DEPRECATED_2){
        printf("%s\n", "Reserved (deprecated)");
    } else if (disk_type == DISK_TYPE_FIXED_HARD_DISK){
        printf("%s\n", "Fixed hard disk");
    } else if (disk_type == DISK_TYPE_DYNAMIC_HARD_DISK) {
        printf("%s\n", "Dynamic hard disk");
    } else if (disk_type == DISK_TYPE_DIFFERENCING_HARD_DISK) {
        printf("%s\n", "Differencing hard disk");
    } else {
        printf("%s\n", "UNDEFINED");
    }

    // checksum
    printf("checksum: 0x%08lx\n", switchByteOrder(footer->checksum, 32));

    // unique id
    printf("unique id: 0x%016lx%016lx\n", switchByteOrder(footer->unique_id_1, 64), 
            switchByteOrder(footer->unique_id_0, 64));

    // saved state
    printf("saved state: ");
    uint8_t saved_state = footer->saved_state;
    if (saved_state == SAVED_STATE_YES) {
        printf("%s\n", "In saved state");
    } else if (saved_state == SAVED_STATE_NO) {
        printf("%s\n", "Not in saved state");
    } else {
        printf("%s\n", "UNDEFINED");
    }
}

/* 
 * Description:
 *     covert ascii hex number to string
 *
 * Params:
 *     - hex: length shorter than 64 bit
 *     - str: char array, length should be char nums + 1 ('\0' need to be append to this array)
 *     - len_bytes: sizeof(str), number of bytes in str including '\0'
 *
 * Notice:
 *     - cannot sizeof(str) in this function to get len_bytes, because sizeof() return len of ptr, not len of arr
 *     - some special byte may cause result str cannot be print by %s. eg.0x00 corresponding to char '\0'
 *     - visual char range: 0x20 - 0x7e
 *     - the index of last byte in str is actually len_bytes-1
 */
void hex2String(uint64_t hex, char *str, int len_bytes) {
    uint8_t mask = 0xff;
    for (int i = 0;  i < (len_bytes-1); i++) {
        uint8_t code = (hex >> (i*8)) & mask;
        if (code >= 0x20 && code <= 0x7e) {
            str[i] = toascii(code);
        } else {
            // replace invisual char with '.'
            str[i] = '.';
        }
    }
    str[len_bytes-1] = '\0';
}

/*
 * Description:
 *     read bytes from input binfile, output into specified LBA of vhdfile
 */
void writeFixedDiskByLBA(char *binfile, char *vhdfile, uint32_t LBA) {
    // check if file exists
    if (access(binfile, F_OK) == -1) {
        printf("File %s not exixts\n", binfile);
        exit(1);
    }
    if (access(vhdfile, F_OK) == -1) {
        printf("File %s not exixts\n", vhdfile);
        exit(1);
    }

    // open input file
    FILE *input_fp = fopen(binfile,"rb+");
    if(input_fp == NULL){
        printf("Cannot open file %s\n", binfile);
        exit(1);
    }
    // move input_fp to begin
    fseek(input_fp, 0, SEEK_SET);

    // open output file
    FILE *output_fp = fopen(vhdfile,"r+b");
    if(output_fp == NULL){
        printf("Cannot open file %s\n", vhdfile);
        exit(1);
    }
    // move output_fp to LBA
    fseek(output_fp, LBA*512, SEEK_SET);

    // transfer
    char buffer[512];
    while (!feof(input_fp)) {
        fread(&buffer, sizeof(buffer), 1, input_fp);
        fwrite(&buffer, sizeof(buffer), 1, output_fp);
    }

    fclose(input_fp);
    fclose(output_fp);
}

/* 
 * Description:
 *     print specified LBA in hex and ascii, similar to xxd
 */
void printFixedDiskByLBA(char *vhdfile, uint32_t LBA) {
    // check if file exists
    if (access(vhdfile, F_OK) == -1) {
        printf("File %s not exixts\n", vhdfile);
        exit(1);
    }

    // open file
    FILE *fp = fopen(vhdfile,"rb+");
    if(fp == NULL){
        printf("Cannot open file %s\n", vhdfile);
        exit(1);
    }

    // move fp to LBA
    fseek(fp, LBA*512, SEEK_SET);

    // read
    uint8_t buffer[512];
    if (!feof(fp)) {
        fread(&buffer, sizeof(buffer), 1, fp);
    }
    fclose(fp);

    uint32_t byteOffset = 0;
    uint64_t lineSum_0 = 0, lineSum_1 = 0;
    char asciiStr_0[9], asciiStr_1[9];
    for (uint16_t i = 0; i < 512; i+=2) {
        if (i % 16 == 0) {
            byteOffset = i + LBA * 512;
            printf("%08x: ", byteOffset);
        }
        printf("%02x%02x ", buffer[i], buffer[i+1]);
        if (i % 16 < 8) {
            // 0 - 7 byte inline
            lineSum_0 += (((uint64_t)buffer[i]) << (i*8));
            lineSum_0 += (((uint64_t)buffer[i+1]) << ((i+1)*8));
        } else {
            // 8 - 15 byte inline
            lineSum_1 += (((uint64_t)buffer[i]) << ((i-8)*8));
            lineSum_1 += (((uint64_t)buffer[i+1]) << ((i-8+1)*8));
        }
        if ((i+2) % 16 == 0) {
            hex2String(lineSum_0, asciiStr_0, sizeof(asciiStr_0));
            hex2String(lineSum_1, asciiStr_1, sizeof(asciiStr_1));
            printf(" %s%s\n", asciiStr_0, asciiStr_1);
            lineSum_0 = 0;
            lineSum_1 = 0;
        }
    }
}

/*
 * Description:
 *     create specified size of new vhdfile
 */
void createFixedDisk(char *filepath, uint32_t len_bytes) {
    // check if file already exists
    if (access(filepath, F_OK) != -1) {
        printf("File %s already exixts\n", filepath);
        exit(1);
    }

    // check file size
    if (len_bytes < VHD_MIN_BYTES|| len_bytes > VHD_MAX_BYTES) {
        printf("Should specify size in 4MB - 4GB\n");
        exit(1);
    }

    // write zero bytes to specified len
    FILE *fp = fopen(filepath,"wb");
    if(fp == NULL){
        printf("Cannot open file %s\n", filepath);
        exit(1);
    }
    int fd = fileno(fp);
    ftruncate(fd, len_bytes);

    // move fp to end
    fseek(fp, 0, SEEK_END);

    // add footer to end
    // 1970.01.01 00:00:00 - now seconds
    time_t seconds = time(NULL);
    uint32_t time_stamp = seconds - secondsGap;
    uint64_t original_size = len_bytes;
    uint64_t current_size = original_size;
    uint32_t totalSectors = original_size / 512;

    Disk_geometry disk_geometry = calCHS(totalSectors);

    Footer footer = { 
        .cookie = DEFAULT_COOKIE,
        .features = FEATURES_RESERVED,
        .file_format_version = DEFAULT_FILE_FORMAT_VERSION,
        .data_offset = FIXED_HARD_DISK_DATA_OFFSET,
        .time_stamp = switchByteOrder(time_stamp, 32),
        .creator_application = CREATOR_APPLICATION,
        .creator_version = CREATOR_VERSION,
        .creator_host_os = CREATOR_HOST_OS_Wi2k,
        .original_size = switchByteOrder(original_size, 64),
        .current_size = switchByteOrder(current_size, 64),
        .disk_geometry = disk_geometry,
        .disk_type = DISK_TYPE_FIXED_HARD_DISK,
        .checksum = 0, // temp value
        .unique_id_0 = DEFAULT_DISK_UNIQUE_ID_0,
        .unique_id_1 = (uint64_t) time_stamp,
        .saved_state = SAVED_STATE_NO
    };

    // cal checksum and fill it into footer
    fillInChecksum(&footer);
 
    // append footer to file
    fwrite(&footer, footerSize , 1, fp);

    // append zero bytes to make footer meet 512 Bytes
    int rest_len = 512 - footerSize;
    char buffer[rest_len];
    memset(buffer, 0x00, rest_len);
    fwrite(buffer, rest_len, 1, fp);
    fclose(fp);
}


/*
 * Description:
 *     Authority-defined algorithm to get CHS from size
 */
Disk_geometry calCHS(uint32_t totalSectors) {
    uint32_t cylinderTimesHeads;
    uint16_t cylinders;
    uint8_t heads, sectorsPerTrack;

    if (totalSectors > 65535 * 16 * 255) {
        totalSectors = 65535 * 16 * 255; // disk max size = 2^28 * 512 Byte = 128 GB
    }

    if (totalSectors >= 65535 * 16 * 63) { // when disk size >= 32 GB
        sectorsPerTrack = 255;
        heads = 16;
        cylinderTimesHeads = totalSectors / sectorsPerTrack;
    } else {
        sectorsPerTrack = 17;
        cylinderTimesHeads = totalSectors / sectorsPerTrack;

        heads = (cylinderTimesHeads + 1023) / 1024;

        if (heads < 4) {
            heads = 4;
        }

        if (cylinderTimesHeads >= (heads * 1024) || heads > 16) {
            sectorsPerTrack = 31;
            heads = 16;
            cylinderTimesHeads = totalSectors / sectorsPerTrack;
        }

        if (cylinderTimesHeads >= (heads * 1024)) {
            sectorsPerTrack = 63;
            heads = 16;
            cylinderTimesHeads = totalSectors / sectorsPerTrack;
        }
    }
    cylinders = cylinderTimesHeads / heads;

    Disk_geometry disk_geometry = {
        .cylinders = switchByteOrder(cylinders, 16),
        .heads = heads,
        .sectorsPerTrack = sectorsPerTrack
    };
    return disk_geometry;
}

/*
 * Description:
 *     Athority-defined algorithm to get checksum field
 */
void fillInChecksum(Footer *footer) {
    uint32_t checksum = 0;

    uint8_t *p = (uint8_t*)footer;
    footer->checksum = 0;
    for (uint16_t counter = 0 ; counter < footerSize ; counter++) {
        checksum += *p;
        p++;
    }
    footer->checksum = switchByteOrder(~checksum, 32);
}


/* 
 * Description:
 *     little endian <=> big endian
 *
 * Params:
 *     - origin: unsigned int, length <= 64
 *     - valid_len: origin's length 
 *
 * Return:
 *     - unsigned int after switch
 *
 * Example:
 *     printf("%lu\n", switchByteOrder(0x1234, 16));
 */
uint64_t switchByteOrder(uint64_t origin, uint8_t valid_len) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < valid_len; i+=8) {
        result += ((origin >> i) & 0xff) << (valid_len-8-i);
    }
    return result;
}

/*
 * Description:
 *     seperate major version and minor version
 */
uint16_t *getVersion(uint32_t version_le) {
    static uint16_t versions[2];
    uint32_t version_be = switchByteOrder(version_le, 32);
    versions[0] = version_be >> 16; // major version
    versions[1] = version_be & 0x0000ffff; // minor version
    return versions;
}
