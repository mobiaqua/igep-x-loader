#include <common.h>
#include <asm/io.h>
#include <linux/mtd/nand.h>

void igep00x0_read_raw (u32 blockid, u8* addr, u32 partsize_blocks)
{
    int i, skip = 0;
    int count_block = 0;
    int res = 0;
    for(i = blockid; i < partsize_blocks; i++, count_block++){
        u32 *p = addr + ((64*2048) * (count_block - skip));
        // printf("nand:read_block: (%d) 0x%.8p\n", i, p);
        res = nand_read_block (i, p, (u32) -1);
        if(res == -1){
            printf("error %d\n", i);
            skip++;
        }
    }
}
