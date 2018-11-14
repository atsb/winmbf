#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "m_fixed.h"
#include "z_zone.h"
#include "i_system.h"

#define CACHE_ALIGN                     32
#define CHUNK_SIZE                      32
#define MIN_BLOCK_SPLIT                 1024
#define LEAVE_ASIDE                     (128 * 1024)
#define RETRY_AMOUNT                    (256 * 1024)
#define ZONEID                          0x931d4a11
#define ZONE_HISTORY                    4

typedef struct memblock
{

    struct memblock *next;
    struct memblock *prev;
    size_t size;
    void **user;
    unsigned char tag;

} memblock_t;

static const size_t HEADER_SIZE = (sizeof(memblock_t) + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);
static memblock_t *blockbytag[PU_MAX];
static int memory_size = 0;
static int free_memory = 0;

void *(Z_Malloc)(size_t size, int tag, void **user)
{

    memblock_t *block = NULL;

    if (!size)
        return user ? *user = NULL : NULL;

    size = (size + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);

    if (memory_size > 0 && ((free_memory + memory_size) < (int)(size + HEADER_SIZE)))
    {

        memblock_t *end_block;
        block = blockbytag[PU_CACHE];

        if (block)
        {

            end_block = block->prev;

            while (1)
            {

                memblock_t *next = block->next;

                (Z_Free)((char *) block + HEADER_SIZE);

                if (((free_memory + memory_size) >= (int)(size + HEADER_SIZE)) || (block == end_block))
                    break;

                block = next;
            }

        }

        block = NULL;

    }

    while (!(block = (malloc)(size + HEADER_SIZE)))
    {

        if (!blockbytag[PU_CACHE])
            I_Error("Z_Malloc: Failure trying to allocate %lu bytes", (unsigned long)size);

        Z_FreeTags(PU_CACHE,PU_CACHE);

    }

    if (!blockbytag[tag])
    {

        blockbytag[tag] = block;
        block->next = block->prev = block;

    }

    else
    {

        blockbytag[tag]->prev->next = block;
        block->prev = blockbytag[tag]->prev;
        block->next = blockbytag[tag];
        blockbytag[tag]->prev = block;

    }

    block->size = size;
    free_memory -= block->size;
    block->tag = tag;
    block->user = user;
    block = (memblock_t *)((char *)block + HEADER_SIZE);

    if (user)
        *user = block;

    return block;

}

void (Z_Free)(void *p)
{

    memblock_t *block = (memblock_t *)((char *)p - HEADER_SIZE);

    if (!p)
        return;

    if (block->user)
        *block->user = NULL;

    if (block == block->next)
        blockbytag[block->tag] = NULL;
    else if (blockbytag[block->tag] == block)
        blockbytag[block->tag] = block->next;

    block->prev->next = block->next;
    block->next->prev = block->prev;
    free_memory += block->size;

    (free)(block);

}

void (Z_FreeTags)(int lowtag, int hightag)
{

    if (lowtag <= PU_FREE)
        lowtag = PU_FREE + 1;

    if (hightag > PU_CACHE)
        hightag = PU_CACHE;

    for (; lowtag <= hightag; lowtag++)
    {

        memblock_t *block, *end_block;

        block = blockbytag[lowtag];

        if (!block)
            continue;

        end_block = block->prev;

        while (1)
        {

            memblock_t *next = block->next;

            (Z_Free)((char *) block + HEADER_SIZE);

            if (block == end_block)
                break;

            block = next;

        }

    }

}

void (Z_ChangeTag)(void *ptr, int tag)
{

    memblock_t *block = (memblock_t *)((char *) ptr - HEADER_SIZE);

    if (!ptr)
        return;

    if (tag == block->tag)
        return;

    if (block == block->next)
        blockbytag[block->tag] = NULL;
    else if (blockbytag[block->tag] == block)
        blockbytag[block->tag] = block->next;

    block->prev->next = block->next;
    block->next->prev = block->prev;

    if (!blockbytag[tag])
    {

        blockbytag[tag] = block;
        block->next = block->prev = block;

    }

    else
    {

        blockbytag[tag]->prev->next = block;
        block->prev = blockbytag[tag]->prev;
        block->next = blockbytag[tag];
        blockbytag[tag]->prev = block;

    }

    block->tag = tag;

}

void *(Z_Calloc)(size_t n1, size_t n2, int tag, void **user)
{

    return (n1 *= n2) ? memset((Z_Malloc)(n1, tag, user), 0, n1) : NULL;

}
