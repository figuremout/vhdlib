/*
 * Author: figuremout
 * Email: 1344584929@qq.com
 * Describtion:
 *     This program relies on Virtual Hard Disk Image Format Specification:
 *     https://www.microsoft.com/en-us/download/details.aspx?id=23850
 */
#include "vhdlib.h"
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>

/*
 * Main
 */
int main(int argc, char *argv[])
{
#if DEBUG
	printf("DEBUG mode on\n");
#endif
	int ch;
	opterr = 0;

	uint16_t *creator_versions;
	int r_count = 0, w_count = 0, b_count = 0;
	uint32_t r_args[argc], w_args[argc], s_arg = 0;
	char *b_args[argc], *d_arg = NULL;

	while ((ch = getopt(argc, argv, "vhr:w:d:b:s:")) != -1) {
		switch (ch) {
		case 'v':
			creator_versions = get_version(CREATOR_VERSION);
			printf("VHD %u.%u - A tool to r/w vhd\n",
			       creator_versions[0], creator_versions[1]);
			break;
		case 'h':
			creator_versions = get_version(CREATOR_VERSION);
			printf("VHD %u.%u - A tool to r/w vhd\n",
			       creator_versions[0], creator_versions[1]);
			// usage
			printf("\nusage: vhd -d [vhdfile]\t\t\t\t\t"
			       "show vhdfile footer info");
			printf("\n\tor: vhd -d [vhdfile] -w[LBA] -b [binfile]\t"
			       "write bin into specified LBA");
			printf("\n\tor: vhd -d [vhdfile] -r[LBA]\t\t\t"
			       "output specified LBA");
			printf("\n\tor: vhd -d [vhdfile] -s[size]\t\t\t"
			       "create vhdfile");

			printf("\n\nArguments:\n");
			printf("\t-h\tshow help\n");
			printf("\t-v\tshow version\n");
			printf("\t-s\tspecify VHD size to create "
			       "(B, K/KB, M/MB, G/GB), range 4MB - 4GB\n");
			printf("\t-r\tspecify LBA to read\n");
			printf("\t-w\tspecify LBA to write\n");
			printf("\t-d\tspecify vhdfile\n");
			printf("\t-b\tspecify binfile\n");
			break;
		case 'd':
			if (d_arg) {
				printf("Too many option -%c\n", ch);
				exit(1);
			} else {
				d_arg = optarg;
			}
			break;
		case 's':
			if (s_arg) {
				printf("Too many option -%c\n", ch);
				exit(1);
			} else {
				s_arg = parse_size(optarg);
			}
			break;
		case 'r':
			r_args[r_count] = atoi(optarg);
			r_count++;
			break;
		case 'w':
			w_args[w_count] = atoi(optarg);
			w_count++;
			break;
		case 'b':
			b_args[b_count] = optarg;
			b_count++;
			break;
		default:
			printf("Undefined option: -%c\n", optopt);
		}
	}

	// create vhdfile
	if (s_arg > 0 && d_arg) {
		printf("------------------------\n");
		create_fixed_disk(d_arg, s_arg);
		printf("Create VHD %s DONE\n", d_arg);
		printf("------------------------\n");
	}

	// print vhdfile's footer
	uint32_t maxLBA = 0;
	if (!d_arg) {
		printf("Not specify vhdfile\n");
	} else {
		struct footer *footer = read_footer(d_arg);
		maxLBA = bswap_16(footer->disk_geometry.cylinders) *
				footer->disk_geometry.heads *
				footer->disk_geometry.sectorsPerTrack -
			1;
		if (!s_arg && w_count <= 0 && r_count <= 0) {
			// only -d exists
			printf("------------------------\n");
			printf("* FILE %s\n", d_arg);
			printf("* LBA range: 0 - %u\n", maxLBA);
			printf("------------------------\n");
			print_footer(footer);
			printf("------------------------\n");
		}
	}

	// write bin into vhdfile
	if (w_count > 0 && w_count == b_count && d_arg) {
		printf("------------------------\n");
		for (int i = 0; i < w_count; i++) {
			// make sure start LBA in range
			if (w_args[i] > maxLBA) {
				printf("LBA %u out of range: 0 - %u\n",
				       w_args[i], maxLBA);
				continue;
			}
			// make sure end LBA in range
			int input_size = get_filesize(b_args[i]);
			if (((input_size - 1) / 512 + w_args[i]) > maxLBA) {
				printf("File %s size out of disk size\n",
				       b_args[i]);
				continue;
			}
			write_fixed_disk_by_LBA(b_args[i], d_arg, w_args[i]);
			printf("Write: VHD %s LBA %u <= BIN %s DONE\n", d_arg,
			       w_args[i], b_args[i]);
		}
		printf("------------------------\n");
	} else if (w_count > 0 && w_count != b_count && d_arg) {
		printf("-w -b not in pair\n");
	}

	// read LBA
	if (r_count > 0 && d_arg) {
		for (int i = 0; i < r_count; i++) {
			printf("------------------------\n");
			printf("* LBA %u of VHD %s\n", r_args[i], d_arg);
			printf("------------------------\n");
			print_fixed_disk_by_LBA(d_arg, r_args[i]);
			printf("------------------------\n");
		}
	}
	return 0;
}
