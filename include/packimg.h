#ifndef _PACKIMG_H
#define _PACKIMG_H

#include <common.h>

#define PACK_MAGIC 0x4b434150
#define PACK_NAME_MAX 32

struct pack_header {
	uint32_t magic;
	uint32_t nentry;
	uint32_t crc;
};

struct pack_entry {
	uint32_t offset;
	uint32_t size;
	uint32_t ldaddr;
	uint32_t crc;
	char name[PACK_NAME_MAX];
};

#endif
