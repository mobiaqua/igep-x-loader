/*
 * Copyright (C) 2010 ISEE
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
#include <config.h>
#include <setup.h>

// #define DECLARE_GLOBAL_DATA_PTR     register volatile gd_t *gd asm ("r8")

typedef struct Linux_Memory_Layout {
    char* kbase_address;                // It must be the dest kernel address
    int k_size;                         // 0 = and return here the size
    char* kImage_rd_address;            // Return here the rd image if it is found
    int rdImage_size;                   // Initial Ram disk size
    int kcmdposs;                       // Internal command counter
    char kcmdline[4* 1024];            // Kernel command line
} l_my;

const char* LinuxImageNames [] = {
        "zimage",
        "bzimage",
        "vmlinuz",
        0,
};

#define SZ_16K              16 * 1024
#define MEM_PADD_BLOCKS     4           // Add 4 * 16K blocks

static struct Linux_Memory_Layout* LMemoryLayout = (struct Linux_Memory_Layout*) XLOADER_KERNEL_MEMORY;
static struct tag *kparams = (struct tag *) XLOADER_KERNEL_PARAMS;
static struct tag *params = (struct tag *) XLOADER_KERNEL_PARAMS;

static void init_memory_layout (void)
{
    // Initialize
    LMemoryLayout->kbase_address = 0;
    LMemoryLayout->k_size = 0;
    LMemoryLayout->kImage_rd_address = 0;
    LMemoryLayout->rdImage_size = 0;
    LMemoryLayout->kcmdposs = 0;
    LMemoryLayout->kcmdline[0] = '\0';
    // memset(LMemoryLayout->kcmdline, 0, 16 * 1024);
}

void add_cmd_param (const char* param_name, const char* param_value)
{
    int pName_len;
    int pValue_len;
    int tLen = 0;
    pName_len = strlen(param_name);
    pValue_len = strlen(param_value);
    strcpy(LMemoryLayout->kcmdline + LMemoryLayout->kcmdposs, param_name);
    LMemoryLayout->kcmdposs += pName_len;
    strcpy(LMemoryLayout->kcmdline + LMemoryLayout->kcmdposs, "=");
    LMemoryLayout->kcmdposs += 1;
    strcpy(LMemoryLayout->kcmdline + LMemoryLayout->kcmdposs, param_value);
    LMemoryLayout->kcmdposs += pValue_len;
    strcpy(LMemoryLayout->kcmdline + LMemoryLayout->kcmdposs, " ");
    LMemoryLayout->kcmdposs += 1;
    LMemoryLayout->kcmdline [LMemoryLayout->kcmdposs+1] = '\0';
}

static void setup_start_tag (void)
{
	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size (tag_core);
	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;
	params = tag_next (params);
}

//#ifdef CONFIG_SETUP_MEMORY_TAGS
static void setup_memory_tags (void)
{
    params->hdr.tag = ATAG_MEM;
    params->hdr.size = tag_size (tag_mem32);
    params->u.mem.start = OMAP34XX_SDRC_CS0;
    params->u.mem.size = 256 * 1024 * 1024;
    params = tag_next (params);
    params->hdr.tag = ATAG_MEM;
    params->hdr.size = tag_size (tag_mem32);
    params->u.mem.start = OMAP34XX_SDRC_CS1;
    params->u.mem.size = 256 * 1024 * 1024;
    params = tag_next (params);
}
// #endif /* CONFIG_SETUP_MEMORY_TAGS */

static void setup_commandline_tag ()
{
	// char *p = commandline;

	if (!LMemoryLayout->kcmdline)
		return;

	/* eat leading white space */
	// for (p = commandline; *p == ' '; p++);

	/* skip non-existent command lines so the kernel will still
	 * use its default command line.
	 */
	//if (*p == '\0')
		// return;

    //printf("strlen cmdline: %d 0x%x\n", strlen(commandline), commandline);

	params->hdr.tag = ATAG_CMDLINE;
	params->hdr.size = (sizeof (struct tag_header) + strlen (LMemoryLayout->kcmdline) + 1 + 4) >> 2;
	strcpy (params->u.cmdline.cmdline, LMemoryLayout->kcmdline);
	params = tag_next (params);
}

#ifdef CONFIG_INITRD_TAG
static void setup_initrd_tag (ulong initrd_start, ulong initrd_end)
{
	/* an ATAG_INITRD node tells the kernel where the compressed
	 * ramdisk can be found. ATAG_RDIMG is a better name, actually.
	 */
	params->hdr.tag = ATAG_INITRD2;
	params->hdr.size = tag_size (tag_initrd);

	params->u.initrd.start = initrd_start;
	params->u.initrd.size = initrd_end - initrd_start;

	params = tag_next (params);
}
#endif /* CONFIG_INITRD_TAG */

#ifdef CONFIG_SERIAL_TAG
void setup_serial_tag (struct tag **tmp)
{
	struct tag *params = *tmp;
	struct tag_serialnr serialnr;
	void get_board_serial(struct tag_serialnr *serialnr);

	get_board_serial(&serialnr);
	params->hdr.tag = ATAG_SERIAL;
	params->hdr.size = tag_size (tag_serialnr);
	params->u.serialnr.low = serialnr.low;
	params->u.serialnr.high= serialnr.high;
	params = tag_next (params);
	*tmp = params;
}
#endif

#ifdef CONFIG_REVISION_TAG
void setup_revision_tag(struct tag **in_params)
{
	u32 rev = 0;
	/* u32 get_board_rev(void); */
	/* rev = get_board_rev() */;
	params->hdr.tag = ATAG_REVISION;
	params->hdr.size = tag_size (tag_revision);
	params->u.revision.rev = rev;
	params = tag_next (params);
}
#endif  /* CONFIG_REVISION_TAG */


static void setup_end_tag (void)
{
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}


/* Pre: kbase_address: kernel base address
*  Out: kernel image size, copied to base_address
*       rdImage copied rear the linux kernel image
*       rdImage_size size of rd Image
*/
int load_kernel_from_mmc (struct Linux_Memory_Layout *myImage)
{
    // Variables
    int size = -1;
    int found = 0;
    int count = 0;
    const char* linuxName = LinuxImageNames[0];

    if(!myImage->kbase_address) return -1;

    while(linuxName && !found){
        printf("try load kernel %s\n", linuxName);
        size = file_fat_read(linuxName, myImage->kbase_address , 0);
        if(size > 0){
            printf("load kernel %s ok, entry point = 0x%x size = %d\n", linuxName, myImage->kbase_address, size);
            myImage->k_size = size;
            if(!myImage->kImage_rd_address){    // Calculate the initrd address
                myImage->kImage_rd_address = myImage->k_size + (MEM_PADD_BLOCKS * SZ_16K) + myImage->kbase_address;
            }
            size = file_fat_read("initrd", myImage->kImage_rd_address, 0);
            if(size > 0){
                myImage->rdImage_size = size;
            }
            // found = 1;
            printf("kernel %s found\n", linuxName);
            return 1;
        }
        else {
            printf("load kernel %s failed\n", linuxName);
        }
        linuxName = LinuxImageNames[++count];
    }
    return found;
}

int cfg_handler ( void* usr_ptr, const char* section, const char* key, const char* value)
{
    unsigned int v;
    int res;
    printf("section: %s key: %s value: %s\n", section, key, value);
    if(!strcmp(section, "kernel")){
        if(!strcmp(key, "kaddress")){
            sscanf(value, "0x%x", &v);
            LMemoryLayout->kbase_address = (char*) v;
            // printf("section: %s key: %s value: %s converted 0x%x\n", section, key, value, v);
        }else if(!strcmp(key, "rdaddress")){
            sscanf(value, "0x%x", &v);
            LMemoryLayout->kImage_rd_address = (char*) v;
        }
    }
    if(!strcmp(section, "kparams")){
        add_cmd_param(key, value);
    }
    return 1;
}

static void cache_flush(void)
{
	asm ("mcr p15, 0, %0, c7, c5, 0": :"r" (0));
}

/*
 * disable IRQ/FIQ interrupts
 * returns true if interrupts had been enabled before we disabled them
 */
int disable_interrupts (void)
{
	unsigned long old,temp;
	__asm__ __volatile__("mrs %0, cpsr\n"
			     "orr %1, %0, #0xc0\n"
			     "msr cpsr_c, %1"
			     : "=r" (old), "=r" (temp)
			     :
			     : "memory");
	return (old & 0x80) == 0;
}

void cleanup_before_linux (void)
{
    unsigned int i;
    disable_interrupts();

	icache_disable();
	dcache_disable();
	cache_flush();

#ifdef __notdef
#ifndef CONFIG_L2_OFF
	/* turn off L2 cache */
	l2_cache_disable();
	/* invalidate L2 cache also */
	invalidate_dcache(get_device_type());
#endif

	i = 0;
	/* mem barrier to sync up things */
	asm("mcr p15, 0, %0, c7, c10, 4": :"r"(i));

#ifndef CONFIG_L2_OFF
	l2_cache_enable();
#endif
#endif
}

int boot_linux ()
{
    int bootr = -1;
    int machine_id = 2344;
    void	(*theKernel)(int zero, int arch, uint params);

    printf("Init Memory Layout\n");
    init_memory_layout();
    printf("Parse configuration file\n");
    if(ini_parse("igep.ini", cfg_handler, (void*) LMemoryLayout) >= 0){
        printf("Try load kernel\n");
        bootr = load_kernel_from_mmc(LMemoryLayout);
        if(bootr > 0){
            setup_start_tag();
            setup_memory_tags();
            printf("kernel command line: \n%s\n", LMemoryLayout->kcmdline);
            setup_commandline_tag();
            setup_end_tag();
            printf("kernel boot: 0x%x 0x%x\n", LMemoryLayout->kbase_address,  kparams);
            cleanup_before_linux();
            theKernel = (void (*)(int, int, uint)) LMemoryLayout->kbase_address;
            theKernel (0, machine_id, kparams);
        }else{
            if(bootr < 0)
                printf("Invalid load kernel address\n");
            else
                printf("kernel not found\n");
        }
    }
    return bootr;
}
