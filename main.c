/*
 * Zelda's Birthday ROM Fixer <z64.me>
 *
 * This ad hoc utility was thrown together to quickly fix
 * miscellaneous files from an ancient Zelda 64 mod titled
 * Zelda's Birthday so it can be played on a wider variety
 * of emulators, as well as on real Nintendo hardware.
 *
 * gcc -o ZeldasBirthdayRomFixer \
 *     -Wall -Wextra -std=c99 -pedantic \
 *     main.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "include/incbin.h"

#define PROGNAME "ZeldasBirthdayRomFixer"
#define OOT_ACTOR_TABLE_LENGTH 471
#define OOT_ACTOR_TABLE_START  0x00B8D440
#define OOT_ACTOR_TABLE_END    0x00B90F20
#define OOT_SCENE_TABLE_START  0x00BA0BB0
#define OOT_SCENE_TABLE_END    0x00BA1448
#define OOT_OBJECT_TABLE_START 0x00B9E6C8
#define OOT_OBJECT_TABLE_END   0x00B9F358
#define OOT_DMADATA_START      0x00012F70
#define OOT_DMADATA_END        0x00019030

#define SCENE_UNUSED_FIRST 0x0004
#define SCENE_UNUSED_LAST  0x0006
#define DMA_UNUSED_FIRST   0x0475
#define DMA_UNUSED_LAST    0x04C6

// zworld header commands
#define CMD_ALT 0x18 // alternate headers
#define CMD_TXA 0x0E // transition actors
#define CMD_ACT 0x01 // actor list
#define CMD_OBJ 0x0B // object list
#define CMD_RFL 0x04 // room file list
#define CMD_END 0x14 // end of header

// misc payloads
#define PLDIR "include/"
INCBIN(EagleCollisionPayload, PLDIR "eagle-collision-payload.bin");
INCBIN(LadderActorPayload, PLDIR "ladder-actor-payload.bin");
INCBIN(LadderObjectPayload, PLDIR "ladder-object-payload.bin");
#define PL_LADDER_ACTOR_ID 0x00E2
#define PL_LADDER_OBJECT_ID 0x013F

//
//
// crc copy-pasta
//
//
/* snesrc - SNES Recompiler
 *
 * Mar 23, 2010: addition by spinout to actually fix CRC if it is incorrect
 *
 * Copyright notice for this file:
 *  Copyright (C) 2005 Parasyte
 *
 * Based on uCON64's N64 checksum algorithm by Andreas Sterbenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>

#define ROL(i, b) (((i) << (b)) | ((i) >> (32 - (b))))
#define BYTES2LONG(b) ( (b)[0] << 24 | \
                        (b)[1] << 16 | \
                        (b)[2] <<  8 | \
                        (b)[3] )

#define N64_HEADER_SIZE  0x40
#define N64_BC_SIZE      (0x1000 - N64_HEADER_SIZE)

#define N64_CRC1         0x10
#define N64_CRC2         0x14

#define CHECKSUM_START   0x00001000
#define CHECKSUM_LENGTH  0x00100000
#define CHECKSUM_CIC6102 0xF8CA4DDC
#define CHECKSUM_CIC6103 0xA3886759
#define CHECKSUM_CIC6105 0xDF26F436
#define CHECKSUM_CIC6106 0x1FEA617A

static void gen_table(unsigned int crc_table[256])
{
	unsigned int crc, poly;
	int	i, j;

	poly = 0xEDB88320;
	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1) crc = (crc >> 1) ^ poly;
			else crc >>= 1;
		}
		crc_table[i] = crc;
	}
}

static unsigned int crc32(
	unsigned int crc_table[256]
	, unsigned char *data
	, int len
)
{
	unsigned int crc = ~0;
	int i;

	for (i = 0; i < len; i++) {
		crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
	}

	return ~crc;
}

static int N64GetCIC(unsigned int crc_table[256], unsigned char *data)
{
	switch (crc32(crc_table, &data[N64_HEADER_SIZE], N64_BC_SIZE)) {
		case 0x6170A4A1: return 6101;
		case 0x90BB6CB5: return 6102;
		case 0x0B050EE0: return 6103;
		case 0x98BC2C86: return 6105;
		case 0xACC8580A: return 6106;
	}

	return 0;
}

static int N64CalcCRC(
	unsigned int crc_table[256]
	, unsigned int *crc
	, unsigned char *data
)
{
	int bootcode, i;
	unsigned int seed;
	unsigned int t1, t2, t3;
	unsigned int t4, t5, t6;
	unsigned int r, d;

	switch ((bootcode = N64GetCIC(crc_table, data))) {
		case 6101:
		case 6102:
			seed = CHECKSUM_CIC6102;
			break;
		case 6103:
			seed = CHECKSUM_CIC6103;
			break;
		case 6105:
			seed = CHECKSUM_CIC6105;
			break;
		case 6106:
			seed = CHECKSUM_CIC6106;
			break;
		default:
			return 1;
	}

	t1 = t2 = t3 = t4 = t5 = t6 = seed;

	i = CHECKSUM_START;
	while (i < (CHECKSUM_START + CHECKSUM_LENGTH)) {
		d = BYTES2LONG(&data[i]);
		if ((t6 + d) < t6)
			t4++;
		t6 += d;
		t3 ^= d;
		r = ROL(d, (d & 0x1F));
		t5 += r;
		if (t2 > d)
			t2 ^= r;
		else
			t2 ^= t6 ^ d;

		if (bootcode == 6105)
			t1 += BYTES2LONG(&data[N64_HEADER_SIZE + 0x0710 + (i & 0xFF)]) ^ d;
		else
			t1 += t5 ^ d;

		i += 4;
	}
	if (bootcode == 6103) {
		crc[0] = (t6 ^ t4) + t3;
		crc[1] = (t5 ^ t2) + t1;
	}
	else if (bootcode == 6106) {
		crc[0] = (t6 * t4) + t3;
		crc[1] = (t5 * t2) + t1;
	}
	else {
		crc[0] = t6 ^ t4 ^ t3;
		crc[1] = t5 ^ t2 ^ t1;
	}

	return 0;
}

/* recalculate rom crc */
void n64crc(void *rom)
{
	unsigned int crc_table[256];
	unsigned char CRC1[4];
	unsigned char CRC2[4];
	unsigned int crc[2];
	unsigned char *rom8 = rom;
	
	assert(rom);
	
	gen_table(crc_table);

	if (!N64CalcCRC(crc_table, crc, rom))
	{
		unsigned int kk1 = crc[0];
		unsigned int kk2 = crc[1];
		int i;
		
		for (i = 0; i < 4; ++i)
		{
			CRC1[i] = (kk1 >> (24-8*i))&0xFF;
			CRC2[i] = (kk2 >> (24-8*i))&0xFF;
		}
		
		for (i = 0; i < 4; ++i)
			*(rom8 + N64_CRC1 + i) = CRC1[i];
		
		for (i = 0; i < 4; ++i)
			*(rom8 + N64_CRC2 + i) = CRC2[i];
	}
}

//
//
// end crc copy-pasta
//
//

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

void wBEu32(void *dst, uint32_t v)
{
	uint8_t *b = dst;
	
	b[0] = v >> 24;
	b[1] = v >> 16;
	b[2] = v >>  8;
	b[3] = v;
}

void wBEu16(void *dst, uint16_t v)
{
	uint8_t *b = dst;
	
	b[0] = v >> 8;
	b[1] = v;
}

void dma_file_add(uint8_t *rom, uint32_t start, uint32_t end)
{
	uint8_t *dmaStart = rom + OOT_DMADATA_START;
	uint8_t *dmaEnd = rom + OOT_DMADATA_END;
	const int dmaStride = 0x10;
	const uint8_t blank[0x10] = { 0 };
	
	for (rom = dmaStart; rom < dmaEnd; rom += dmaStride)
	{
		if (!memcmp(rom, blank, dmaStride))
		{
			wBEu32(rom, start);
			wBEu32(rom + 4, end);
			wBEu32(rom + 8, start);
			fprintf(stderr, "added file %08x %08x to dmadata\n", start, end);
			return;
		}
	}
}

bool dma_file_exists(uint8_t *rom, uint32_t start, uint32_t end, const char *type, int index)
{
	uint8_t *oRom = rom;
	uint8_t *dmaStart = rom + OOT_DMADATA_START;
	uint8_t *dmaEnd = rom + OOT_DMADATA_END;
	const int dmaStride = 0x10;
	
	for (rom = dmaStart; rom < dmaEnd; rom += dmaStride)
	{
		if (BEu32(rom) == start)
		{
			if (BEu32(rom + 4) != end)
			{
				/*
				fprintf(stderr
					, "%s %d %08x %08x error: dmadata has different size (%08x %08x)\n"
					, type, index
					, start, end
					, start, BEu32(rom + 4)
				);
				*/
				
				// update existing dmadata entry
				fprintf(stderr, "updated file %08x %08x in dmadata\n", start, end);
				wBEu32(rom + 4, end);
				return true;
				
				return false;
			}
			else
				return true;
		}
	}
	
	// doesn't exist in dmadata: add it
	dma_file_add(oRom, start, end);
	
	//fprintf(stderr, "%s %d %08x %08x error: no dma entry exists\n", type, index, start, end);
	return false;
	
	(void)type;
	(void)index;
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
	uint32_t end = (off & 0xffffff) + 0xA0; // a forgiving header length
	
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

bool do_header(uint8_t *room, size_t *roomSz, uint32_t off, uint8_t *rom)
{
	uint8_t *roomEnd = room + *roomSz;
	const int stride = 8;
	
	if (!is_header(room, *roomSz, off))
		return false;
	
	#if 0 // XXX hard-coded spider house fix
	// was once required but now the rom is stable without it
	if (*roomSz == 0xfe40)
	{
		const uint8_t spider[] = {
			0x15, 0x05, 0x00, 0x00, 0x00, 0x00, 0x13, 0x1C, 0x04, 0x06, 0x00, 0x00,
			0x02, 0x00, 0x01, 0x30, 0x0E, 0x05, 0x00, 0x00, 0x02, 0x00, 0x00, 0xA8,
			0x0C, 0x04, 0x00, 0x00, 0x02, 0x00, 0x00, 0xF8, 0x19, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0xAE, 0x8C,
			0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x60, 0x07, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x03, 0x0D, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x20,
			0x00, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x98, 0x11, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x13, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x64,
			0x0F, 0x04, 0x00, 0x00, 0x02, 0x00, 0x01, 0x68, 0x1B, 0x1B, 0x00, 0x00,
			0x02, 0x00, 0x02, 0x60, 0x02, 0x10, 0x00, 0x00, 0x02, 0x00, 0x05, 0x80,
			0x1C, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x4C, 0x1A, 0x00, 0x00, 0x00,
			0x02, 0x00, 0x06, 0x00, 0x1E, 0x01, 0x00, 0x00, 0x02, 0x00, 0x04, 0x54,
			0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		
		// it's a match
		if (!memcmp(room, spider, sizeof(spider)))
		{
			// header fix: 11000000 00000100 -> 11000000 00000000
			room[0x56] = 0x00;
			
			fprintf(stderr, "applying spider house patch\n");
		}
	}
	#endif
	
	// XXX hard-coded Eagle Labyrinth dungeon fixes
	{
		// replace old collision with custom collision
		// (loading time improved from 18 seconds to 1 second)
		if (*roomSz == 0x1A7D0 && (room - rom) == 0x3913000)
		{
			fprintf(stderr, "applying eagle labyrinth patch\n");
			
			// inject custom collision data
			memcpy(room + 0x460, gEagleCollisionPayloadData, gEagleCollisionPayloadSize);
			
			// update header to reference new collision data
			wBEu32(room + 0x24, 0x02002FDC);
			
			// shrink scene file
			*roomSz = 0x3010;
			
			// quick warp to room11
			if (false)
			{
				wBEu32(room + 0x2D8, 0x03986000);
				wBEu32(room + 0x2D8 + 4, 0x0398A7E0);
			}
		}
		
		// room11: replace ladder
		if (*roomSz == 0x47e0 && (room - rom) == 0x03986000 && room[0x31] == 0x15)
		{
			wBEu16(room + 0x4670, PL_LADDER_ACTOR_ID);
			wBEu16(room + 0x46, PL_LADDER_OBJECT_ID);
		}
	}
	
	for (off &= 0xffffff; off <= *roomSz - stride; off += stride)
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
					size_t sz = end - start;
					
					do_header(rom + start, &sz, 0x03000000, rom);
					
					// possible resize
					dma_file_exists(rom, start, start + sz, "room", i);
					
					dat += stride;
				}
				break;
			}
			
			case CMD_OBJ: // object list patching
				break;
			
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
	const int spanDma = 0x10;
	
	// XXX free up some dmadata and scene table entries to make room for customs
	memset(rom + OOT_DMADATA_START + DMA_UNUSED_FIRST * spanDma
		, 0, ((DMA_UNUSED_LAST + 1) - DMA_UNUSED_FIRST) * spanDma
	);
	memset(rom + OOT_SCENE_TABLE_START + SCENE_UNUSED_FIRST * spanScene
		, 0, ((SCENE_UNUSED_LAST + 1) - SCENE_UNUSED_FIRST) * spanScene
	);
	
	// for each entry in the scene table
	for (uint32_t i = OOT_SCENE_TABLE_START; i < OOT_SCENE_TABLE_END; i += spanScene)
	{
		uint8_t *dat = rom + i;
		uint32_t start = BEu32(dat);
		uint32_t end = BEu32(dat + 4);
		size_t sz = end - start;
		
		if (start == 0 || end < start || start >= romSz)
			continue;
		
		//fprintf(stderr, "do scene %08x %08x\n", start, end);
		do_header(rom + start, &sz, 0x02000000, rom);
		
		// possible resize
		dma_file_exists(rom, start, start + sz, "scene", (i - OOT_SCENE_TABLE_START) / spanScene);
		
		// overwrite file end, in case of resize
		wBEu32(dat + 4, start + sz);
	}
	
	// sanity check object table
	for (uint32_t i = OOT_OBJECT_TABLE_START; i < OOT_OBJECT_TABLE_END; i += spanObject)
	{
		uint8_t *dat = rom + i;
		uint32_t start = BEu32(dat);
		uint32_t end = BEu32(dat + 4);
		uint32_t sz = end - start;
		int idx = (i - OOT_OBJECT_TABLE_START) / spanObject;
		
		// XXX object payloads
		{
			// inject custom ladder object payload
			if (idx == PL_LADDER_OBJECT_ID)
				memcpy(rom + start, gLadderObjectPayloadData, (sz = gLadderObjectPayloadSize));
		}
		
		if (start == 0 || end < start || start >= romSz)
			continue;
		
		dma_file_exists(rom, start, start + sz, "object", idx);
		wBEu32(dat, start);
		wBEu32(dat + 4, start + sz);
	}
	
	// sanity check actor table
	for (uint32_t i = OOT_ACTOR_TABLE_START; i < OOT_ACTOR_TABLE_END; i += spanActor)
	{
		uint8_t *dat = rom + i;
		uint32_t start = BEu32(dat);
		uint32_t end = BEu32(dat + 4);
		uint32_t sz = end - start;
		int idx = (i - OOT_ACTOR_TABLE_START) / spanActor;
		
		// XXX actor overlay payloads
		{
			// inject custom ladder actor payload
			if (idx == PL_LADDER_ACTOR_ID)
			{
				const unsigned char addrs[24] = {
					0x80, 0xB9, 0x59, 0xD0, 0x80, 0xB9, 0x60, 0xD0, 0x00, 0x00, 0x00, 0x00,
					0x80, 0xB9, 0x5F, 0xB0, 0x80, 0x13, 0x82, 0xD4, 0x00, 0x00, 0x00, 0x00
				};
				memcpy(dat + 8, addrs, sizeof(addrs));
				memcpy(rom + start, gLadderActorPayloadData, (sz = gLadderActorPayloadSize));
				wBEu16(rom + start + 0x5E8, PL_LADDER_OBJECT_ID);
			}
		}
		
		if (start == 0 || end < start || start >= romSz)
			continue;
		
		dma_file_exists(rom, start, start + sz, "actor", idx);
		wBEu32(dat, start);
		wBEu32(dat + 4, start + sz);
	}
	
	// misc patches...
	{
		// saria crash fix
		{
			uint8_t *saria = rom + 0x00EAB540;
			
			// restore original assembly
			wBEu32(saria + 0xD98 + 0, 0x3C0E8016); // lui t6, 0x8016
			wBEu32(saria + 0xD98 + 4, 0xA6000210); // sh r0, 0x0210(s0)
			
			// disable Kokiri Forest cutscene
			wBEu32(saria + 0x9C4, 0x24020003); // addiu v0, r0, 0x0003 ; make branch 4 function same as branch 3
		}
	}
	
	// update crc checksum
	n64crc(rom);
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
		fprintf(stderr, "misc fixes are applied if you throw a rom at it (recommended)\n");
		#ifdef _WIN32
		fprintf(stderr, "simple drag-n-drop style win32 application\n");
		fprintf(stderr, "(aka close this window and drag a zworld onto the exe)\n");
		fprintf(stderr, "(warning: it will modify the input file, keep a backup!)\n");
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
		do_header(room, &roomSz, 0x03000000, 0);
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
