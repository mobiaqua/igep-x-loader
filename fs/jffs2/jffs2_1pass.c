/*
-------------------------------------------------------------------------
 * Filename:      jffs2.c
 * Version:       $Id: jffs2_1pass.c,v 1.7 2002/01/25 01:56:47 nyet Exp $
 * Copyright:     Copyright (C) 2001, Russ Dill
 * Author:        Russ Dill <Russ.Dill@asu.edu>
 * Description:   Module to load kernel from jffs2
 *-----------------------------------------------------------------------*/
/*
 * some portions of this code are taken from jffs2, and as such, the
 * following copyright notice is included.
 *
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: jffs2_1pass.c,v 1.7 2002/01/25 01:56:47 nyet Exp $
 *
 */

/* Ok, so anyone who knows the jffs2 code will probably want to get a papar
 * bag to throw up into before reading this code. I looked through the jffs2
 * code, the caching scheme is very elegant. I tried to keep the version
 * for a bootloader as small and simple as possible. Instead of worring about
 * unneccesary data copies, node scans, etc, I just optimized for the known
 * common case, a kernel, which looks like:
 *	(1) most pages are 4096 bytes
 *	(2) version numbers are somewhat sorted in acsending order
 *	(3) multiple compressed blocks making up one page is uncommon
 *
 * So I create a linked list of decending version numbers (insertions at the
 * head), and then for each page, walk down the list, until a matching page
 * with 4096 bytes is found, and then decompress the watching pages in
 * reverse order.
 *
 */

/*
 * Adapted by Nye Liu <nyet@zumanetworks.com> and
 * Rex Feany <rfeany@zumanetworks.com>
 * on Jan/2002 for U-Boot.
 *
 * Clipped out all the non-1pass functions, cleaned up warnings,
 * wrappers, etc. No major changes to the code.
 * Please, he really means it when he said have a paper bag
 * handy. We needed it ;).
 *
 */

/*
 * Bugfixing by Kai-Uwe Bloem <kai-uwe.bloem@auerswald.de>, (C) Mar/2003
 *
 * - overhaul of the memory management. Removed much of the "paper-bagging"
 *   in that part of the code, fixed several bugs, now frees memory when
 *   partition is changed.
 *   It's still ugly :-(
 * - fixed a bug in jffs2_1pass_read_inode where the file length calculation
 *   was incorrect. Removed a bit of the paper-bagging as well.
 * - removed double crc calculation for fragment headers in jffs2_private.h
 *   for speedup.
 * - scan_empty rewritten in a more "standard" manner (non-paperbag, that is).
 * - spinning wheel now spins depending on how much memory has been scanned
 * - lots of small changes all over the place to "improve" readability.
 * - implemented fragment sorting to ensure that the newest data is copied
 *   if there are multiple copies of fragments for a certain file offset.
 *
 * The fragment sorting feature must be enabled by CONFIG_SYS_JFFS2_SORT_FRAGMENTS.
 * Sorting is done while adding fragments to the lists, which is more or less a
 * bubble sort. This takes a lot of time, and is most probably not an issue if
 * the boot filesystem is always mounted readonly.
 *
 * You should define it if the boot filesystem is mounted writable, and updates
 * to the boot files are done by copying files to that filesystem.
 *
 *
 * There's a big issue left: endianess is completely ignored in this code. Duh!
 *
 *
 * You still should have paper bags at hand :-(. The code lacks more or less
 * any comment, and is still arcane and difficult to read in places. As this
 * might be incompatible with any new code from the jffs2 maintainers anyway,
 * it should probably be dumped and replaced by something like jffs2reader!
 */

#define CONFIG_JFFS2_NAND
#define CONFIG_SYS_JFFS2_SORT_FRAGMENTS

#include <common.h>
#include <config.h>
#ifdef IGEP00X_ENABLE_FLASH_BOOT
#include <malloc.h>
#include <linux/stat.h>
#include <linux/time.h>
#include <jffs2/jffs2.h>
#include <jffs2/jffs2_1pass.h>
#include <linux/mtd/compat.h>
#include <asm/errno.h>
#include <linux/mtd/mtd.h>
#include <onenand_uboot.h>
#include <nand_uboot.h>
#include "jffs2_private.h"

#define	NODE_CHUNK	1024	/* size of memory allocation chunk in b_nodes */
#define	SPIN_BLKSIZE	18	/* spin after having scanned 1<<BLKSIZE bytes */

/* Debugging switches */
#undef DEBUG_DIRENTS		/* print directory entry list after scan */
#undef DEBUG_FRAGMENTS		/* print fragment list after scan */
#undef DEBUG			/* enable debugging messages */


#ifdef  DEBUG
# define DEBUGF(fmt,args...)	printf(fmt ,##args)
#else
# define DEBUGF(fmt,args...)
#endif

#include "summary.h"

/* keeps pointer to currently processed partition */
static struct part_info *current_part;



#if (defined(CONFIG_JFFS2_NAND) && \
     defined(CONFIG_CMD_NAND) )
/*
 * Support for jffs2 on top of NAND-flash
 *
 * NAND memory isn't mapped in processor's address space,
 * so data should be fetched from flash before
 * being processed. This is exactly what functions declared
 * here do.
 *
 */

#define NAND_PAGE_SIZE 2048
#define NAND_PAGE_SHIFT 11
#define NAND_PAGE_MASK (~(NAND_PAGE_SIZE-1))

#ifndef NAND_CACHE_PAGES
#define NAND_CACHE_PAGES 4
#endif
#define NAND_CACHE_SIZE (NAND_CACHE_PAGES*NAND_PAGE_SIZE)

#define CACHE_BLOCK_SIZE    (2 * 1024)

static u8* nand_cache = NULL;
static u32 nand_cache_off = (u32)-1;

typedef enum Nand_read_st {
    n_sync_prepare,
    n_read,
    /* Next Configuratuion */
    nr_init,
    nr_wait,
    nr_next

}Nand_read_st;

static Nand_read_st r_state = n_sync_prepare;

extern void* __async_open_read_page (struct mtd_info *mtd, loff_t from);
extern int __async_read_is_ready (void* handle);
extern int __async_read_now (void* handle, u8* data);
extern void __async_read_next (void* handle);
extern void __async_close_read_page (void* handle);

extern void __async_dma_copy_done (void* handle);
extern int __async_dma_read_next (void* handle, u8* prevData, u8* nextdata);

static int load_memory_cache ()
{
    // Read Nand Handle
    void* dhandle = NULL;
    // Index Block
    int actual_block = 0;
    // Get Number Blocks to Read
    u32 n_blocks = CONFIG_JFFS2_PART_SIZE / CACHE_BLOCK_SIZE;
    // Block Processing flag
    int ProcBlock = 1;
    // Get Memory Cache
    nand_cache = malloc (CONFIG_JFFS2_PART_SIZE);

    for(actual_block = 0; actual_block < n_blocks; actual_block++){
        ProcBlock = 1;
        while(ProcBlock){
            switch(r_state){
                case n_sync_prepare:
                case nr_init:
                    // Open first block
                    // printf("jffs2 -> Open: %d\n", actual_block);
                    dhandle = __async_open_read_page(mtd_info, CONFIG_JFFS2_PART_OFFSET +  (actual_block * CACHE_BLOCK_SIZE));
                    __async_dma_read_next (dhandle, NULL, nand_cache + (actual_block * CACHE_BLOCK_SIZE));
                    actual_block--;
                    r_state = nr_wait;
                    break;
                case nr_wait:
                    // Wait
                    __async_dma_copy_done(dhandle);
                    ProcBlock = 0;
                    r_state = nr_next;
                    break;
                case nr_next:
                    if((actual_block+1) >= n_blocks){
                        //printf("jffs2 -> Process ECC for last block: %d\n", actual_block);
                        __async_dma_read_next (dhandle, nand_cache + (actual_block * CACHE_BLOCK_SIZE), NULL);
                    } else{
                        if( ! ((actual_block+1) % 256) ){
                            //printf("jffs2 -> Change Window: %d: Process %d\n", actual_block+1, actual_block);
                            __async_dma_read_next (dhandle, nand_cache + (actual_block * CACHE_BLOCK_SIZE), NULL);
                            __async_close_read_page(dhandle);
                            r_state = nr_init;
                            ProcBlock = 0;
                            break;
                        }
                        else{
                            // printf("jffs2 -> Read Next %d: Process %d\n", actual_block+1, actual_block);
                            __async_dma_read_next (dhandle, nand_cache + (actual_block * CACHE_BLOCK_SIZE), nand_cache + ((actual_block + 1) * CACHE_BLOCK_SIZE));
                        }
                    }
                    r_state = nr_wait;
                    break;
            }
        }
    }
    return 0;
}

static int read_nand_cached (u32 off, u32 size, u_char *buf)
{
	u32 i = 0;
	size_t retlen;
	void* dhandle = NULL;
	u32 n_blocks = CONFIG_JFFS2_PART_SIZE / CACHE_BLOCK_SIZE;
	if(!nand_cache){
		// nand_cache = malloc (CONFIG_JFFS2_PART_SIZE);
#ifdef __DEBUG__
		serial_getc();
		set_time_mark_start();
#endif
        load_memory_cache();
#ifdef __notdef
		for(i = 0; i < n_blocks; i++){
#ifdef __notdef
			nand_read(mtd_info, CONFIG_JFFS2_PART_OFFSET + (i * CACHE_BLOCK_SIZE), CACHE_BLOCK_SIZE, &retlen, nand_cache + (i * CACHE_BLOCK_SIZE));
			if(retlen != CACHE_BLOCK_SIZE){
			    return -1;
				// printf("BUG() read_nand return = %u != 4096\n", retlen);
			}
#else
			switch(r_state){
                case n_sync_prepare:
                    //printf("1\n");
                    dhandle = __async_open_read_page(mtd_info, CONFIG_JFFS2_PART_OFFSET + (i * CACHE_BLOCK_SIZE));
                    // while(!__async_read_is_ready());
                    __async_read_next(dhandle);
                    // while(!__async_read_is_ready(dhandle));
                    __async_read_now(dhandle ,nand_cache + (i * CACHE_BLOCK_SIZE));
                    __async_read_next(dhandle);
                    r_state = n_read;
                    break;
                case n_read:
                    // while(!__async_read_is_ready(dhandle));
                    __async_read_now(dhandle, nand_cache + (i * CACHE_BLOCK_SIZE));
                    if( (i+1) % 256 )
                        __async_read_next(dhandle);
                    else{
                        __async_close_read_page(dhandle);
                        r_state = n_sync_prepare;
                        // while(!__async_read_is_ready());
                    }
                    break;
			}
#endif
		}
#endif
#ifdef __DEBUG__
		set_time_mark_end();
		printf("load time: %d\n", get_mark_elapsed_time());
#endif
#ifdef __DEBUG_MEMORY_TEST
		printf("Read Memory complete %08x\n", crc32(0, nand_cache, CONFIG_JFFS2_PART_SIZE));
#endif
	}
	if(off < CONFIG_JFFS2_PART_OFFSET){
		// printf("BUG() - off (%x) < CONFIG_JFFS2_PART_OFFSET\n", off);
		return -1;
	}
	if(off > (CONFIG_JFFS2_PART_SIZE + CONFIG_JFFS2_PART_OFFSET)){
		// printf("BUG() - off (%x) > CONFIG_JFFS2_PART_SIZE(%x)\n", off, CONFIG_JFFS2_PART_SIZE);
		return -1;
	}
	if(!buf){
		// printf("BUG() - NULL buffer pointer\n");
		return -1;
	}
	memcpy(buf, &(nand_cache[off - CONFIG_JFFS2_PART_OFFSET]), size);
	return size;

}

#ifdef __notdef
static int read_nand_cached (u32 off, u32 size, u_char *buf)
{
	u32 i = 0;
	size_t retlen;
	u32 n_blocks = CONFIG_JFFS2_PART_SIZE / CACHE_BLOCK_SIZE;
	if(!nand_cache){
		nand_cache = malloc (CONFIG_JFFS2_PART_SIZE);
		for(i = 0; i < n_blocks; i++){
			nand_read(mtd_info, CONFIG_JFFS2_PART_OFFSET + (i * CACHE_BLOCK_SIZE), CACHE_BLOCK_SIZE, &retlen, nand_cache + (i * CACHE_BLOCK_SIZE));
			if(retlen != CACHE_BLOCK_SIZE){
			    return -1;
				// printf("BUG() read_nand return = %u != 4096\n", retlen);
			}
		}
#ifdef __DEBUG_MEMORY_TEST
		printf("Read Memory complete %08x\n", crc32(0, nand_cache, CONFIG_JFFS2_PART_SIZE));
#endif
	}
	if(off < CONFIG_JFFS2_PART_OFFSET){
		// printf("BUG() - off (%x) < CONFIG_JFFS2_PART_OFFSET\n", off);
		return -1;
	}
	if(off > (CONFIG_JFFS2_PART_SIZE + CONFIG_JFFS2_PART_OFFSET)){
		// printf("BUG() - off (%x) > CONFIG_JFFS2_PART_SIZE(%x)\n", off, CONFIG_JFFS2_PART_SIZE);
		return -1;
	}
	if(!buf){
		// printf("BUG() - NULL buffer pointer\n");
		return -1;
	}
	memcpy(buf, &(nand_cache[off - CONFIG_JFFS2_PART_OFFSET]), size);
	return size;
}
#endif

static void *get_fl_mem_nand(u32 off, u32 size, void *ext_buf)
{
	u_char *buf = ext_buf ? (u_char*)ext_buf : (u_char*)malloc(size);

	if (NULL == buf) {
#ifdef __DEBUG__
		printf("get_fl_mem_nand: can't alloc %d bytes\n", size);
#endif
		return NULL;
	}
	if (read_nand_cached(off, size, buf) < 0) {
		if (!ext_buf)
			free(buf);
		return NULL;
	}

	return buf;
}

static void *get_node_mem_nand(u32 off, void *ext_buf)
{
	struct jffs2_unknown_node node;
	void *ret = NULL;

	if (NULL == get_fl_mem_nand(off, sizeof(node), &node))
		return NULL;

	if (!(ret = get_fl_mem_nand(off, node.magic ==
			       JFFS2_MAGIC_BITMASK ? node.totlen : sizeof(node),
			       ext_buf))) {
#ifdef __DEBUG__
		printf("off = %#x magic %#x type %#x node.totlen = %d\n",
		       off, node.magic, node.nodetype, node.totlen);
#endif
	}
	return ret;
}

static void put_fl_mem_nand(void *buf)
{
	free(buf);
}
#endif

#if defined(CONFIG_CMD_ONENAND)

#include <linux/mtd/onenand.h>

#define ONENAND_PAGE_SIZE 2048
#define ONENAND_PAGE_SHIFT 11
#define ONENAND_PAGE_MASK (~(ONENAND_PAGE_SIZE-1))

#ifndef ONENAND_CACHE_PAGES
#define ONENAND_CACHE_PAGES 4
#endif
#define ONENAND_CACHE_SIZE (ONENAND_CACHE_PAGES*ONENAND_PAGE_SIZE)

static u8* onenand_cache;
static u32 onenand_cache_off = (u32)-1;

static int read_onenand_cached (u32 off, u32 size, u_char *buf)
{
	u32 i = 0;
	size_t retlen;
	u32 n_blocks = CONFIG_JFFS2_PART_SIZE / 4096;
	if(!onenand_cache){
		onenand_cache = malloc (CONFIG_JFFS2_PART_SIZE);
		for(i = 0; i < n_blocks; i++){
			onenand_read(mtd_info, CONFIG_JFFS2_PART_OFFSET + (i * 4096), 4096, &retlen, onenand_cache + (i * 4096));
			if(retlen != 4096){
				// printf("BUG() onenand_read return = %u != 4096\n", retlen);
				return -1;
			}
		}
#ifdef __DEBUG_MEMORY_TEST
		printf("Read Memory complete %08x\n", crc32(0, onenand_cache, CONFIG_JFFS2_PART_SIZE));
#endif
	}
	if(off < CONFIG_JFFS2_PART_OFFSET){
		// printf("BUG() - off (%x) < CONFIG_JFFS2_PART_OFFSET\n", off);
		return 1;
	}
	if(off > (CONFIG_JFFS2_PART_SIZE + CONFIG_JFFS2_PART_OFFSET)){
		// printf("BUG() - off (%x) > CONFIG_JFFS2_PART_SIZE(%x)\n", off, CONFIG_JFFS2_PART_SIZE);
		return -1;
	}
	if(!buf){
		// printf("BUG() - NULL buffer pointer\n");
		return -1;
	}
	memcpy(buf, &(onenand_cache[off - CONFIG_JFFS2_PART_OFFSET]), size);
	return size;
}

static void *get_fl_mem_onenand(u32 off, u32 size, void *ext_buf)
{
	u_char *buf = ext_buf ? (u_char *)ext_buf : (u_char *)malloc(size);

	if (NULL == buf) {
#if 0
		printf("get_fl_mem_onenand: can't alloc %d bytes\n", size);
#endif
		return NULL;
	}
	if (read_onenand_cached(off, size, buf) < 0) {
		if (!ext_buf)
			free(buf);
		return NULL;
	}

	return buf;
}

static void *get_node_mem_onenand(u32 off, void *ext_buf)
{
	struct jffs2_unknown_node node;
	void *ret = NULL;

	if (NULL == get_fl_mem_onenand(off, sizeof(node), &node))
		return NULL;

	ret = get_fl_mem_onenand(off, node.magic ==
			JFFS2_MAGIC_BITMASK ? node.totlen : sizeof(node),
			ext_buf);
	if (!ret) {
#if 0
		printf("off = %#x magic %#x type %#x node.totlen = %d\n",
		       off, node.magic, node.nodetype, node.totlen);
#endif
	}
	return ret;
}


static void put_fl_mem_onenand(void *buf)
{
	free(buf);
}
#endif


#if defined(CONFIG_CMD_FLASH)
/*
 * Support for jffs2 on top of NOR-flash
 *
 * NOR flash memory is mapped in processor's address space,
 * just return address.
 */

#include <linux/mtd/nand.h>

static inline void *get_fl_mem_nor(u32 off, u32 size, void *ext_buf)
{
	u32 addr = off;
	struct mtdids *id = current_part->dev->id;

	extern flash_info_t flash_info[];
	flash_info_t *flash = &flash_info[id->num];

	addr += flash->start[0];
	if (ext_buf) {
		memcpy(ext_buf, (void *)addr, size);
		return ext_buf;
	}
	return (void*)addr;
}

static inline void *get_node_mem_nor(u32 off, void *ext_buf)
{
	struct jffs2_unknown_node *pNode;

	/* pNode will point directly to flash - don't provide external buffer
	   and don't care about size */
	pNode = get_fl_mem_nor(off, 0, NULL);
	return (void *)get_fl_mem_nor(off, pNode->magic == JFFS2_MAGIC_BITMASK ?
			pNode->totlen : sizeof(*pNode), ext_buf);
}
#endif


/*
 * Generic jffs2 raw memory and node read routines.
 *
 */
static inline void *get_fl_mem(u32 off, u32 size, void *ext_buf)
{
	struct mtdids *id = current_part->dev->id;

	switch(id->type) {
#if defined(CONFIG_CMD_FLASH)
	case MTD_DEV_TYPE_NOR:
		return get_fl_mem_nor(off, size, ext_buf);
		break;
#endif
#if defined(CONFIG_JFFS2_NAND) && defined(CONFIG_CMD_NAND)
	case MTD_DEV_TYPE_NAND:
		return get_fl_mem_nand(off, size, ext_buf);
		break;
#endif
#if defined(CONFIG_CMD_ONENAND)
	case MTD_DEV_TYPE_ONENAND:
		return get_fl_mem_onenand(off, size, ext_buf);
		break;
#endif
	default:
#if 0
		printf("get_fl_mem: unknown device type, " \
			"using raw offset!\n");
#else
        break;
#endif
	}
	return (void*)off;
}

static inline void *get_node_mem(u32 off, void *ext_buf)
{
	struct mtdids *id = current_part->dev->id;

	switch(id->type) {
#if defined(CONFIG_CMD_FLASH)
	case MTD_DEV_TYPE_NOR:
		return get_node_mem_nor(off, ext_buf);
		break;
#endif
#if defined(CONFIG_JFFS2_NAND) && \
    defined(CONFIG_CMD_NAND)
	case MTD_DEV_TYPE_NAND:
		return get_node_mem_nand(off, ext_buf);
		break;
#endif
#if defined(CONFIG_CMD_ONENAND)
	case MTD_DEV_TYPE_ONENAND:
		return get_node_mem_onenand(off, ext_buf);
		break;
#endif
	default:
#if 0
		printf("get_fl_mem: unknown device type, " \
			"using raw offset!\n");
#else
        break;
#endif
	}
	return (void*)off;
}

static inline void put_fl_mem(void *buf, void *ext_buf)
{
	struct mtdids *id = current_part->dev->id;

	/* If buf is the same as ext_buf, it was provided by the caller -
	   we shouldn't free it then. */
	if (buf == ext_buf)
		return;
	switch (id->type) {
#if defined(CONFIG_JFFS2_NAND) && defined(CONFIG_CMD_NAND)
	case MTD_DEV_TYPE_NAND:
		return put_fl_mem_nand(buf);
#endif
#if defined(CONFIG_CMD_ONENAND)
	case MTD_DEV_TYPE_ONENAND:
		return put_fl_mem_onenand(buf);
#endif
	}
}

/* Compression names */
static char *compr_names[] = {
	"NONE",
	"ZERO",
	"RTIME",
	"RUBINMIPS",
	"COPY",
	"DYNRUBIN",
	"ZLIB",
// #if defined(CONFIG_JFFS2_LZO)
	"LZO",
// #endif
};

/* Memory management */
struct mem_block {
	u32	index;
	struct mem_block *next;
	struct b_node nodes[NODE_CHUNK];
};


static void free_nodes(struct b_list *list)
{
	while (list->listMemBase != NULL) {
		struct mem_block *next = list->listMemBase->next;
		free( list->listMemBase );
		list->listMemBase = next;
	}
}

static struct b_node* add_node(struct b_list *list)
{
	u32 index = 0;
	struct mem_block *memBase;
	struct b_node *b;

	memBase = list->listMemBase;
	if (memBase != NULL)
		index = memBase->index;
#if 0
	putLabeledWord("add_node: index = ", index);
	putLabeledWord("add_node: memBase = ", list->listMemBase);
#endif

	if (memBase == NULL || index >= NODE_CHUNK) {
		/* we need more space before we continue */
		memBase = mmalloc(sizeof(struct mem_block));
		if (memBase == NULL) {
#if 0
			putstr("add_node: malloc failed\n");
#endif
			return NULL;
		}
		memBase->next = list->listMemBase;
		index = 0;
#if 0
		putLabeledWord("add_node: alloced a new membase at ", *memBase);
#endif

	}
	/* now we have room to add it. */
	b = &memBase->nodes[index];
	index ++;

	memBase->index = index;
	list->listMemBase = memBase;
	list->listCount++;
	return b;
}

static struct b_node* insert_node(struct b_list *list, u32 offset)
{
	struct b_node *new;
#ifdef CONFIG_SYS_JFFS2_SORT_FRAGMENTS
	struct b_node *b, *prev;
#endif

	if (!(new = add_node(list))) {
#if 0
		putstr("add_node failed!\r\n");
#endif
		return NULL;
	}
	new->offset = offset;

	// printf("insert_node: %x\n", offset);

#ifdef CONFIG_SYS_JFFS2_SORT_FRAGMENTS
	if (list->listTail != NULL && list->listCompare(new, list->listTail))
		prev = list->listTail;
	else if (list->listLast != NULL && list->listCompare(new, list->listLast))
		prev = list->listLast;
	else
		prev = NULL;

	for (b = (prev ? prev->next : list->listHead);
	     b != NULL && list->listCompare(new, b);
	     prev = b, b = b->next) {
		list->listLoops++;
	}
	if (b != NULL)
		list->listLast = prev;

	if (b != NULL) {
		new->next = b;
		if (prev != NULL)
			prev->next = new;
		else
			list->listHead = new;
	} else
#endif
	{
		new->next = (struct b_node *) NULL;
		if (list->listTail != NULL) {
			list->listTail->next = new;
			list->listTail = new;
		} else {
			list->listTail = list->listHead = new;
		}
	}

	return new;
}

#ifdef CONFIG_SYS_JFFS2_SORT_FRAGMENTS
/* Sort data entries with the latest version last, so that if there
 * is overlapping data the latest version will be used.
 */
static int compare_inodes(struct b_node *new, struct b_node *old)
{
	struct jffs2_raw_inode ojNew;
	struct jffs2_raw_inode ojOld;
	struct jffs2_raw_inode *jNew =
		(struct jffs2_raw_inode *)get_fl_mem(new->offset, sizeof(ojNew), &ojNew);
	struct jffs2_raw_inode *jOld =
		(struct jffs2_raw_inode *)get_fl_mem(old->offset, sizeof(ojOld), &ojOld);

	return jNew->version > jOld->version;
}

/* Sort directory entries so all entries in the same directory
 * with the same name are grouped together, with the latest version
 * last. This makes it easy to eliminate all but the latest version
 * by marking the previous version dead by setting the inode to 0.
 */
static int compare_dirents(struct b_node *new, struct b_node *old)
{
	struct jffs2_raw_dirent ojNew;
	struct jffs2_raw_dirent ojOld;
	struct jffs2_raw_dirent *jNew =
		(struct jffs2_raw_dirent *)get_fl_mem(new->offset, sizeof(ojNew), &ojNew);
	struct jffs2_raw_dirent *jOld =
		(struct jffs2_raw_dirent *)get_fl_mem(old->offset, sizeof(ojOld), &ojOld);
	int cmp;

	/* ascending sort by pino */
	if (jNew->pino != jOld->pino)
		return jNew->pino > jOld->pino;

	/* pino is the same, so use ascending sort by nsize, so
	 * we don't do strncmp unless we really must.
	 */
	if (jNew->nsize != jOld->nsize)
		return jNew->nsize > jOld->nsize;

	/* length is also the same, so use ascending sort by name
	 */
	cmp = strncmp((char *)jNew->name, (char *)jOld->name, jNew->nsize);
	if (cmp != 0)
		return cmp > 0;

	/* we have duplicate names in this directory, so use ascending
	 * sort by version
	 */
	if (jNew->version > jOld->version) {
		/* since jNew is newer, we know jOld is not valid, so
		 * mark it with inode 0 and it will not be used
		 */
		jOld->ino = 0;
		return 1;
	}

	return 0;
}
#endif

void
jffs2_free_cache(struct part_info *part)
{
	struct b_lists *pL;

	if (part->jffs2_priv != NULL) {
		pL = (struct b_lists *)part->jffs2_priv;
		free_nodes(&pL->frag);
		free_nodes(&pL->dir);
		free(pL->readbuf);
		free(pL);
	}
}

static u32
jffs_init_1pass_list(struct part_info *part)
{
	struct b_lists *pL;

	jffs2_free_cache(part);

	if (NULL != (part->jffs2_priv = malloc(sizeof(struct b_lists)))) {
		pL = (struct b_lists *)part->jffs2_priv;

		memset(pL, 0, sizeof(*pL));
#ifdef CONFIG_SYS_JFFS2_SORT_FRAGMENTS
		pL->dir.listCompare = compare_dirents;
		pL->frag.listCompare = compare_inodes;
#endif
	}
	return 0;
}

/* find the inode from the slashless name given a parent */
static long
jffs2_1pass_read_inode(struct b_lists *pL, u32 inode, char *dest)
{
	struct b_node *b;
	struct jffs2_raw_inode *jNode;
	u32 totalSize = 0;
	u32 latestVersion = 0;
	uchar *lDest;
	uchar *src;
	long ret;
	int i;
	u32 counter = 0;
#ifdef CONFIG_SYS_JFFS2_SORT_FRAGMENTS
	/* Find file size before loading any data, so fragments that
	 * start past the end of file can be ignored. A fragment
	 * that is partially in the file is loaded, so extra data may
	 * be loaded up to the next 4K boundary above the file size.
	 * This shouldn't cause trouble when loading kernel images, so
	 * we will live with it.
	 */
	for (b = pL->frag.listHead; b != NULL; b = b->next) {
		jNode = (struct jffs2_raw_inode *) get_fl_mem(b->offset,
			sizeof(struct jffs2_raw_inode), pL->readbuf);
		if ((inode == jNode->ino)) {
			/* get actual file length from the newest node */
			if (jNode->version >= latestVersion) {
				totalSize = jNode->isize;
				latestVersion = jNode->version;
			}
		}
		put_fl_mem(jNode, pL->readbuf);
	}
#endif
	for (b = pL->frag.listHead; b != NULL; b = b->next) {
		jNode = (struct jffs2_raw_inode *) get_node_mem(b->offset,
								pL->readbuf);
		if ((inode == jNode->ino)) {
#if 0
			printf("\n\nread_inode: totlen = %u ", jNode->totlen);
			printf("read_inode: inode = %u ", jNode->ino);
			printf("read_inode: version = %u ", jNode->version);
			printf("read_inode: isize = %u ", jNode->isize);
			printf("read_inode: offset = %x ", jNode->offset);
			printf("read_inode: csize = %u ", jNode->csize);
			printf("read_inode: dsize = %u\n", jNode->dsize);
//			printf("read_inode: compr = ", jNode->compr);
//			printf("read_inode: usercompr = ", jNode->usercompr);
//			printf("read_inode: flags = ", jNode->flags);
#endif

#ifndef CONFIG_SYS_JFFS2_SORT_FRAGMENTS
			/* get actual file length from the newest node */
			if (jNode->version >= latestVersion) {
				totalSize = jNode->isize;
				latestVersion = jNode->version;
			}
#endif
			// printf("insert_node: %x\n", b->offset);
			if(dest) {
				src = ((uchar *) jNode) + sizeof(struct jffs2_raw_inode);
				/* ignore data behind latest known EOF */
				if (jNode->offset > totalSize) {
					put_fl_mem(jNode, pL->readbuf);
					continue;
				}
				if (b->datacrc == CRC_UNKNOWN)
					b->datacrc = data_crc(jNode) ?
						CRC_OK : CRC_BAD;
				if (b->datacrc == CRC_BAD) {
					put_fl_mem(jNode, pL->readbuf);
					continue;
				}

				lDest = (uchar *) (dest + jNode->offset);
#if 0
				putLabeledWord("read_inode: src = ", src);
				putLabeledWord("read_inode: dest = ", lDest);
#endif
				switch (jNode->compr) {
				case JFFS2_COMPR_NONE:
					ret = (unsigned long) ldr_memcpy(lDest, src, jNode->dsize);
					break;
				case JFFS2_COMPR_ZERO:
					ret = 0;
					for (i = 0; i < jNode->dsize; i++)
						*(lDest++) = 0;
					break;
				case JFFS2_COMPR_RTIME:
					ret = 0;
					// rtime_decompress(src, lDest, jNode->csize, jNode->dsize);
					break;
				case JFFS2_COMPR_DYNRUBIN:
					/* this is slow but it works */
					ret = 0;
					// dynrubin_decompress(src, lDest, jNode->csize, jNode->dsize);
					break;
				case JFFS2_COMPR_ZLIB:
					ret = zlib_decompress(src, lDest, jNode->csize, jNode->dsize);
					break;
//#if defined(CONFIG_JFFS2_LZO)
				case JFFS2_COMPR_LZO:
					ret = lzo_decompress(src, lDest, jNode->csize, jNode->dsize);
					break;
//#endif
				default:
					/* unknown */
					putLabeledWord("UNKOWN COMPRESSION METHOD = ", jNode->compr);
					put_fl_mem(jNode, pL->readbuf);
					return -1;
					break;
				}
			}

#if 0
			putLabeledWord("read_inode: totalSize = ", totalSize);
			putLabeledWord("read_inode: compr ret = ", ret);
#endif
		}
		counter++;
		put_fl_mem(jNode, pL->readbuf);
	}

#if 0
	putLabeledWord("read_inode: returning = ", totalSize);
#endif
	return totalSize;
}

/* find the inode from the slashless name given a parent */
static u32
jffs2_1pass_find_inode(struct b_lists * pL, const char *name, u32 pino)
{
	struct b_node *b;
	struct jffs2_raw_dirent *jDir;
	int len;
	u32 counter;
	u32 version = 0;
	u32 inode = 0;

	/* name is assumed slash free */
	len = strlen(name);

	counter = 0;
	/* we need to search all and return the inode with the highest version */
	for(b = pL->dir.listHead; b; b = b->next, counter++) {
		jDir = (struct jffs2_raw_dirent *) get_node_mem(b->offset, pL->readbuf);
		// printf("%s\n", (char *)jDir->name);
		if ((pino == jDir->pino) && (len == jDir->nsize) &&
		    (jDir->ino) &&	/* 0 for unlink */
		    (!strncmp((char *)jDir->name, name, len))) {	/* a match */
			if (jDir->version < version) {
				put_fl_mem(jDir, pL->readbuf);
				continue;
			}

			if (jDir->version == version && inode != 0) {
				/* I'm pretty sure this isn't legal */
#if 0
				putstr(" ** ERROR ** ");
				putnstr(jDir->name, jDir->nsize);
				putLabeledWord(" has dup version =", version);
#endif
			}
			inode = jDir->ino;
			version = jDir->version;
		}
#if 0
		putstr("\r\nfind_inode:p&l ->");
		putnstr(jDir->name, jDir->nsize);
		putstr("\r\n");
		putLabeledWord("pino = ", jDir->pino);
		putLabeledWord("nsize = ", jDir->nsize);
		putLabeledWord("b = ", (u32) b);
		putLabeledWord("counter = ", counter);
#endif
		put_fl_mem(jDir, pL->readbuf);
	}
	return inode;
}

static u32
jffs2_1pass_search_inode(struct b_lists * pL, const char *fname, u32 pino)
{
	int i;
	char tmp[256];
	char working_tmp[256];
	char *c;

	/* discard any leading slash */
	i = 0;
	while (fname[i] == '/')
		i++;
	strcpy(tmp, &fname[i]);

	while ((c = (char *) strchr(tmp, '/')))	/* we are still dired searching */
	{
		strncpy(working_tmp, tmp, c - tmp);
		working_tmp[c - tmp] = '\0';
#if 0
		putstr("search_inode: tmp = ");
		putstr(tmp);
		putstr("\r\n");
		putstr("search_inode: wtmp = ");
		putstr(working_tmp);
		putstr("\r\n");
		putstr("search_inode: c = ");
		putstr(c);
		putstr("\r\n");
#endif
		for (i = 0; i < strlen(c) - 1; i++)
			tmp[i] = c[i + 1];
		tmp[i] = '\0';
#if 0
		putstr("search_inode: post tmp = ");
		putstr(tmp);
		putstr("\r\n");
#endif

		if (!(pino = jffs2_1pass_find_inode(pL, working_tmp, pino))) {
#if 0
			putstr("find_inode failed for name=");
			putstr(working_tmp);
			putstr("\r\n");
#endif
			return 0;
		}
	}
	/* this is for the bare filename, directories have already been mapped */
	if (!(pino = jffs2_1pass_find_inode(pL, tmp, pino))) {
#if 0
		putstr("find_inode failed for name=");
		putstr(tmp);
		putstr("\r\n");
#endif
		return 0;
	}
	return pino;

}

static u32
jffs2_1pass_resolve_inode(struct b_lists * pL, u32 ino)
{
	struct b_node *b;
	struct b_node *b2;
	struct jffs2_raw_dirent *jDir;
	struct jffs2_raw_inode *jNode;
	u8 jDirFoundType = 0;
	u32 jDirFoundIno = 0;
	u32 jDirFoundPino = 0;
	char tmp[256];
	u32 version = 0;
	u32 pino;
	unsigned char *src;

	/* we need to search all and return the inode with the highest version */
	for(b = pL->dir.listHead; b; b = b->next) {
		jDir = (struct jffs2_raw_dirent *) get_node_mem(b->offset,
								pL->readbuf);
		if (ino == jDir->ino) {
			if (jDir->version < version) {
				put_fl_mem(jDir, pL->readbuf);
				continue;
			}

			if (jDir->version == version && jDirFoundType) {
				/* I'm pretty sure this isn't legal */
				putstr(" ** ERROR ** ");
				putnstr(jDir->name, jDir->nsize);
				putLabeledWord(" has dup version (resolve) = ",
					version);
			}

			jDirFoundType = jDir->type;
			jDirFoundIno = jDir->ino;
			jDirFoundPino = jDir->pino;
			version = jDir->version;
		}
		put_fl_mem(jDir, pL->readbuf);
	}
	/* now we found the right entry again. (shoulda returned inode*) */
	if (jDirFoundType != DT_LNK)
		return jDirFoundIno;

	/* it's a soft link so we follow it again. */
	b2 = pL->frag.listHead;
	while (b2) {
		jNode = (struct jffs2_raw_inode *) get_node_mem(b2->offset,
								pL->readbuf);
		if (jNode->ino == jDirFoundIno) {
			src = (unsigned char *)jNode + sizeof(struct jffs2_raw_inode);

#if 0
			putLabeledWord("\t\t dsize = ", jNode->dsize);
			putstr("\t\t target = ");
			putnstr(src, jNode->dsize);
			putstr("\r\n");
#endif
			strncpy(tmp, (char *)src, jNode->dsize);
			tmp[jNode->dsize] = '\0';
			put_fl_mem(jNode, pL->readbuf);
			break;
		}
		b2 = b2->next;
		put_fl_mem(jNode, pL->readbuf);
	}
	/* ok so the name of the new file to find is in tmp */
	/* if it starts with a slash it is root based else shared dirs */
	if (tmp[0] == '/')
		pino = 1;
	else
		pino = jDirFoundPino;

	return jffs2_1pass_search_inode(pL, tmp, pino);
}

unsigned char
jffs2_1pass_rescan_needed(struct part_info *part)
{
	struct b_node *b;
	struct jffs2_unknown_node onode;
	struct jffs2_unknown_node *node;
	struct b_lists *pL = (struct b_lists *)part->jffs2_priv;

	if (part->jffs2_priv == 0){
		DEBUGF ("rescan: First time in use\n");
		return 1;
	}

	/* if we have no list, we need to rescan */
	if (pL->frag.listCount == 0) {
		DEBUGF ("rescan: fraglist zero\n");
		return 1;
	}

	/* but suppose someone reflashed a partition at the same offset... */
	b = pL->dir.listHead;
	while (b) {
		node = (struct jffs2_unknown_node *) get_fl_mem(b->offset,
			sizeof(onode), &onode);
		if (node->nodetype != JFFS2_NODETYPE_DIRENT) {
			DEBUGF ("rescan: fs changed beneath me? (%lx)\n",
					(unsigned long) b->offset);
			return 1;
		}
		b = b->next;
	}
	return 0;
}

#ifdef DEBUG_FRAGMENTS
static void
dump_fragments(struct b_lists *pL)
{
	struct b_node *b;
	struct jffs2_raw_inode ojNode;
	struct jffs2_raw_inode *jNode;

	putstr("\r\n\r\n******The fragment Entries******\r\n");
	b = pL->frag.listHead;
	while (b) {
		jNode = (struct jffs2_raw_inode *) get_fl_mem(b->offset,
			sizeof(ojNode), &ojNode);
		putLabeledWord("\r\n\tbuild_list: FLASH_OFFSET = ", b->offset);
		putLabeledWord("\tbuild_list: totlen = ", jNode->totlen);
		putLabeledWord("\tbuild_list: inode = ", jNode->ino);
		putLabeledWord("\tbuild_list: version = ", jNode->version);
		putLabeledWord("\tbuild_list: isize = ", jNode->isize);
		putLabeledWord("\tbuild_list: atime = ", jNode->atime);
		putLabeledWord("\tbuild_list: offset = ", jNode->offset);
		putLabeledWord("\tbuild_list: csize = ", jNode->csize);
		putLabeledWord("\tbuild_list: dsize = ", jNode->dsize);
		putLabeledWord("\tbuild_list: compr = ", jNode->compr);
		putLabeledWord("\tbuild_list: usercompr = ", jNode->usercompr);
		putLabeledWord("\tbuild_list: flags = ", jNode->flags);
		putLabeledWord("\tbuild_list: offset = ", b->offset);	/* FIXME: ? [RS] */
		b = b->next;
	}
}
#endif

#ifdef DEBUG_DIRENTS
static void
dump_dirents(struct b_lists *pL)
{
	struct b_node *b;
	struct jffs2_raw_dirent *jDir;

	putstr("\r\n\r\n******The directory Entries******\r\n");
	b = pL->dir.listHead;
	while (b) {
		jDir = (struct jffs2_raw_dirent *) get_node_mem(b->offset,
								pL->readbuf);
		putstr("\r\n");
		putnstr(jDir->name, jDir->nsize);
		putLabeledWord("\r\n\tbuild_list: magic = ", jDir->magic);
		putLabeledWord("\tbuild_list: nodetype = ", jDir->nodetype);
		putLabeledWord("\tbuild_list: hdr_crc = ", jDir->hdr_crc);
		putLabeledWord("\tbuild_list: pino = ", jDir->pino);
		putLabeledWord("\tbuild_list: version = ", jDir->version);
		putLabeledWord("\tbuild_list: ino = ", jDir->ino);
		putLabeledWord("\tbuild_list: mctime = ", jDir->mctime);
		putLabeledWord("\tbuild_list: nsize = ", jDir->nsize);
		putLabeledWord("\tbuild_list: type = ", jDir->type);
		putLabeledWord("\tbuild_list: node_crc = ", jDir->node_crc);
		putLabeledWord("\tbuild_list: name_crc = ", jDir->name_crc);
		putLabeledWord("\tbuild_list: offset = ", b->offset);	/* FIXME: ? [RS] */
		b = b->next;
		put_fl_mem(jDir, pL->readbuf);
	}
}
#endif

#define DEFAULT_EMPTY_SCAN_SIZE	2048

static inline uint32_t EMPTY_SCAN_SIZE(uint32_t sector_size)
{
	if (sector_size < DEFAULT_EMPTY_SCAN_SIZE)
		return sector_size;
	else
		return DEFAULT_EMPTY_SCAN_SIZE;
}

static u32
jffs2_1pass_build_lists(struct part_info * part)
{
	struct b_lists *pL;
	struct jffs2_unknown_node *node;
	u32 nr_sectors = part->size/part->sector_size;
	u32 i;
	u32 counter4 = 0;
	u32 counterF = 0;
	u32 counterN = 0;
	u32 max_totlen = 0;
	u32 buf_size = DEFAULT_EMPTY_SCAN_SIZE;
	char *buf;

	/* turn off the lcd.  Refreshing the lcd adds 50% overhead to the */
	/* jffs2 list building enterprise nope.  in newer versions the overhead is */
	/* only about 5 %.  not enough to inconvenience people for. */
	/* lcd_off(); */

	/* if we are building a list we need to refresh the cache. */
	jffs_init_1pass_list(part);
	pL = (struct b_lists *)part->jffs2_priv;
	buf = malloc(buf_size * 2);

	printf("XLoader: Scan jffs2 partition: ");

	/* start at the beginning of the partition */
	for (i = 0; i < nr_sectors; i++) {
		uint32_t sector_ofs = i * part->sector_size;
		uint32_t buf_ofs = sector_ofs;
		uint32_t buf_len;
		uint32_t ofs, prevofs;

		printf(".");

		buf_len = EMPTY_SCAN_SIZE(part->sector_size);

		get_fl_mem((u32)part->offset + buf_ofs, buf_len, buf);

		/* We temporarily use 'ofs' as a pointer into the buffer/jeb */
		ofs = 0;

		/* Scan only 4KiB of 0xFF before declaring it's empty */
		while (ofs < EMPTY_SCAN_SIZE(part->sector_size) &&
				*(uint32_t *)(&buf[ofs]) == 0xFFFFFFFF)
			ofs += 4;

		if (ofs == EMPTY_SCAN_SIZE(part->sector_size))
			continue;

		ofs += sector_ofs;
		prevofs = ofs - 1;

	scan_more:
		while (ofs < sector_ofs + part->sector_size) {
			if (ofs == prevofs) {
#ifdef __DEBUG__
				printf("offset %08x already seen, skip\n", ofs);
#endif
				ofs += 4;
				counter4++;
				continue;
			}
			prevofs = ofs;
			if (sector_ofs + part->sector_size <
					ofs + sizeof(*node))
				break;
			if (buf_ofs + buf_len < ofs + sizeof(*node)) {
				buf_len = min_t(uint32_t, buf_size, sector_ofs
						+ part->sector_size - ofs);
				get_fl_mem((u32)part->offset + ofs, buf_len,
					   buf);
				buf_ofs = ofs;
			}

			node = (struct jffs2_unknown_node *)&buf[ofs-buf_ofs];

			if (*(uint32_t *)(&buf[ofs-buf_ofs]) == 0xffffffff) {
				uint32_t inbuf_ofs;
				uint32_t empty_start, scan_end;

				empty_start = ofs;
				ofs += 4;
				scan_end = min_t(uint32_t, EMPTY_SCAN_SIZE(
							part->sector_size)/8,
							buf_len);
			more_empty:
				inbuf_ofs = ofs - buf_ofs;
				while (inbuf_ofs < scan_end) {
					if (*(uint32_t *)(&buf[inbuf_ofs]) !=
							0xffffffff)
						goto scan_more;

					inbuf_ofs += 4;
					ofs += 4;
				}
				/* Ran off end. */

				/* See how much more there is to read in this
				 * eraseblock...
				 */
				buf_len = min_t(uint32_t, buf_size,
						sector_ofs +
						part->sector_size - ofs);
				if (!buf_len) {
					/* No more to read. Break out of main
					 * loop without marking this range of
					 * empty space as dirty (because it's
					 * not)
					 */
					break;
				}
				scan_end = buf_len;
				get_fl_mem((u32)part->offset + ofs, buf_len,
					   buf);
				buf_ofs = ofs;
				goto more_empty;
			}
			if (node->magic != JFFS2_MAGIC_BITMASK ||
					!hdr_crc(node)) {
				ofs += 4;
				counter4++;
				continue;
			}
			if (ofs + node->totlen >
					sector_ofs + part->sector_size) {
				ofs += 4;
				counter4++;
				continue;
			}
			/* if its a fragment add it */
			switch (node->nodetype) {
			case JFFS2_NODETYPE_INODE:
				if (buf_ofs + buf_len < ofs + sizeof(struct
							jffs2_raw_inode)) {
					get_fl_mem((u32)part->offset + ofs,
						   buf_len, buf);
					buf_ofs = ofs;
					node = (void *)buf;
				}
				if (!inode_crc((struct jffs2_raw_inode *) node))
				       break;

				if (insert_node(&pL->frag, (u32) part->offset +
						ofs) == NULL) {
					free(buf);
					jffs2_free_cache(part);
					return 0;
				}
				if (max_totlen < node->totlen)
					max_totlen = node->totlen;
				break;
			case JFFS2_NODETYPE_DIRENT:
				if (buf_ofs + buf_len < ofs + sizeof(struct
							jffs2_raw_dirent) +
							((struct
							 jffs2_raw_dirent *)
							node)->nsize) {
					get_fl_mem((u32)part->offset + ofs,
						   buf_len, buf);
					buf_ofs = ofs;
					node = (void *)buf;
				}

				if (!dirent_crc((struct jffs2_raw_dirent *)
							node) ||
						!dirent_name_crc(
							(struct
							 jffs2_raw_dirent *)
							node))
					break;
				// if (! (counterN%100))
					// printf ("\b\b.  ");
				if (insert_node(&pL->dir, (u32) part->offset +
						ofs) == NULL) {
					free(buf);
					jffs2_free_cache(part);
					return 0;
				}
				if (max_totlen < node->totlen)
					max_totlen = node->totlen;
				counterN++;
				break;
			case JFFS2_NODETYPE_CLEANMARKER:
				if (node->totlen != sizeof(struct jffs2_unknown_node))
					printf("OOPS Cleanmarker has bad size "
						"%d != %zu\n",
						node->totlen,
						sizeof(struct jffs2_unknown_node));
				break;
			case JFFS2_NODETYPE_PADDING:
				if (node->totlen < sizeof(struct jffs2_unknown_node))
					printf("OOPS Padding has bad size "
						"%d < %zu\n",
						node->totlen,
						sizeof(struct jffs2_unknown_node));
				break;
			case JFFS2_NODETYPE_SUMMARY:
				break;
			default:
				printf("Unknown node type: %x len %d offset 0x%x\n",
					node->nodetype,
					node->totlen, ofs);
			}
			ofs += ((node->totlen + 3) & ~3);
			counterF++;
		}
	}

	free(buf);
// #ifdef __DEBUG__
	printf("done.\n");		/* close off the dots */
// #endif
	/* We don't care if malloc failed - then each read operation will
	 * allocate its own buffer as necessary (NAND) or will read directly
	 * from flash (NOR).
	 */
	pL->readbuf = malloc(max_totlen);

	/* turn the lcd back on. */
	/* splash(); */

#if 0
	putLabeledWord("dir entries = ", pL->dir.listCount);
	putLabeledWord("frag entries = ", pL->frag.listCount);
	putLabeledWord("+4 increments = ", counter4);
	putLabeledWord("+file_offset increments = ", counterF);

#endif

#ifdef DEBUG_DIRENTS
	dump_dirents(pL);
#endif

#ifdef DEBUG_FRAGMENTS
	dump_fragments(pL);
#endif

	/* give visual feedback that we are done scanning the flash */
	led_blink(0x0, 0x0, 0x1, 0x1);	/* off, forever, on 100ms, off 100ms */
	return 1;
}

static struct b_lists *
jffs2_get_list(struct part_info * part, const char *who)
{
	/* copy requested part_info struct pointer to global location */
	current_part = part;

	if (jffs2_1pass_rescan_needed(part)) {
		if (!jffs2_1pass_build_lists(part)) {
			printf("%s: Failed to scan JFFSv2 file structure\n", who);
			return NULL;
		}
	}
	return (struct b_lists *)part->jffs2_priv;
}

/* Load a file from flash into memory. fname can be a full path */
u32
jffs2_1pass_load(char *dest, struct part_info * part, const char *fname)
{

	struct b_lists *pl;
	long ret = 1;
	u32 inode;

	if (! (pl  = jffs2_get_list(part, "load")))
		return 0;

	if (! (inode = jffs2_1pass_search_inode(pl, fname, 1))) {
#ifdef __DEBUG__
		printf("load: Failed to find inode %s\r\n", fname);
#endif
		return 0;
	}
#ifdef __DEBUG__
	printf("(1) jffs2_1pass_load: %s (%x)\n", fname, inode);
#endif
	/* Resolve symlinks */
	if (! (inode = jffs2_1pass_resolve_inode(pl, inode))) {
#ifdef __DEBUG__
		putstr("load: Failed to resolve inode structure\r\n");
#endif
		return 0;
	}
#ifdef __DEBUG__
	printf("(2) jffs2_1pass_resolve_inode: %s (%x)\n", fname, inode);
#endif
	if ((ret = jffs2_1pass_read_inode(pl, inode, dest)) < 0) {
#ifdef __DEBUG__
		printf("load: Failed to read inode\r\n");
#endif
		return 0;
	}
#ifdef __DEBUG__
	printf("(3) jffs2_1pass_read_inode: %s (%x)\n", fname, inode);
#endif
#ifdef __DEBUG_MEMORY_TEST
	// Check Memory crc
	printf("(2) Read Memory complete %08x\n", crc32(0, nand_cache, CONFIG_JFFS2_PART_SIZE));
#endif

#ifdef __DEBUG__
	printf("load: loaded '%s' to 0x%lx (%ld bytes)\n", fname,
				(unsigned long) dest, ret);
#endif
	return ret;
}

#endif
