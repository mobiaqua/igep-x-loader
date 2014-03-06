/*
 * (c) 2014 - Manel Caro : ISEE - www.isee.biz
 * IGEP00x0 Nand DMA aync driver (only Micron & hynix Nand memories be supported)
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
*/
#include <common.h>
#include <asm/io.h>

#include <malloc.h>
#include <linux/mtd/compat.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <asm/arch/gpmc.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#include <asm/io.h>
#include <asm/errno.h>

#include <common.h>

#include <asm/arch/mem.h>
#include <asm/arch/omap_gpmc.h>

// #define MAX_PARTITIONS                      3
// #define CONFIG_SYS_NAND_RESET_CNT 200000

static u32 cTicks = 0;
static struct Nand_Memory *flashMemory = NULL;

/* Check PIN WAIT0 (busy/ready) Nand indicator */
static int nand_dev_is_ready (void)
{
	return readl(GPMC_STATUS) & (1 << 8);
}

/* Wait unti ready or max_timeout */
static void wait_nand_until_ready (int max_timeout)
{
    while(!nand_dev_is_ready());
}

/* command interface */
static inline void omap_write_gpmc_cmd (u32 cmd)
{
    __raw_writeb(cmd, GPMC_NAND_COMMAND_0);
}

/* address interface */
static inline void omap_write_gpmc_addr (u32 addr)
{
    __raw_writeb(addr, GPMC_NAND_ADDRESS_0);
}

/* data interface */
static inline void omap_write_gpmc_dat (u32 dat)
{
    __raw_writeb(dat, GPMC_NAND_DATA_0);
}

/* read raw 8 bits from data interface */
static inline u8 nand_read_u8 (void)
{
    return __raw_readb(GPMC_NAND_DATA_0);
}

/* read raw 16 bits from data interface */
static inline u16 nand_read_u16 (void)
{
    return __raw_readw(GPMC_NAND_DATA_0);
}

/* read memory mapped 32 bits, only if prefetch is active */
static inline u32 nand_read_32 (void)
{
    return __raw_readl(NAND_ADDR_MAP);
}

/* read buffer count from memory mapped, only if prefetch is active */
static inline void nand_read_buff32 (u32* dst, int count)
{
    while (--count >= 0) {
        *dst = nand_read_32();
        dst++;
    }
}

/* read raw buffer from gpmc interface */
void nand_read_buf8 (u8* buff, int len)
{
    int i;
    for(i=0; i < len; i++)
        buff[i] = nand_read_u8();
}

/* read raw 16 bits buffer from gpmc interface */
void __nand_read_buff16 (u8* buf, int len)
{
    int i;
    u16*j = (u16*) buf;
    for(i=0; i < len/2; i++){
         j[i] = nand_read_u16();
    }
}

/* read buffer with prefetch interface running */
void prefetch_read_buff (u8* buf, int len)
{
    int r_count = 0;
    u16 *p = (u16*) buf;
    do{
        r_count = gpmc_read_status(GPMC_PREFETCH_FIFO_CNT);
        if((r_count % 4) == 0){
            r_count = r_count >> 2;
            nand_read_buff32(p, r_count);
            p += r_count << 1;
            len -= r_count << 2;
        }
    }while(len > 0);
}

/* read buffer, aligned to 64 bytes using DMA and prefetch engine, len is the size in bytes */
void nand_read_buf16 (u8 *buf, int len)
{
    // Setup DMA tranfer (setup channel 10)
    dma_gpmc_transfer(10, NAND_ADDR_MAP, buf, len);
    // Enable prefetch engine
    if(gpmc_prefetch_enable(0, PREFETCH_FIFOTHRESHOLD_MAX, 1, len, 0))
    {
        hang();
    }
    // prefetch_read_buff(buf, len);
    // Wait Completition
    while(!dma_is_transfer_complete(10));
    // if transfer is complete reset prefetch engine and stop it
    gpmc_prefetch_reset(0);
}

/* Send get NAND status command, note after receive the status the interface
left in command mode, it must be changed to data mode if we're receiving data
TODO: check if exist any timming violation here
*/
static u8 nand_status (void)
{
    omap_write_gpmc_cmd(NAND_CMD_STATUS);
    return nand_read_u8();
}

/* send reset command to NAND */
static void nand_reset (void)
{
    omap_write_gpmc_cmd(NAND_CMD_RESET);
    while((nand_status() & NAND_STATUS_READY)!= NAND_STATUS_READY);
}

/* change interface to data mode */
static void nand_set_datamode (void)
{
    omap_write_gpmc_cmd(NAND_CMD_READ0);
    udelay(10);
    wait_nand_until_ready(0);
    udelay(2);
}

/* get status enhaced from nand fro selected addr
   addr aligned to page block starts at addr[6]
*/
static u8 nand_status_enhaced (u32 addr)
{
    omap_write_gpmc_cmd(NAND_CMD_STATUS_ENH);
    omap_write_gpmc_addr(addr);
    omap_write_gpmc_addr(addr >> 8);
    omap_write_gpmc_addr(addr >> 16);
    return nand_read_u8();
}

/* read the Nand Manufacturer ID */
void read_nand_manufacturer_id (u32 *m, u32 *i)
{
    nand_reset();
    omap_write_gpmc_cmd(NAND_CMD_READID);
    omap_write_gpmc_addr(0);
    wait_nand_until_ready(0);
    *m = nand_read_u8();
    *i = nand_read_u8();
}

/* Set Nand ECC calculation (en=1 or en=0)
only tested with Micron Memory
*/
int set_nand_ecc (int en)
{
    omap_write_gpmc_cmd(0xEF);
    omap_write_gpmc_addr(0x90);
    udelay(70);
    if(en)
        omap_write_gpmc_dat(0x08);
    else
        omap_write_gpmc_dat(0x00);
    omap_write_gpmc_dat(0x00);
    omap_write_gpmc_dat(0x00);
    omap_write_gpmc_dat(0x00);
    wait_nand_until_ready(0);
}

/* get Nand ECC engine status
    0: Not enabled
    8: Enabled
*/
int get_nand_ecc (void)
{
    u8 dat[4];
    omap_write_gpmc_cmd(0xEE);
    omap_write_gpmc_addr(0x90);
    udelay(10);
    wait_nand_until_ready(0);
    udelay(20);
    nand_read_buf8(&dat, 4);
    return dat[0] & 0x8;
}

/* Generic send a signle command with address */
void igep00x0_nand_cmd (u8 cmd, u8 addr, u8* buff, u32 size)
{
    omap_write_gpmc_cmd(cmd);
    omap_write_gpmc_addr(addr);
    wait_nand_until_ready(0);
    nand_read_buf8(buff, size);
}

/* Calculate onfo 16 bits CRC */
static u16 onfi_crc16 (u16 crc, u8 const *p, size_t len)
{
	int i;

	while (len--) {
		crc ^= *p++ << 8;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
	}

	return crc;
}

/* read NAND page, address aligned to page, block starts at add[6]
it uses DMA & prefetch engine for read the page, it receives page in 25uS */
void igep00x0_read_page (u32 addr, struct Nand_Page* Page)
{
    // First send CMD
    omap_write_gpmc_cmd(NAND_CMD_READ0);
    // Ignore column
    omap_write_gpmc_addr(0x00);
    omap_write_gpmc_addr(0x00);
    // Send Address
    omap_write_gpmc_addr(addr);
    omap_write_gpmc_addr(addr >> 8);
    omap_write_gpmc_addr(addr >> 16);
    // SEND READ START command
    omap_write_gpmc_cmd(NAND_CMD_READSTART);
    udelay(10);
    wait_nand_until_ready(0);
    udelay(2);
    nand_read_buf16(Page->page_dat, Page->Memory->mem_page_tsize);
}

/*
 * Check if the NAND chip is ONFI compliant, returns 0 if it is, -1 otherwise
 */
static int nand_flash_detect_onfi (struct Nand_Memory* fMem)
{
    u8 rbytes[5] = {0, 0, 0, 0, 0};
    igep00x0_nand_cmd(NAND_CMD_READID, 0x20, rbytes, 4);
    // Verify onfi signature
    if(strcmp(rbytes, "ONFI"))
        return -1;
    // Read page param from Nand
    igep00x0_nand_cmd(NAND_CMD_PARAM, 0, 0, 0);
    nand_read_buf8(&fMem->onfi, sizeof(struct nand_onfi_params));
    // Verify structure with CRC
    if(onfi_crc16(ONFI_CRC_BASE, &fMem->onfi, 254) != fMem->onfi.crc)
        return -1;  /* onfi detect failed */
    /* Save Onfi read information from Nand */
    fMem->mem_page_size = fMem->onfi.byte_per_page;
    fMem->mem_page_spare_size = fMem->onfi.spare_bytes_per_page;
    fMem->mem_page_block = fMem->onfi.pages_per_block;
    fMem->mem_erase_size = fMem->onfi.pages_per_block * fMem->mem_page_size;
    fMem->mem_size = fMem->onfi.blocks_per_lun * fMem->mem_erase_size;
    fMem->mem_page_tsize = fMem->mem_page_size + fMem->mem_page_spare_size;
    fMem->mem_ecc_size = 256;
    fMem->mem_ecc_bytes = 3;
    fMem->mem_ecc_steps = fMem->mem_page_size / fMem->mem_ecc_size;
    fMem->mem_ecc_total = fMem->mem_ecc_steps * fMem->mem_ecc_bytes;
    nand_reset();
    return 0;
}

static struct Nand_Page* create_page (u32 address)
{
    struct Nand_Page *Page = malloc (sizeof(struct Nand_Page));
    Page->Memory = flashMemory;
    Page->nand_addr = address;
    Page->ecc_calc = malloc(Page->Memory->mem_ecc_total);
    Page->ecc_nand = malloc(Page->Memory->mem_ecc_total);
    Page->page_dat = malloc (Page->Memory->mem_page_tsize);
    Page->spare_dat = Page->page_dat + Page->Memory->mem_page_size;
    Page->ecc_fail  = 0;
    return Page;
}

static void free_page (struct Nand_Page *Page)
{
    free(Page->ecc_calc);
    free(Page->ecc_nand);
    free(Page->page_dat);
    free(Page);
}

/* +++++++++++++++ */

int nand_prepare_read_buf (u32 addr)
{
    // First send CMD
    omap_write_gpmc_cmd(NAND_CMD_READ0);
    // Ignore column
    // Column mask = 0x0000007F
    // Column addr_0[0..7]
    omap_write_gpmc_addr(0x00);
    // Column addr_1[8..10]
    omap_write_gpmc_addr(0x00);
    // Send Address [Page + Block]
    omap_write_gpmc_addr(addr);
    omap_write_gpmc_addr(addr >> 8);
    omap_write_gpmc_addr(addr >> 16);
    // SEND READ START command
    omap_write_gpmc_cmd(NAND_CMD_READSTART);
    udelay(10);
    wait_nand_until_ready(0);
    udelay(2);
}

int nand_prepare_read_buf_next (void)
{
    omap_write_gpmc_cmd(NAND_CMD_CACHE_READ);
    udelay(10);
    wait_nand_until_ready(0);
    udelay(2);
}
int nand_prepare_read_buf_last (void)
{
    omap_write_gpmc_cmd(NAND_CMD_CACHE_END);
    udelay(10);
    wait_nand_until_ready(0);
    udelay(2);
}

int nand_transfer_buf (u8* buf, int len)
{
    // Enable DMA transfer
    dma_gpmc_transfer(10, NAND_ADDR_MAP, buf, len);
    // Enable prefetch engine
    return !gpmc_prefetch_enable(0, PREFETCH_FIFOTHRESHOLD_MAX, 1, len, 0);
}

// 0: Not complete and 1: complete
int nand_read_buf_is_complete (void)
{
    // Check if transfer is complete
    return dma_is_transfer_complete(10);
}

void nand_read_buf_complete ()
{
    gpmc_prefetch_reset(0);
}

u32 block_to_page_addr (u32 block_id)
{
    return (block_id << 6 );
    // return ( flashMemory->mem_page_block * flashMemory->mem_page_size * block_id );
}

u32 addr_to_page (u32 addr)
{
    return (addr >> 11);
}

int IsBadBlock ( struct Nand_Page* Page )
{
    return (Page->spare_dat[0] != 0xFF );
}

enum sBLOCK_STATES {    st_BLOCK_PREPARE_INITIAL,
                        st_CALC_ECC,
                        st_WAIT_PAGE,
                        st_REQUEST_NEXT } ;

/* This is a simple Nand block read with verify badblock & Ecc Verification,
0: All pages read ok
u32 (-1) bad block, pages not read
>0 count pages fail, any page fail is set in memory as 0xFFFFFFFF (erased) */
u32 nand_read_block (u32 block_id, u8* mem, u32 size)
{
    u32 pageFail = 0;
    int i, pageCount = 0;
    u32 initial_addr = 0;
    // u32 pageFail = 0;
    enum sBLOCK_STATES rBlock_state = st_BLOCK_PREPARE_INITIAL;
    struct Nand_Block *block_p = 0;
    if(size < (flashMemory->mem_page_block * flashMemory->mem_page_size)){
        // printf("wheeek!, size=%u flashMemory->mem_page_block = %u flashMemory->mem_page_size %u total = %u\n", size, flashMemory->mem_page_block,flashMemory->mem_page_size,  flashMemory->mem_page_block * flashMemory->mem_page_size );
        return -1;
    }
    block_p = malloc(sizeof(struct Nand_Block));
    block_p->block_addrs = block_id;
    for(i = 0; i < flashMemory->mem_page_block; i++){
        block_p->Pages[i] = 0;
    }
    initial_addr = block_to_page_addr (block_id);
    while( pageCount <= flashMemory->mem_page_block ){
        switch(rBlock_state){
            case st_BLOCK_PREPARE_INITIAL: /* Only for page 0, it prepare the sequential cache read from NAND */
                block_p->Pages[pageCount] = create_page ( initial_addr + pageCount );
                nand_prepare_read_buf( (block_p->Pages[pageCount])->nand_addr );
                nand_prepare_read_buf_next();
                nand_transfer_buf((block_p->Pages[pageCount])->page_dat, flashMemory->mem_page_tsize);
                rBlock_state = st_WAIT_PAGE;
                break;
            case st_CALC_ECC:
                // Verify the Bad block Existence, check only page 0, it's possible
                // to verify the second page too ?¿?¿
                if(pageCount-1 == 0){
                    /* if( IsBadBlock ( block_p->Pages[pageCount - 1] )){
                        pageFail = 0xFFFFFFFF;
                        goto exit_fail;
                    } */
                }
                // Calculate the ECC in pageCount - 1
                if(verify_correct_ecc( block_p->Pages[pageCount - 1] )){
                    // printf("page ecc fail: %u\n", pageCount-1);
                    // Remove page and put a 0 indicator on page
                    pageFail++;
                    free_page(block_p->Pages[pageCount - 1]);
                    block_p->Pages[pageCount - 1] = 0;
                    while(!dma_is_transfer_complete(0));
                    // Fill with Erased Page
                    dma_fill(0, mem + ((pageCount - 1) * flashMemory->mem_page_size), flashMemory->mem_page_size, 0, 0xFFFFFFFF);
                }
                else {  /* Not Fail */
                    // printf("page ecc ok: %u\n", pageCount-1);
                    // mem_dump((block_p->Pages[pageCount - 1])->page_dat, 512 );
                    while(!dma_is_transfer_complete(0));
                    dma_memcpy(0, (block_p->Pages[pageCount - 1])->page_dat, mem + ((pageCount - 1) * flashMemory->mem_page_size) ,flashMemory->mem_page_size, 0);
                }
                rBlock_state = st_WAIT_PAGE;
                break;
            case st_WAIT_PAGE:
                // printf("st_WAIT_PAGE (%u)\n", pageCount);
                if(nand_read_buf_is_complete()){
                    nand_read_buf_complete();
                    rBlock_state = st_REQUEST_NEXT;
                }
                break;
            case st_REQUEST_NEXT:
                // printf("st_REQUEST_NEXT Actual: %u Next: %u\n", pageCount, pageCount+1);
                pageCount++;
                if(pageCount < flashMemory->mem_page_block){
                    block_p->Pages[pageCount] = create_page ( initial_addr + pageCount );
                    if(pageCount+1 >= flashMemory->mem_page_block){
                        nand_prepare_read_buf_last();
                        // printf("st_REQUEST_NEXT Actual: last (%u)\n", pageCount);
                    }else{
                        nand_prepare_read_buf_next();
                        // printf("st_REQUEST_NEXT Actual: next (%u)\n", pageCount);
                    }
                    nand_transfer_buf((block_p->Pages[pageCount])->page_dat, flashMemory->mem_page_tsize);
                }
                rBlock_state = st_CALC_ECC;
                break;
        }
    }
    // Verify if any block fail and if anyone is found retry to load the page
    if(pageFail > 0){
        for(i = 0; i < flashMemory->mem_page_block; i++){
            if(block_p->Pages[i] == 0){
                block_p->Pages[i] = create_page ( initial_addr + i );
                igep00x0_read_page(initial_addr + i , block_p->Pages[i] );
                if(!verify_correct_ecc( block_p->Pages[i])){
                    // Copy the right page then
                    pageFail--;
                    while(!dma_is_transfer_complete(0));
                    dma_memcpy(0, (block_p->Pages[i])->page_dat, mem + (i * flashMemory->mem_page_size) ,flashMemory->mem_page_size, 0);
                }
            }
        }
        // Wait if last page has been copied
        while(!dma_is_transfer_complete(0));
    }
exit_fail:
    // free pages
    for(i = 0; i < flashMemory->mem_page_block; i++){
        if(block_p->Pages[i] != 0){
            free_page(block_p->Pages[i]);
            block_p->Pages[i] = 0;
        }
    }
    // free index
    free(block_p);
    // Page fail without recovery
    return pageFail;
}

/* ++++++++++++++++++++++++++ */

int igep00x0_InitFlashMemory (void)
{
    struct Nand_Page* Page;
    int i;
    char *test_buffer;
    u32* addr = 0x80008000;
    flashMemory = malloc(sizeof(struct Nand_Memory));
    if(nand_flash_detect_onfi(flashMemory)) /* read onfi information from nand device */
        hang();
    for(i=0; i < MAX_PARTITIONS; i++)
        flashMemory->part[i].in_use = 0;

    // Page = create_page( 0 );
    // start_count();
    // igep00x0_read_page (0, Page );
    // end_count();
    // cTicks = getCountTicks();
    // if(verify_correct_ecc(Page) < 0){
    //    printf("Page read error\n");
    // }

    // start_count();
    // nand_read_block (4, addr, 1 * 1024 * 1024);
    // end_count();
    // cTicks = getCountTicks();
    // printf("read block t: %u fails: %u\n", cTicks, pageFail);
    // while(1);
    return 0;
}

int igep00x0_create_partition (u32 add , u32 size, int cached)
{
    int i, j;
    // Check alignement 4
    if(size % 4)
        return -1;
    for(i=0; i < MAX_PARTITIONS; i++){
        if(!flashMemory->part[i].in_use){
            // define new partition
            flashMemory->part[i].nand_start_addr = add;
            flashMemory->part[i].nand_part_size = size;
            flashMemory->part[i].in_use = 1;
            flashMemory->part[i].sector_size = flashMemory->mem_page_block * flashMemory->mem_page_size;
            flashMemory->part[i].sector_cnt = flashMemory->part[i].nand_part_size / flashMemory->part[i].sector_size;
            // Initialize block list
            for (j = 0; j < PART_MAX_SECTOR; j++){
                flashMemory->part[i].Block[j] = 0;
            }
            return i;
        }
    }
    return -1;
}

int show_mem_info (void)
{
    char DevName [21];
    memcpy(DevName, flashMemory->onfi.model, 20);
    DevName [20] = 0;
    printf("IGEP-X-Loader: Memory Model: %s\n", DevName);
    printf("IGEP-X-Loader: Flash Memory size: %u MiB Page Size: %u KiB Spare Area: %u Bytes Pages x Block: %u\n", \
                                                                flashMemory->mem_size / (1024*1024), \
                                                                flashMemory->mem_page_size / 1024, \
                                                                flashMemory->mem_page_spare_size, \
                                                                flashMemory->mem_page_block);
    return 0;
}
