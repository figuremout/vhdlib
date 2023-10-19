/*
 * Authority-defined vals in footer
 * Notice: followings are all in big-endian byte order
 */
#ifndef _VHDLIB_H
#define _VHDLIB_H 1

#include <stdint.h>
#include <stdio.h>

#define DEFAULT_COOKIE 0x78697463656e6f63UL /* conectix */

#define FEATURES_NO_FEATURES_ENABLED 0x00000000U
#define FEATURES_TEMPORARY 0x01000000U
#define FEATURES_RESERVED 0x02000000U

#define CREATOR_HOST_OS_Wi2k 0x6b326957U /* Windows */
#define CREATOR_HOST_OS_Mac 0x2063614dU /* Macintosh */
#define CREATOR_HOST_OS_Lnux 0x78756e4cU /* Linux self-defined */

#define DISK_TYPE_NONE 0x00000000U
#define DISK_TYPE_RESERVED_DEPRECATED_0 0x01000000U
#define DISK_TYPE_FIXED_HARD_DISK 0x02000000U
#define DISK_TYPE_DYNAMIC_HARD_DISK 0x03000000U
#define DISK_TYPE_DIFFERENCING_HARD_DISK 0x04000000U
#define DISK_TYPE_RESERVED_DEPRECATED_1 0x05000000U
#define DISK_TYPE_RESERVED_DEPRECATED_2 0x06000000U

#define DEFAULT_FILE_FORMAT_VERSION 0x00000100U /* version 1.0 */

#define FIXED_HARD_DISK_DATA_OFFSET \
	0xffffffffffffffffUL /* this field is useless for fixed disk */

#define CREATOR_APPLICATION 0x006d6a7aU /* zjm */
#define CREATOR_VERSION 0x00000100U /* version 1.0 */

#define SAVED_STATE_YES 0x01U
#define SAVED_STATE_NO 0x00U

/*
 * Config
 */
#define VHD_MAX_BYTES 0xffffffffU /* 4 GB */
#define VHD_MIN_BYTES 0x00008800U /* 34 KB */
#define SECONDS_OFFSET \
	946699200 /* 1970.01.01 00:00:00 - 2000.01.01 12:00:00 946699200s */

/*
 * Footer bits struct
 * footer should be 512 Bytes in total, struct Footer has 85 Bytes,
 * should reserve 427 Bytes zeros after this struct.
 * Notice:
 * struct must set byte align to keep its size exactly,
 * otherwise GCC will fill in extra bytes.
 */
struct disk_geometry {
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectorsPerTrack;
} __attribute__((packed));

struct uuid {
	uint32_t f1;
	uint16_t f2;
	uint16_t f3;
	uint8_t f4[8];
} __attribute__((packed));

struct footer {
	uint64_t cookie;
	uint32_t features;
	uint32_t file_format_version;
	uint64_t data_offset;
	uint32_t time_stamp; /* This is the number of seconds since January 1, 2000 12:00:00 AM in UTC/GMT */
	uint32_t creator_application;
	uint32_t creator_version;
	uint32_t creator_host_os;
	uint64_t original_size;
	uint64_t current_size;
	struct disk_geometry disk_geometry;
	uint32_t disk_type;
	uint32_t checksum;
	struct uuid uuid;
	uint8_t saved_state;
} __attribute__((packed));

/*
 * Global variables
 */
extern const size_t footer_size;

/*
 * Function declarations
 */
extern void create_fixed_disk(const char *filepath, uint32_t len_bytes);
extern void print_fixed_disk_by_LBA(const char *vhdfile, uint32_t LBA);
extern void write_fixed_disk_by_LBA(const char *binfile, const char *vhdfile,
				    uint32_t LBA);
extern struct footer *read_footer(const char *filepath);
extern void print_footer(const struct footer *footer);

extern int get_filesize(const char *filepath);

extern struct disk_geometry *cal_CHS(uint32_t totalSectors);
extern void fillin_checksum(struct footer *footer);
extern void hex2str(uint64_t hex, char *str, int len_bytes);
extern uint16_t *get_version(uint32_t version_le);
extern uint32_t parse_size(const char *sizeStr);

#endif /* _VHDLIB_H */
