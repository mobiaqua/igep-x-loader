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

// #define __DEBUG__

#define SZ_1K				        1024
#define SZ_2K				        2 * SZ_1K
#define SZ_16K				        16 * SZ_1K
#define SZ_1M                       SZ_1K * SZ_1K
#define KERNEL_MAX_CMDLINE		    4 * SZ_1K

#define DEFAULT_RAMDISK_NAME        "initrd"

/* Internal Memory layout */
typedef struct Linux_Memory_Layout {
    int machine_id;                     // Linux Machine ID
    char* kbase_address;                // It must be the dest kernel address
    int k_size;                         // 0 = and return here the size
    char* kImage_rd_address;            // Return here the rd image if it is found
    int rdImage_size;                   // Initial Ram disk size
    int kcmdposs;                       // Internal command counter
    struct tag_revision revision;       // Revision ID
    struct tag_serialnr serial;         // Serial ID
    char kcmdline[KERNEL_MAX_CMDLINE];  // Kernel command line
} l_my;

/* Linux Images */
const char* LinuxImageNames [] = {
        "kparam"   /* Use kparam first */
        "zImage",   /* jffs2 it's case sensitive */
        "zimage",   /* fat name it's not case sentitive */
        "vmlinuz",
        "bzImage",
        "bzimage",
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

char *kImage_Name = NULL;
char *kRdImage_Name = NULL;
/* Default Boot kernel */
int boot_kernel = 1;

/* Initialize */
static void init_memory_layout (void)
{
    // Initialize Linux Memory Layout struct
    LMemoryLayout->machine_id = IGEP0030_MACHINE_ID;
    LMemoryLayout->kbase_address = 0;
    LMemoryLayout->k_size = 0;
    LMemoryLayout->kImage_rd_address = 0;
    LMemoryLayout->rdImage_size = 0;
    LMemoryLayout->kcmdposs = 0;
    LMemoryLayout->revision.rev = 0;
    LMemoryLayout->serial.low = 0;
    LMemoryLayout->serial.high = 0;
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
    params->hdr.tag = ATAG_MEM;
    params->hdr.size = tag_size (tag_mem32);
    params->u.mem.start = OMAP34XX_SDRC_CS0;
    params->u.mem.size = IGEP_CS0_MEMORY_SIZE;
    params = tag_next (params);
    params->hdr.tag = ATAG_MEM;
    params->hdr.size = tag_size (tag_mem32);
    params->u.mem.start = OMAP34XX_SDRC_CS1;
    params->u.mem.size = IGEP_CS1_MEMORY_SIZE;
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

static void setup_initrd_tag (ulong initrd_start, ulong initrd_size)
{
	params->hdr.tag = ATAG_INITRD2;
	params->hdr.size = tag_size (tag_initrd);
	params->u.initrd.start = initrd_start;
	params->u.initrd.size = initrd_size;
	params = tag_next (params);
}

void setup_serial_tag ()
{
	params->hdr.tag = ATAG_SERIAL;
	params->hdr.size = tag_size (tag_serialnr);
	params->u.serialnr.low = LMemoryLayout->serial.low;
	params->u.serialnr.high= LMemoryLayout->serial.high;
	params = tag_next (params);
}

void setup_revision_tag ()
{
	params->hdr.tag = ATAG_REVISION;
	params->hdr.size = tag_size (tag_revision);
	params->u.revision.rev = LMemoryLayout->revision.rev;
	params = tag_next (params);
}

/* setup_end_tag : terminate the var kernel list */
static void setup_end_tag (void)
{
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}


/* Pre: kbase_address: kernel base address
*       from: IGEP_MMC_BOOT -> boot from mmc
        from: IGEP_ONENAND_BOOT -> boot from onenand jffs2 boot partition
*  Out: kernel image size, copied to base_address
*       rdImage copied below the linux kernel image
*       rdImage_size size of rd Image
*  result:	 -1 = kbase_address it's not configured
*  		0 = kernel not found in mmc
*		1 = kernel found
*/
int load_kernel (struct Linux_Memory_Layout *myImage, int from)
{
    int size = -1;
    int found = 0;
    int count = 0;
    const char* linuxName;
    const char* rdImageName = DEFAULT_RAMDISK_NAME;

    if(kImage_Name)
        linuxName = kImage_Name;
    else
        linuxName = LinuxImageNames[++count];
    if(kRdImage_Name)
        rdImageName = kRdImage_Name;

	if(!myImage->kbase_address) return -1;

	while(linuxName && !found){
		/* Try load the linuxName [n] Image into kbase_address */
		if(from == IGEP_MMC_BOOT)
            size = file_fat_read(linuxName, myImage->kbase_address , 0);
        else
            size = load_jffs2_file(linuxName, myImage->kbase_address);
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
			if(from == IGEP_MMC_BOOT)
                size = file_fat_read(rdImageName, myImage->kImage_rd_address, 0);
            else
                size = load_jffs2_file(rdImageName, myImage->kImage_rd_address, 0);
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
        else if(!strcmp(key, "revision")){
            sscanf(value, "%u", &v);
            LMemoryLayout->revision.rev = v;
        }
        else if(!strcmp(key, "serial.low")){
            sscanf(value, "%u", &v);
            LMemoryLayout->serial.low = v;
        }
        else if(!strcmp(key, "serial.high")){
            sscanf(value, "%u", &v);
            LMemoryLayout->serial.high = v;
        }
        else if(!strcmp(key, "kImageName")){
            if(kImage_Name) free(kImage_Name);
            kImage_Name = malloc (strlen(value)+1);
            memcpy(kImage_Name, value, strlen(value));
            kImage_Name[strlen(value)] = '\0';
        }
        else if(!strcmp(key, "kRdImageName")){
            if(kRdImage_Name) free(kRdImage_Name);
            kRdImage_Name = malloc (strlen(value)+1);
            memcpy(kRdImage_Name, value, strlen(value));
            kRdImage_Name[strlen(value)] = '\0';
        }
        else if(!strcmp(key, "MachineID")){
            sscanf(value, "%u", &v);
            LMemoryLayout->machine_id = v;
        }
        else if(!strcmp(key, "Mode")){
            if(!strcmp(value, "kernel")){
                boot_kernel=1;
            }
            else boot_kernel=0;
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

int load_and_parse ()
{
    int bootr;
    bootr = ini_parse(IGEP_BOOT_CFG_INI_FILE, IGEP_MMC_BOOT, cfg_handler, (void*) LMemoryLayout);
    if(bootr >= 0){
#ifdef __DEBUG__
        printf("Loaded %s from MMC\n", IGEP_MMC_BOOT);
#endif
        return bootr;
    }
    bootr = ini_parse(IGEP_BOOT_CFG_INI_FILE, IGEP_ONENAND_BOOT, cfg_handler, (void*) LMemoryLayout);
#ifdef __DEBUG__
    if(bootr >= 0)
        printf("Loaded %s from OneNand\n", IGEP_ONENAND_BOOT);
    else
        printf("Configuration file <not found>\n");
#endif
    return bootr;
}

int load_kernel_image (struct Linux_Memory_Layout* layout)
{
    // load_kernel
    int bootr;
    bootr = load_kernel (layout, IGEP_MMC_BOOT);
    if(bootr > 0) return bootr;
    bootr = load_kernel (layout, IGEP_ONENAND_BOOT);
    return bootr;
}

/* Main Linux boot
*  If all it's ok this function never returns because at latest it
*  Jump directly to the kernel image.
*/
int boot_linux (/*int machine_id*/)
{
    int bootr = -1;
    // int machine_id = IGEP0030_MACHINE_ID;
    void (*theKernel)(int zero, int arch, uint params);
    void (*theARMExec)(void);

#ifdef __DEBUG__
    printf("Init Memory Layout\n");
#endif
    /* initialize internal variables */
    init_memory_layout();
#ifdef __DEBUG__
    printf("Parse configuration file\n");
#endif
    /* parse configuration file */
    if(load_and_parse() >= 0){
#ifdef __DEBUG__
        printf("Try load kernel\n");
#endif
	/* If parse it's ok, it's a good moment for load
        the kernel image from the mmc card */
        bootr = load_kernel_image (LMemoryLayout);
	/* if bootr it's > 0 then we get right the kernel image */
        if(bootr > 0){
            // If boot_kernel == 0 then we load a binary file and jump to it
            if(!boot_kernel){
                printf("XLoader: Boot ARM binary mode: %s ...\n", kImage_Name);
                cleanup_before_linux();
                theARMExec = (void (*)(void)) LMemoryLayout->kbase_address;
                theARMExec();
                hang();
            }
            /* prepare the kernel command line list */
            setup_start_tag();
            setup_memory_tags();
            setup_serial_tag();
            setup_revision_tag();
            if(LMemoryLayout->rdImage_size > 0){
                setup_initrd_tag((ulong) LMemoryLayout->kImage_rd_address , LMemoryLayout->rdImage_size);
            }

#ifdef __DEBUG__
            // printf("kernel command line: \n%s\n", LMemoryLayout->kcmdline);
#endif
            setup_commandline_tag();
            setup_end_tag();

#ifdef __DEBUG__
            printf("kernel boot: 0x%x 0x%x\n", LMemoryLayout->kbase_address,  kparams);
#endif
            /* Prepare the system for kernel boot */
            cleanup_before_linux();
            theKernel = (void (*)(int, int, uint)) LMemoryLayout->kbase_address;
            if(LMemoryLayout->machine_id == IGEP0020_MACHINE_ID)
                printf("XLoader: IGEPv2 : kernel boot ...\n");
            else if(LMemoryLayout->machine_id == IGEP0030_MACHINE_ID)
                printf("XLoader: IGEP Module : kernel boot ...\n");
            else if(LMemoryLayout->machine_id == IGEP0032_MACHINE_ID)
                printf("XLoader: IGEP Module 0032 : kernel boot ...\n");
            else printf("XLoader: Unknown %d : kernel boot ...\n", LMemoryLayout->machine_id);
            /* Kernel Boot */
            theKernel (0, LMemoryLayout->machine_id, kparams);
        }else{
            if(bootr < 0)
                printf("Invalid load kernel address\n");
            else
                printf("kernel not found\n");
        }
    }
    return bootr;
}
