#include "vhdlib.h"
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <byteswap.h>

static void write_block(void *buffer, int bufferSize, FILE *fp);
static void read_block(void *buffer, int bufferSize, FILE *fp);

/*
 * Global variables
 */
const size_t footer_size = sizeof(struct footer);

/*
 * Description:
 *     create specified size of new vhdfile
 */
void create_fixed_disk(const char *filepath, uint32_t len_bytes)
{
	/* check if file already exists */
	if (access(filepath, F_OK) != -1) {
		printf("File %s already exixts\n", filepath);
		exit(1);
	}

	/* check file size */
	if (len_bytes < VHD_MIN_BYTES || len_bytes > VHD_MAX_BYTES) {
		printf("Should specify size in 34KB - 4GB\n");
		exit(1);
	}

	/* write zero bytes to specified len */
	FILE *fp = fopen(filepath, "wb");
	if (fp == NULL) {
		printf("Cannot open file %s\n", filepath);
		exit(1);
	}
	int fd = fileno(fp);
	int ret = ftruncate(fd, len_bytes);
	if (ret != 0) {
		printf("Error occurs when creating file %s\n", filepath);
		exit(1);
	}

	/* move fp to end */
	fseek(fp, 0, SEEK_END);

	/* add footer to end */
	/* 1970.01.01 00:00:00 - now seconds */
	time_t seconds = time(NULL);
	uint32_t time_stamp = seconds - SECONDS_OFFSET;
	uint64_t original_size = len_bytes;
	uint64_t current_size = original_size;
	uint32_t totalSectors = original_size / 512;

	struct disk_geometry *disk_geometry = cal_CHS(totalSectors);

	struct footer *footer = &(struct footer){
		.cookie = DEFAULT_COOKIE,
		.features = FEATURES_RESERVED,
		.file_format_version = DEFAULT_FILE_FORMAT_VERSION,
		.data_offset = FIXED_HARD_DISK_DATA_OFFSET,
		.time_stamp = bswap_32(time_stamp),
		.creator_application = CREATOR_APPLICATION,
		.creator_version = CREATOR_VERSION,
		.creator_host_os = CREATOR_HOST_OS_Lnux,
		.original_size = bswap_64(original_size),
		.current_size = bswap_64(current_size),
		.disk_geometry = *disk_geometry,
		.disk_type = DISK_TYPE_FIXED_HARD_DISK,
		.checksum = 0, /* temp value */
		.uuid = { 0 }, /* temp value */
		.saved_state = SAVED_STATE_NO,
	};

	/* generate uuid */
	uuid_generate((uint8_t *)&footer->uuid);

	/* cal checksum and fill it into footer */
	fillin_checksum(footer);

	/* append footer to file */
	write_block(footer, footer_size, fp);
	/* append zero bytes to make footer meet 512 Bytes */
	int rest_len = 512 - footer_size;
	char buffer[rest_len];
	memset(buffer, 0x00, rest_len);
	write_block(&buffer, rest_len, fp);
	fclose(fp);
	/* print uuid */
	char uuid_str[37];
	uuid_unparse((uint8_t *)&footer->uuid, uuid_str);
	printf("New VHD uuid: %s\n", uuid_str);
}

/*
 * Description:
 *     print specified LBA in hex and ascii, similar to xxd
 */
void print_fixed_disk_by_LBA(const char *vhdfile, uint32_t LBA)
{
	/* check if file exists */
	if (access(vhdfile, F_OK) == -1) {
		printf("File %s not exixts\n", vhdfile);
		exit(1);
	}

	/* open file */
	FILE *fp = fopen(vhdfile, "rb");
	if (fp == NULL) {
		printf("Cannot open file %s\n", vhdfile);
		exit(1);
	}

	/* move fp to LBA */
	fseek(fp, LBA * 512, SEEK_SET);

	/* read */
	uint8_t buffer[512];
	read_block(&buffer, sizeof(buffer), fp);
	fclose(fp);

	uint32_t byteOffset = 0;
	uint64_t lineSum_0 = 0, lineSum_1 = 0;
	char asciiStr_0[9], asciiStr_1[9];
	for (uint16_t i = 0; i < 512; i += 2) {
		if (i % 16 == 0) {
			byteOffset = i + LBA * 512;
			printf("%08x: ", byteOffset);
		}
		printf("%02x%02x ", buffer[i], buffer[i + 1]);
		if (i % 16 < 8) {
			/* 0 - 7 byte inline */
			lineSum_0 += (((uint64_t)buffer[i]) << (i * 8));
			lineSum_0 +=
				(((uint64_t)buffer[i + 1]) << ((i + 1) * 8));
		} else {
			/* 8 - 15 byte inline */
			lineSum_1 += (((uint64_t)buffer[i]) << ((i - 8) * 8));
			lineSum_1 += (((uint64_t)buffer[i + 1])
				      << ((i - 8 + 1) * 8));
		}
		if ((i + 2) % 16 == 0) {
			hex2str(lineSum_0, asciiStr_0, sizeof(asciiStr_0));
			hex2str(lineSum_1, asciiStr_1, sizeof(asciiStr_1));
			printf(" %s%s\n", asciiStr_0, asciiStr_1);
			lineSum_0 = 0;
			lineSum_1 = 0;
		}
	}
}

/*
 * Description:
 *     read bytes from input binfile, output into specified LBA of vhdfile
 */
void write_fixed_disk_by_LBA(const char *binfile, const char *vhdfile,
			     uint32_t LBA)
{
	/* check if file exists */
	if (access(binfile, F_OK) == -1) {
		printf("File %s not exixts\n", binfile);
		exit(1);
	}
	if (access(vhdfile, F_OK) == -1) {
		printf("File %s not exixts\n", vhdfile);
		exit(1);
	}

	/* open input file */
	FILE *input_fp = fopen(binfile, "rb");
	if (input_fp == NULL) {
		printf("Cannot open file %s\n", binfile);
		exit(1);
	}
	/* move input_fp to begin */
	fseek(input_fp, 0, SEEK_SET);

	/* open output file */
	FILE *output_fp = fopen(vhdfile, "rb+");
	if (output_fp == NULL) {
		printf("Cannot open file %s\n", vhdfile);
		exit(1);
	}
	/* move output_fp to LBA */
	fseek(output_fp, LBA * 512, SEEK_SET);

	/* transfer */
	int input_size = get_filesize(binfile);
	uint8_t buffer[input_size];
	read_block(&buffer, sizeof(buffer), input_fp);
	write_block(&buffer, sizeof(buffer), output_fp);

	fclose(input_fp);
	fclose(output_fp);
}

/*
 * Description:
 *     read file footer into struct Footer
 */
struct footer *read_footer(const char *filepath)
{
	/* check if file exists */
	if (access(filepath, F_OK) == -1) {
		printf("File %s not exixts\n", filepath);
		exit(1);
	}

	int filesize = get_filesize(filepath);
	if (filesize < (VHD_MIN_BYTES + 512) || filesize > VHD_MAX_BYTES) {
		printf("File %s size %d Bytes, not in 4MB - 4GB\n", filepath,
		       filesize);
		exit(1);
	}

	FILE *fp = fopen(filepath, "rb");
	/* move fp to footer */
	fseek(fp, -512, SEEK_END);
	struct footer *footer = malloc(footer_size);
	read_block(footer, footer_size, fp);
	fclose(fp);
	return footer;
}

/*
 * Description:
 *     print struct Footer info
 */
void print_footer(const struct footer *footer)
{
	/* cookie */
	char cookie_str[sizeof(footer->cookie) + 1];
	hex2str(footer->cookie, cookie_str, sizeof(cookie_str));
	printf("cookie: %s\n", cookie_str);

	/* features */
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

	/* file format version */
	uint16_t *format_versions = get_version(footer->file_format_version);
	printf("file format version: %u.%u\n", format_versions[0],
	       format_versions[1]);

	/* data offset */
	printf("data offset: 0x%016lx\n", footer->data_offset);

	/* time stamp */
	uint32_t unix_timestamp =
		bswap_32(footer->time_stamp) +
		SECONDS_OFFSET; /* get seconds start from 1970 */
	time_t timer = unix_timestamp;
	struct tm *info = localtime(&timer);
	char buffer[80];
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
	printf("time stamp: 0x%08x (%s)\n", unix_timestamp, buffer);

	/* creator application */
	char app_str[sizeof(footer->creator_application) + 1];
	hex2str(footer->creator_application, app_str, sizeof(app_str));
	printf("creator application: %s\n", app_str);

	/* creator version */
	uint16_t *creator_versions = get_version(footer->creator_version);
	printf("creator version: %u.%u\n", creator_versions[0],
	       creator_versions[1]);

	/* creator host os */
	printf("creator host os: ");
	if (footer->creator_host_os == CREATOR_HOST_OS_Wi2k) {
		printf("Windows\n");
	} else if (footer->creator_host_os == CREATOR_HOST_OS_Mac) {
		printf("Macintosh\n");
	} else if (footer->creator_host_os == CREATOR_HOST_OS_Lnux) {
		printf("Linux\n");
	} else {
		printf("UNDEFINED\n");
	}

	/* original size */
	printf("original size: ");
	uint64_t original_B = bswap_64(footer->original_size);
	uint64_t original_MB, original_GB;
	original_MB = original_B / (1024 * 1024);
	original_GB = original_MB / 1024;
	if (original_GB > 0) {
		printf("%lu GB\n", original_GB);
	} else if (original_MB > 0) {
		printf("%lu MB\n", original_MB);
	} else {
		printf("%lu B\n", original_B);
	}

	/* current size */
	printf("current size: ");
	uint64_t current_B = bswap_64(footer->current_size);
	uint64_t current_MB, current_GB;
	current_MB = current_B / (1024 * 1024);
	current_GB = current_MB / 1024;
	if (current_GB > 0) {
		printf("%lu GB\n", current_GB);
	} else if (current_MB > 0) {
		printf("%lu MB\n", current_MB);
	} else {
		printf("%lu B\n", current_B);
	}

	/* disk geometry */
	printf("Cylinders: %hu\n", bswap_16(footer->disk_geometry.cylinders));
	printf("Heads: %d\n", footer->disk_geometry.heads);
	printf("SectorsPerTrack: %d\n", footer->disk_geometry.sectorsPerTrack);

	/* disk type */
	printf("disk type: ");
	uint32_t disk_type = footer->disk_type;
	if (disk_type == DISK_TYPE_NONE) {
		printf("%s\n", "None");
	} else if (disk_type == DISK_TYPE_RESERVED_DEPRECATED_0 ||
		   disk_type == DISK_TYPE_RESERVED_DEPRECATED_1 ||
		   disk_type == DISK_TYPE_RESERVED_DEPRECATED_2) {
		printf("%s\n", "Reserved (deprecated)");
	} else if (disk_type == DISK_TYPE_FIXED_HARD_DISK) {
		printf("%s\n", "Fixed hard disk");
	} else if (disk_type == DISK_TYPE_DYNAMIC_HARD_DISK) {
		printf("%s\n", "Dynamic hard disk");
	} else if (disk_type == DISK_TYPE_DIFFERENCING_HARD_DISK) {
		printf("%s\n", "Differencing hard disk");
	} else {
		printf("%s\n", "UNDEFINED");
	}

	/* checksum */
	printf("checksum: 0x%08x\n", bswap_32(footer->checksum));

	/* unique id */
	char uuid_str[37];
	uuid_unparse((uint8_t *)&footer->uuid, uuid_str);
	printf("unique id: %s\n", uuid_str);

	/* saved state */
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
 *     get size (byte) of a file
 */
int get_filesize(const char *filepath)
{
	/* check if file exists */
	if (access(filepath, F_OK) == -1) {
		printf("File %s not exixts\n", filepath);
		exit(1);
	}

	FILE *fp;
	int len;
	fp = fopen(filepath, "rb");
	if (fp == NULL) {
		printf("Cannot open file %s\n", filepath);
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fclose(fp);
	return len;
}

/*
 * Description:
 *     write block
 */
static void write_block(void *buffer, int bufferSize, FILE *fp)
{
	size_t bufferCount = fwrite(buffer, bufferSize, 1, fp);
	if (bufferCount != 1 && ferror(fp) != 0) {
		printf("Error occurs when writing buffer into file\n");
		fclose(fp);
		exit(1);
	}
}

/*
 * Description:
 *     read block
 */
static void read_block(void *buffer, int bufferSize, FILE *fp)
{
	size_t bufferCount = fread(buffer, bufferSize, 1, fp);
	if (bufferCount != 1 && ferror(fp) != 0) {
		printf("Error occurs when reading buffer from file\n");
		fclose(fp);
		exit(1);
	}
}

/*
 * Description:
 *     parse size string into byte num, eg. "1MB" => 1048576
 */
uint32_t parse_size(const char *sizeStr)
{
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
		printf("Size %s illegal\n", sizeStr);
		exit(1);
	}
	return sizeNum;
}

/*
 * Description:
 *     covert ascii hex number to string
 *
 * Params:
 *     - hex: length shorter than 64 bit
 *     - str: char array, length should be char nums + 1
 *     ('\0' need to be append to this array)
 *     - len_bytes: sizeof(str), number of bytes in str including '\0'
 *
 * Notice:
 *     - cannot sizeof(str) in this function to get len_bytes,
 *     because sizeof() return len of ptr, not len of arr
 *     - some special byte may cause result str cannot be print by %s.
 *     eg.0x00 corresponding to char '\0'
 *     - visual char range: 0x20 - 0x7e
 *     - the index of last byte in str is actually len_bytes-1
 */
void hex2str(uint64_t hex, char *str, int len_bytes)
{
	uint8_t mask = 0xff;
	for (int i = 0; i < (len_bytes - 1); i++) {
		uint8_t code = (hex >> (i * 8)) & mask;
		if (code >= 0x20 && code <= 0x7e) {
			str[i] = toascii(code);
		} else {
			/* replace invisual char with '.' */
			str[i] = '.';
		}
	}
	str[len_bytes - 1] = '\0';
}

/*
 * Description:
 *     Authority-defined algorithm to get CHS from size
 */
struct disk_geometry *cal_CHS(uint32_t totalSectors)
{
	uint32_t cylinderTimesHeads;
	uint16_t cylinders;
	uint8_t heads, sectorsPerTrack;

	if (totalSectors > 65535 * 16 * 255) {
		totalSectors =
			65535 * 16 *
			255; /* disk max size = 2^28 * 512 Byte = 128 GB */
	}

	if (totalSectors >= 65535 * 16 * 63) { /* when disk size >= 32 GB */
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
	/* to make sure cylinders > 0, vhd must bigger than 34KB */
	cylinders = cylinderTimesHeads / heads;

	struct disk_geometry *disk_geometry = malloc(sizeof(*disk_geometry));
	disk_geometry->cylinders = bswap_16(cylinders);
	disk_geometry->heads = heads;
	disk_geometry->sectorsPerTrack = sectorsPerTrack;
	return disk_geometry;
}

/*
 * Description:
 *     Athority-defined algorithm to get checksum field
 */
void fillin_checksum(struct footer *footer)
{
	uint32_t checksum = 0;

	uint8_t *p = (uint8_t *)footer;
	footer->checksum = 0;
	for (uint16_t counter = 0; counter < footer_size; counter++) {
		checksum += *p;
		p++;
	}
	footer->checksum = bswap_32(~checksum);
}

/*
 * Description:
 *     seperate major version and minor version
 */
uint16_t *get_version(uint32_t version_le)
{
	static uint16_t versions[2];
	uint32_t version_be = bswap_32(version_le);
	versions[0] = version_be >> 16; /* major version */
	versions[1] = version_be & 0x0000ffff; /* minor version */
	return versions;
}
