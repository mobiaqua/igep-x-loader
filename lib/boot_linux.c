/*
 * Copyright (C) 2010 ISEE
 * Manel Caro, ISEE, <mcaro@iseebcn.com>
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

#define SZ_1K				1024
#define SZ_2K				2 * SZ_1K
#define SZ_16K				16 * SZ_1K
#define KERNEL_MAX_CMDLINE		4 * SZ_1K

/* Internal Memory layout */
typedef struct Linux_Memory_Layout {
    char* kbase_address;                // It must be the dest kernel address
    int k_size;                         // 0 = and return here the size
    char* kImage_rd_address;            // Return here the rd image if it is found
    int rdImage_size;                   // Initial Ram disk size
    int kcmdposs;                       // Internal command counter
    char kcmdline[KERNEL_MAX_CMDLINE];  // Kernel command line
} l_my;

/* Linux Images */
const char* LinuxImageNames [] = {
        "zimage",
        "bzimage",
        "vmlinuz",
        0,
};

#define MEM_PADD_BLOCKS     4           // Add 4 * 16K blocks

/* 
 * Internal Global Variables : It Resides in the SDRAM not in the internal RAM 
 * at this stage the SDRAM must be Initialized and ready for use it. 
*/
static struct Linux_Memory_Layout* LMemoryLayout = (struct Linux_Memory_Layout*) XLOADER_KERNEL_MEMORY;
static struct tag *kparams = (struct tag *) XLOADER_KERNEL_PARAMS;
static struct tag *params = (struct tag *) XLOADER_KERNEL_PARAMS;

/* Initialize */
static void init_memory_layout (void)
{
    // Initialize Linux Memory Layout struct
    LMemoryLayout->kbase_address = 0;
    LMemoryLayout->k_size = 0;
    LMemoryLayout->kImage_rd_address = 0;
    LMemoryLayout->rdImage_size = 0;
    LMemoryLayout->kcmdposs = 0;
    LMemoryLayout->kcmdline[0] = '\0';
}

/* add_cmd_param : Add new kernel command line param to the internal buffer */
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

/* setup_start_tag : Initialize the kernel command line variable list */
static void setup_start_tag (void)
{
	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size (tag_core);
	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;
	params = tag_next (params);
}

/* setup the map memory setup for the kernel */
static void setup_memory_tags (void)
{
    /* TODO: Setup the memory, actually it's configured for 
     * use 512 MBytes and it's map into CS0 and CS1 
     */
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

/* setup_commandline_tag: Set the command line kernel variable in the var list */
static void setup_commandline_tag ()
{
	if (!LMemoryLayout->kcmdline) return;
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

/* setup_end_tag : terminate the var kernel list */
static void setup_end_tag (void)
{
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}


/* Pre: kbase_address: kernel base address
*  Out: kernel image size, copied to base_address
*       rdImage copied rear the linux kernel image
*       rdImage_size size of rd Image
*  result:	 -1 = kbase_address it's not configured
*  		0 = kernel not found in mmc
*		1 = kernel found
*/
int load_kernel_from_mmc (struct Linux_Memory_Layout *myImage)
{
    	int size = -1;
    	int found = 0;
    	int count = 0;
    	const char* linuxName = LinuxImageNames[0];
    	
	/* Check if the kbase_address it'sprovided (PREREQUISITE) */
	if(!myImage->kbase_address) return -1;

	while(linuxName && !found){
#ifdef __DEBUG__
		printf("try load kernel %s from mmc\n", linuxName);
#endif
		/* Try load the linuxName [n] Image into kbase_address */
		size = file_fat_read(linuxName, myImage->kbase_address , 0);
        	/* If size > 0 then the image was loaded ok */
		if(size > 0){
#ifdef __DEBUG__
            		printf("load kernel %s ok, entry point = 0x%x size = %d\n", linuxName, myImage->kbase_address, size);
#endif           
			/* Update the size variable */
			myImage->k_size = size;
			/* if the ram disk dest address it's not supplied then calculate the address */
            		if(!myImage->kImage_rd_address){
				// Put the RD address rear the kernel address with MEM_PADD_BLOCKS for padding
                		myImage->kImage_rd_address = myImage->k_size + (MEM_PADD_BLOCKS * SZ_16K) + myImage->kbase_address;
            		}
			/* try to load the RAM disk into kImage_rd_address */
			/* TODO: the rd image now it's hardcoded here, maybe it's a good idea 
			 * to permit supply a different names for it */
            		size = file_fat_read("initrd", myImage->kImage_rd_address, 0);
            		/* Update Information if we get the ram disk image into memory */
			if(size > 0){
                		myImage->rdImage_size = size;
            		}
#ifdef __DEBUG__
            		printf("kernel %s found first stage done\n", linuxName);
#endif
            		return 1;
        	}
#ifdef __DEBUG__
        	else {
            		printf("load kernel %s failed\n", linuxName);
        	}
#endif
		/* Try the next kernel image name */
        	linuxName = LinuxImageNames[++count];
    }
    return found;
}

/* cfg_handler : Ini file variables from ini file parser */
int cfg_handler ( void* usr_ptr, const char* section, const char* key, const char* value)
{
    unsigned int v;
    int res;
#ifdef __DEBUG__
    printf("section: %s key: %s value: %s\n", section, key, value);
#endif
    /* SECTION: kernel */
    if(!strcmp(section, "kernel")){
        if(!strcmp(key, "kaddress")){
            /* PARAM: kaddress : Destination kernel copy address */
            sscanf(value, "0x%x", &v);
            LMemoryLayout->kbase_address = (char*) v;
        }else if(!strcmp(key, "rdaddress")){
            /* PARAM: rdaddress : Destination initrd copy address */
	    sscanf(value, "0x%x", &v);
            LMemoryLayout->kImage_rd_address = (char*) v;
        }
    }
    /* SECTION: Kernel parameters */
    if(!strcmp(section, "kparams")){
	/* All variables in this section should be a kernel parameters */
        add_cmd_param(key, value);
    }
    return 1;
}

/* --- ARMv7 Kernel Boot Prerequisites --- */

/* cache_flush */
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

/* Prepare the system for kernel boot */
void cleanup_before_linux (void)
{
	unsigned int i;
    	
	/* disable all interrupts */
	disable_interrupts();

	/* disable I and D caches */
	icache_disable();
	dcache_disable();
	/* flush cache */
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

/* Main Linux boot 
*  If all it's ok this function never returns because at latest it 
*  Jump directly to the kernel image.
*/
int boot_linux (/*int machine_id*/)
{
    int bootr = -1;
    int machine_id = 2344;
    void (*theKernel)(int zero, int arch, uint params);

#ifdef __DEBUG__
    printf("Init Memory Layout\n");
#endif
    /* initialize internal variables */
    init_memory_layout();
#ifdef __DEBUG__
    printf("Parse configuration file\n");
#endif
    /* parse configuration file, actually only a ini file be supported */
    /* TODO: Set variable the configuartion file name, add other file formats  */
    if(ini_parse("igep.ini", cfg_handler, (void*) LMemoryLayout) >= 0){
#ifdef __DEBUG__
        printf("Try load kernel\n");
#endif
	/* If parse it's ok, it's a good moment for load 
        the kernel image from the mmc card */
        bootr = load_kernel_from_mmc(LMemoryLayout);
	/* if bootr it's > 0 then we get right the kernel image */
        if(bootr > 0){
	    /* prepare the kernel command line list */
            setup_start_tag();
            setup_memory_tags();
#ifdef __DEBUG__
            printf("kernel command line: \n%s\n", LMemoryLayout->kcmdline);
#endif
	    setup_commandline_tag();
            setup_end_tag();
#ifdef __DEBUG__            
	    printf("kernel boot: 0x%x 0x%x\n", LMemoryLayout->kbase_address,  kparams);
#endif
	    /* Prepare the system for kernel boot */
	    cleanup_before_linux();
            theKernel = (void (*)(int, int, uint)) LMemoryLayout->kbase_address;
            /* boot ! = GREAT ;) */
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
