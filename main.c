/*
 * zworld-omit-unused-actors <z64.me>
 *
 * finds and deletes unused actors
 * from Zelda 64 scene and room files
 *
 * gcc -o zworld-omit-unused-actors \
 *     -Wall -Wextra -std=c99 -pedantic \
 *     main.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define PROGNAME "zworld-omit-unused-actors"
#define OOT_ACTOR_TABLE_LENGTH 471

// zworld header commands
#define CMD_ALT 0x18 // alternate headers
#define CMD_TXA 0x0E // transition actors
#define CMD_ACT 0x01 // actor list
#define CMD_END 0x14 // end of header

/* minimal file loader
 * returns 0 on failure
 * returns pointer to loaded file on success
 */
void *loadfile(const char *fn, size_t *sz)
{
	FILE *fp;
	void *dat;
	
	/* rudimentary error checking returns 0 on any error */
	if (
		!fn
		|| !sz
		|| !(fp = fopen(fn, "rb"))
		|| fseek(fp, 0, SEEK_END)
		|| !(*sz = ftell(fp))
		|| fseek(fp, 0, SEEK_SET)
		|| !(dat = malloc(*sz))
		|| fread(dat, 1, *sz, fp) != *sz
		|| fclose(fp)
	)
		return 0;
	
	return dat;
}

/* minimal file writer
 * returns 0 on failure
 * returns non-zero on success
 */
int savefile(const char *fn, const void *dat, const size_t sz)
{
	FILE *fp;
	
	/* rudimentary error checking returns 0 on any error */
	if (
		!fn
		|| !sz
		|| !dat
		|| !(fp = fopen(fn, "wb"))
		|| fwrite(dat, 1, sz, fp) != sz
		|| fclose(fp)
	)
		return 0;
	
	return 1;
}

uint32_t BEu32(const void *src)
{
	const uint8_t *b = src;
	
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

uint16_t BEu16(const void *src)
{
	const uint8_t *b = src;
	
	return (b[0] << 8) | b[1];
}

bool is_overlay_excluded(const uint16_t v)
{
	const uint16_t unused[] = {
		0x0001, 0x0003, 0x0005, 0x0006, 0x0017, 0x001A, 0x001F, 0x0022,
		0x0031, 0x0036, 0x0053, 0x0073, 0x0074, 0x0075, 0x0076, 0x0078,
		0x0079, 0x007A, 0x007B, 0x007E, 0x007F, 0x0083, 0x00A0, 0x00B2,
		0x00CE, 0x00D8, 0x00EA, 0x00EB, 0x00F2, 0x00F3, 0x00FB, 0x0109,
		0x010D, 0x010E, 0x0128, 0x0129, 0x0134, 0x0154, 0x015D, 0x0161,
		0x0180, 0x01AA
	};
	const int num = sizeof(unused) / sizeof(*unused);
	
	if (v >= OOT_ACTOR_TABLE_LENGTH)
		return true;
	
	for (int i = 0; i < num; ++i)
		if (unused[i] == v)
			return true;
	
	return false;
}

bool is_header(uint8_t *room, const size_t roomSz, uint32_t off)
{
	const int stride = 8;
	const uint8_t pat[8] = { CMD_END }; // bigendian bytes 14000000 00000000
	uint32_t end = (off & 0xffffff) + 0x80; // a forgiving header length
	
	if ((off & 3) || ((off >> 24) != 0x03))
		return false;
	
	// bounds safety
	if (roomSz - stride < end)
		end = roomSz - stride;
	
	// if end-header pattern is found, it's a header
	for (off &= 0xffffff; off <= end; off += stride)
		if (!memcmp(room + off, pat, stride))
			return true;
	
	// otherwise, it isn't
	return false;
}

bool do_header(uint8_t *room, const size_t roomSz, uint32_t off)
{
	uint8_t *roomEnd = room + roomSz;
	const int stride = 8;
	
	if (!is_header(room, roomSz, off))
		return false;
	
	for (off &= 0xffffff; off <= roomSz - stride; off += stride)
	{
		uint8_t *b = room + off;
		
		switch (*b)
		{
			case CMD_TXA: // transition actors
			case CMD_ACT: // actor list
			{
				int num = b[1];
				const int stride = 16;
				uint32_t addr = BEu32(b + 4);
				uint8_t *start = room + (addr & 0xffffff);
				uint8_t *end = start + num * stride;
				uint8_t *dat = start;
				int off = (*b == CMD_TXA) ? 4 : 0;
				
				if (!addr || !num)
					break;
				
				for (int i = 0; i < num; )
				{
					uint16_t overlay = BEu16(dat + off);
					
					if (is_overlay_excluded(overlay))
					{
						memmove(dat, dat + stride, (num - (i + 1)) * stride);
						
						--num;
						continue;
					}
					
					++i;
					dat += stride;
				}
				
				memset(dat, 0, end - dat);
				b[1] = num;
				break;
			}
			
			// alternate headers
			case CMD_ALT:
			{
				uint32_t addr = BEu32(b + 4);
				uint8_t *dat = room + (addr & 0xffffff);
				
				if (!addr)
					break;
				
				while (dat <= roomEnd - 4)
				{
					addr = BEu32(dat);
					
					// skip addresses 00000000, parse all others
					if (addr && !do_header(room, roomSz, addr))
						break;
					
					dat += 4;
				}
				break;
			}
			
			// end
			case CMD_END:
				return true;
		}
	}
	
	return true;
}

int main(int argc, char *argv[])
{
	const char *ofn = argv[2];
	const char *fn = argv[1];
	uint8_t *room;
	size_t roomSz;
	
	fprintf(stderr, PROGNAME " <z64.me>\n");
	
	if (!ofn)
		ofn = fn;
	
	if (argc != 2 && argc != 3)
	{
		fprintf(stderr, "args:\n" PROGNAME " \"infile.zworld\" \"outfile.zworld\"\n");
		fprintf(stderr, "outfile is optional; if not specified, infile is overwritten\n");
		fprintf(stderr, "supports both scene and room files, hence zworld\n");
		#ifdef _WIN32
		fprintf(stderr, "simple drag-n-drop style win32 application\n");
		fprintf(stderr, "(aka close this window and drag a zworld onto the exe)\n");
		fprintf(stderr, "(warning: it will modify the zworld so keep a backup!)\n");
		getchar();
		#endif
		return -1;
	}
	
	if (!(room = loadfile(fn, &roomSz)))
	{
		fprintf(stderr, "failed to open or read input file '%s'\n", fn);
		return -1;
	}
	
	do_header(room, roomSz, 0x03000000);
	
	if (!savefile(ofn, room, roomSz))
	{
		fprintf(stderr, "failed to write output file '%s'\n", fn);
		return -1;
	}
	
	free(room);
	return 0;
}
