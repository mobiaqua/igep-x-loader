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
#include <asm/arch/omap3.h>
#include <asm/io.h>

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
    unsigned int dss;					// Video Display output (1 = Enabled - 0 = Disabled)
    unsigned int dss_color;				// Video Color
    char kcmdline[KERNEL_MAX_CMDLINE];  // Kernel command line
} l_my;

/* Linux Images */
const char* LinuxImageNames [] = {
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
char *kImageAlt_Name = NULL;
char *dss_bitmap = NULL;
/* Default Boot kernel */
int boot_kernel = 1;
#ifdef K_VERIFY_CRC
int verify_ker_crc = 0;
#endif

/* Initialize */
static void init_memory_layout (void)
{
    // Initialize Linux Memory Layout struct
    LMemoryLayout->machine_id = DEFAULT_BOARD_ID;
    LMemoryLayout->kbase_address = DEFAULT_KADDRESS;
    LMemoryLayout->k_size = 0;
    LMemoryLayout->kImage_rd_address = DEFAULT_KRADRRESS;
    LMemoryLayout->rdImage_size = 0;
    LMemoryLayout->kcmdposs = 0;
    LMemoryLayout->revision.rev = 0;
    LMemoryLayout->serial.low = 0;
    LMemoryLayout->serial.high = 0;
    LMemoryLayout->dss = 0;
    LMemoryLayout->dss_color = DVI_ISEE_DEFAULT_COLOR;
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
    if(IS_MEM_512()){
        params->hdr.tag = ATAG_MEM;
        params->hdr.size = tag_size (tag_mem32);
        params->u.mem.start = OMAP34XX_SDRC_CS1;
        params->u.mem.size = IGEP_CS1_MEMORY_SIZE;
        params = tag_next (params);
    }
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
        linuxName = LinuxImageNames[0];
    if(kRdImage_Name)
        rdImageName = kRdImage_Name;

	if(!myImage->kbase_address) return -1;

	while(linuxName && !found){
#ifdef __DEBUG__
		printf("linux kernel name: %s\n", linuxName);
#endif
		/* Try load the linuxName [n] Image into kbase_address */
#ifdef IGEP00X_ENABLE_MMC_BOOT
		if(from == IGEP_MMC_BOOT)
            size = file_fat_read(linuxName, myImage->kbase_address , 0);
#endif
#ifdef IGEP00X_ENABLE_FLASH_BOOT
#ifdef IGEP00X_ENABLE_MMC_BOOT
        else
#endif
            size = load_jffs2_file(linuxName, myImage->kbase_address);
#endif
        	/* If size > 0 then the image was loaded ok */
		if(size > 0){
			/* Update the size variable */
			myImage->k_size = size;
#ifdef __CHECK_MEMORY_CRC__
			printf("XLoader: kernel %s loaded from %s at 0x%x size = %d (crc: %08x)\n", linuxName, from ? "FLASH" : "MMC" ,myImage->kbase_address, myImage->k_size,
                    crc32(0, LMemoryLayout->kbase_address, LMemoryLayout->k_size));
#else
			printf("XLoader: kernel %s loaded from %s at 0x%x size = %d\n", linuxName, from ? "FLASH" : "MMC" ,myImage->kbase_address, myImage->k_size);
#endif
			if(!rdImageName) return 1;

			/* if the ram disk dest address it's not supplied then calculate the address */
            if(!myImage->kImage_rd_address){
				// Put the RD address rear the kernel address with MEM_PADD_BLOCKS for padding
                myImage->kImage_rd_address = myImage->k_size + (MEM_PADD_BLOCKS * SZ_16K) + myImage->kbase_address;
            }
			/* try to load the RAM disk into kImage_rd_address */
			/* TODO: the rd image now it's hardcoded here, maybe it's a good idea
			 * to permit supply a different names for it */
#ifdef IGEP00X_ENABLE_MMC_BOOT
			if(from == IGEP_MMC_BOOT)
                size = file_fat_read(rdImageName, myImage->kImage_rd_address, 0);
#endif
#ifdef IGEP00X_ENABLE_FLASH_BOOT
#ifdef IGEP00X_ENABLE_MMC_BOOT
            else
#endif
                size = load_jffs2_file(rdImageName, myImage->kImage_rd_address, 0);
#endif
            /* Update Information if we get the ram disk image into memory */
			if(size > 0){
                myImage->rdImage_size = size;
                printf("XLoader: RamDisk %s loaded from %s at 0x%x size = %d\n", rdImageName, from ? "FLASH" : "MMC" ,myImage->kImage_rd_address, size);
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

#ifdef ENABLE_LOAD_INI_FILE
// #pragma GCC push_options
// #pragma GCC optimize ("O0")
/* cfg_handler : Ini file variables from ini file parser */
int cfg_handler ( void* usr_ptr, const char* section, const char* key, const char* value)
{
    unsigned int v;
    int res;
#ifdef __DEBUG__
    printf("DEBUG: section: %s key: %s value: %s\n", section, key, value);
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
#ifdef K_VERIFY_CRC
        else if(!strcmp(key, "Validate_kCRC")){
			if(!strcmp(value, "0"))
				verify_ker_crc = 0;
			else
				verify_ker_crc = 1;
		}
		else if(!strcmp(key, "kcrc")){
			sscanf(value, "%u", &v);
		}
		else if(!strcmp(key, "kImageAltName")){
			/* if CRC fails on first kernel it boots this backup directly */
            if(kImageAlt_Name) free(kImageAlt_Name);
            kImageAlt_Name = malloc (strlen(value)+1);
            memcpy(kImageAlt_Name, value, strlen(value));
            kImageAlt_Name[strlen(value)] = '\0';
		}
#endif
		else if(!strcmp(key, "dss")){
			sscanf(value, "%u", &v);
			LMemoryLayout->dss = v;
		}
		else if(!strcmp(key, "dss_color")){
			sscanf(value, "0x%x", &v);
			LMemoryLayout->dss_color = v;
		}
		else if(!strcmp(key, "dss_bitmap")){
			if(dss_bitmap) free(dss_bitmap);
			dss_bitmap = malloc (strlen(value)+1);
			memcpy(dss_bitmap, value, strlen(value));
			dss_bitmap[strlen(value)] = '\0';
		}
    }
    /* SECTION: Kernel parameters */
    if(!strcmp(section, "kparams")){
        /* All variables in this section should be a kernel parameters */
        if(!IS_MEM_512()){
            if(DSP_PRESENT())
                add_cmd_param(key, value);
            else{
                if(!strcmp(key, "mem") || !strcmp(key, "MEM"))
                    add_cmd_param(key, "256M");
                else
                    add_cmd_param(key, value);
            }
        }
        else
            add_cmd_param(key, value);
    }
    return 1;
}
// #pragma GCC pop_options
#endif
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
    if(is_cpu_family() == CPU_OMAP36XX){
        /* flush caches */
        cache_flush();
        /* enable I Cache */
        icache_enable();
        /* enable D Cache */
        dcache_enable();
        /* Invalidate d cache */
        invalidate_dcache(get_device_type());
    }
    else{
        icache_disable();
        dcache_disable();
        /* flush cache */
        cache_flush();
    }
    /* Enable l2 cache */
    l2_cache_enable();
}

int load_and_parse ()
{
    int bootr = 0;
#ifdef ENABLE_LOAD_INI_FILE
    bootr = ini_parse(IGEP_BOOT_CFG_INI_FILE, IGEP_MMC_BOOT, cfg_handler, (void*) LMemoryLayout);
    if(bootr >= 0){
        printf("XLoader: Configuration file igep.ini Loaded from MMC\n");
        return bootr;
    }
    bootr = ini_parse(IGEP_BOOT_CFG_INI_FILE, IGEP_ONENAND_BOOT, cfg_handler, (void*) LMemoryLayout);
    if(bootr >= 0)
		printf("XLoader: Configuration file igep.ini Loaded from Flash memory\n");
    else{
        printf("XLoader: Configuration file igep.ini not found or invalid (%d), using defaults\n", bootr);
        add_cmd_param("console", "ttyO2,115200n8");
        add_cmd_param("mem", "430M");
#ifdef IGEP00X_ENABLE_FLASH_BOOT
        add_cmd_param("root", "/dev/mtdblock2 rw rootwait");
        add_cmd_param("rootfstype", "jffs2");
#else
#ifdef IGEP00X_ENABLE_MMC_BOOT
        add_cmd_param("root", "/dev/mmcblk0p2 rw rootwait");
#endif
#endif
    }
#else
    printf("XLoader: Configuration file igep.ini disabled, using defaults\n");
    add_cmd_param("console", "ttyO2,115200n8");
    add_cmd_param("mem", "430M");
#ifdef IGEP00X_ENABLE_FLASH_BOOT
    add_cmd_param("root", "/dev/mtdblock2 rw rootwait");
    add_cmd_param("rootfstype", "jffs2");
#else
#ifdef IGEP00X_ENABLE_MMC_BOOT
    add_cmd_param("root", "/dev/mmcblk0p2 rw rootwait");
#endif
#endif
#endif
    return bootr;
}

int load_kernel_image (struct Linux_Memory_Layout* layout)
{
    // load_kernel
    int bootr;
#ifdef IGEP00X_ENABLE_MMC_BOOT
    // printf("XLoader: try load kernel from MMC\n");
    bootr = load_kernel (layout, IGEP_MMC_BOOT);
    if(bootr > 0) return bootr;
#endif
#ifdef IGEP00X_ENABLE_FLASH_BOOT
    // printf("XLoader: try load kernel from Flash\n");
    bootr = load_kernel (layout, IGEP_ONENAND_BOOT);
#endif
    return bootr;
}

void setup_video (void)
{
    char* splash = NULL;
    int fsize = 0;
    if(LMemoryLayout->dss){
		if(dss_bitmap){
			splash = malloc (DSS_VIDEO_MEMORY_SIZE);
			if(fsize = file_fat_read(dss_bitmap, splash, 0) > 0){
				enable_video_buffer(splash);
			} else if (fsize = load_jffs2_file(dss_bitmap, splash) > 0) {
				enable_video_buffer(splash);
			}
			else {
				enable_video_color(LMemoryLayout->dss_color);
			}
		}
		else
			enable_video_color(LMemoryLayout->dss_color);
	}
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
		/* Configure the Video Setup */
		setup_video();
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
                // printf("XLoader: Boot ARM binary mode: %s ...\n", kImage_Name);
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
            /*if(LMemoryLayout->machine_id == IGEP0020_MACHINE_ID)
                printf("XLoader: IGEPv2 : kernel boot ...\n");
            else if(LMemoryLayout->machine_id == IGEP0030_MACHINE_ID)
                printf("XLoader: IGEP Module : kernel boot ...\n");
            else if(LMemoryLayout->machine_id == IGEP0032_MACHINE_ID)
                printf("XLoader: IGEP Module 0032 : kernel boot ...\n");
            else printf("XLoader: Unknown %d : kernel boot ...\n", LMemoryLayout->machine_id); */
            /* Kernel Boot */
#ifdef __DEBUG__
			printf("kernel %p size %d crc %08x\n", LMemoryLayout->kbase_address,
                   LMemoryLayout->k_size, crc32(0, LMemoryLayout->kbase_address,
                                                LMemoryLayout->k_size));
#endif
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
