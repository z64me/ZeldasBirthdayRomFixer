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
#define OOT_ACTOR_TABLE_START  0x00B8D440
#define OOT_ACTOR_TABLE_END    0x00B90F20
#define OOT_SCENE_TABLE_START  0x00BA0BB0
#define OOT_SCENE_TABLE_END    0x00BA1448
#define OOT_OBJECT_TABLE_START 0x00B9E6C8
#define OOT_OBJECT_TABLE_END   0x00B9F358
#define OOT_DMADATA_START      0x00012F70
#define OOT_DMADATA_END        0x00019030

// zworld header commands
#define CMD_ALT 0x18 // alternate headers
#define CMD_TXA 0x0E // transition actors
#define CMD_ACT 0x01 // actor list
#define CMD_RFL 0x04 // room file list
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

bool dma_file_exists(uint8_t *rom, uint32_t start, uint32_t end, const char *type, int index)
{
	uint8_t *dmaStart = rom + OOT_DMADATA_START;
	uint8_t *dmaEnd = rom + OOT_DMADATA_END;
	const int dmaStride = 0x10;
	
	for (rom = dmaStart; rom < dmaEnd; rom += dmaStride)
	{
		if (BEu32(rom) == start)
		{
			if (BEu32(rom + 4) != end)
			{
				fprintf(stderr
					, "%s %d %08x %08x error: dmadata has different size (%08x %08x)\n"
					, type, index
					, start, end
					, start, BEu32(rom + 4)
				);
				return false;
			}
			else
				return true;
		}
	}
	
	fprintf(stderr, "%s %d %08x %08x error: no dma entry exists\n", type, index, start, end);
	return false;
}

uint16_t BEu16(const void *src)
{
	const uint8_t *b = src;
	
	return (b[0] << 8) | b[1];
}

bool is_overlay_excluded(const uint16_t v)
{
	// XXX some of these slots are repurposed in Zelda's Birthday
	const uint16_t unused[] = {
		0x0001, /*0x0003,*/ 0x0005, /*0x0006,*/ 0x0017, 0x001A, 0x001F, 0x0022,
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
	
	if ((off & 3) || (((off >> 24) != 0x03) && ((off >> 24) != 0x02)))
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

bool do_header(uint8_t *room, const size_t roomSz, uint32_t off, uint8_t *rom)
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
			// room file list
			case CMD_RFL:
			{
				int num = b[1];
				const int stride = 8;
				uint32_t addr = BEu32(b + 4);
				uint8_t *dat = room + (addr & 0xffffff);
				
				if (!addr || !num || !rom)
					break;
				
				for (int i = 0; i < num; ++i)
				{
					uint32_t start = BEu32(dat);
					uint32_t end = BEu32(dat + 4);
					
					dma_file_exists(rom, start, end, "room", i);
					do_header(rom + start, end - start, 0x03000000, rom);
					
					dat += stride;
				}
				break;
			}
			
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
					if (addr && !do_header(room, roomSz, addr, rom))
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

void do_rom(uint8_t *rom, const size_t romSz)
{
	const int spanScene = 0x14;
	const int spanActor = 0x20;
	const int spanObject = 0x8;
	
	// for each entry in the scene table
	for (uint32_t i = OOT_SCENE_TABLE_START; i < OOT_SCENE_TABLE_END; i += spanScene)
	{
		uint8_t *dat = rom + i;
		uint32_t start = BEu32(dat);
		uint32_t end = BEu32(dat + 4);
		
		if (start == 0 || end < start || start >= romSz)
			continue;
		
		//fprintf(stderr, "do scene %08x %08x\n", start, end);
		dma_file_exists(rom, start, end, "scene", (i - OOT_SCENE_TABLE_START) / spanScene);
		do_header(rom + start, end - start, 0x02000000, rom);
	}
	
	// sanity check object table
	for (uint32_t i = OOT_OBJECT_TABLE_START; i < OOT_OBJECT_TABLE_END; i += spanObject)
	{
		uint8_t *dat = rom + i;
		uint32_t start = BEu32(dat);
		uint32_t end = BEu32(dat + 4);
		
		if (start == 0 || end < start || start >= romSz)
			continue;
		
		dma_file_exists(rom, start, end, "object", (i - OOT_OBJECT_TABLE_START) / spanObject);
	}
	
	// sanity check actor table
	for (uint32_t i = OOT_ACTOR_TABLE_START; i < OOT_ACTOR_TABLE_END; i += spanActor)
	{
		uint8_t *dat = rom + i;
		uint32_t start = BEu32(dat);
		uint32_t end = BEu32(dat + 4);
		
		if (start == 0 || end < start || start >= romSz)
			continue;
		
		dma_file_exists(rom, start, end, "actor", (i - OOT_ACTOR_TABLE_START) / spanActor);
	}
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
	
	if (is_header(room, roomSz, 0x03000000))
	{
		do_header(room, roomSz, 0x03000000, 0);
	}
	else if (roomSz > OOT_SCENE_TABLE_END)
	{
		do_rom(room, roomSz);
	}
	
	if (!savefile(ofn, room, roomSz))
	{
		fprintf(stderr, "failed to write output file '%s'\n", fn);
		return -1;
	}
	
	free(room);
	return 0;
}
