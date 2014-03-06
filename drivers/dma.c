#include <common.h>
#include <asm/arch/dma.h>
#include <asm/io.h>
#ifdef __SDMA_IRQ__
#include <asm/arch/interrupts.h>
#endif
#define DMA_DROP		1 << 1
#define DMA_HALF		1 << 2
#define DMA_FRAME		1 << 3
#define DMA_LAST		1 << 4
#define DMA_BLOCK		1 << 5
#define DMA_SYNC		1 << 6
#define DMA_PKT			1 << 7
#define DMA_TX_ERROR	1 << 8
#define DMA_SUPER_ERR	1 << 10
#define DMA_ALIGN_ERR	1 << 11
#define DMA_DRAIN_END	1 << 12
#define DMA_SUPER_BLOCK	1 << 14

#ifdef __SDMA_IRQ__

enum dma_TRS_STATUS { t_dma_WAIT = 0, t_dma_COMPLETE = 1, t_dma_ERROR = 3 };
typedef void (*__sdma_handler) (u32 channel, u32 src, u32 dest, u32 bSZ, enum dma_TRS_STATUS st, u32 dma_status, void* userp);

typedef struct {
    u32 channel;
    u32 src;
    u32 dst;
    u32 bSZ;
    u32 dma_status;
    enum dma_TRS_STATUS trs_st;
    u32 irq;
    __sdma_handler handler;
    void* user_p;
}  tsDMA_ctl ;

#endif

static tsDMA *System_DMA = 0;
#ifdef __SDMA_IRQ__
static tsDMA_ctl* System_DMA_ctl[32];
#endif

static u32 mask_table[] = {
    0x00000000,     /* 0 bit */
    0x00000001,     /* 1 bit */
    0x00000003,     /* 2 bits */
    0x00000007,     /* 3 bits */
    0x0000000F,     /* 4 bits */
    0x0000001F,     /* 5 bits */
    0x0000003F,     /* 6 bits */
    0x0000007F,     /* 7 bits */
    0x000000FF,     /* 8 bits */
};

static u32 inline sbit_val (u32 reg, u32 nbits, u32 spos, u32 val)
{
    return ((reg & ~((mask_table[nbits]) << spos)) | (val << spos));
}

#ifdef __SDMA_IRQ__
/* IRQ handler function */
void irq_sdma_handler (u32 irq)
{
    u32 irq_f, irq_l;
    u32 *reg_status, *reg_irq_en;
    int i;

    if( irq == SDMA_IRQ_0 ){
        reg_status = &System_DMA->irq_st0;
        reg_irq_en = &System_DMA->irq_en0;
    } else if( irq == SDMA_IRQ_1 ){
        reg_status = &System_DMA->irq_st1;
        reg_irq_en = &System_DMA->irq_en1;
    } else if( irq == SDMA_IRQ_2 ){
        reg_status = &System_DMA->irq_st2;
        reg_irq_en = &System_DMA->irq_en2;
    } else if( irq == SDMA_IRQ_3 ){
        reg_status = &System_DMA->irq_st3;
        reg_irq_en = &System_DMA->irq_en3;
    }

    // read status
    irq_l = irq_f = __raw_readl (reg_status);

    for (i = 0; i < 32; i++){
        if( irq_l & 0x00000001 ){
            if(System_DMA_ctl[i]){
                tsDMA_ctl* ctrs = System_DMA_ctl[i];
                tsChannelDMA* chan = request_dma_channel(ctrs->channel);
                // Read Event status
                ctrs->dma_status = __raw_readl( &chan->DMA4_CSRi);
                if( ctrs->dma_status & (DMA_ALIGN_ERR | DMA_SUPER_ERR | DMA_TX_ERROR))
                    ctrs->trs_st = t_dma_ERROR;
                else
                    ctrs->trs_st = t_dma_COMPLETE;
                if(ctrs->handler)
                    ctrs->handler(ctrs->channel, ctrs->src, ctrs->dst, ctrs->bSZ, ctrs->trs_st ,ctrs->dma_status, ctrs->user_p);
                // end_count();
//                else
//                    printf("SDMA: Interrupt (%d): Channel: %d - ST 0x%x\n", irq, i, ctrs->dma_status );
                // Clear Event Status
                __raw_writel(0xFFFFFFFF, &chan->DMA4_CSRi);
               free(System_DMA_ctl[i]);
               System_DMA_ctl[i] = 0;
            }
            // disable more interrupts from this source
            u32 r = __raw_readl ( reg_irq_en );
            r = sbit_val( r, 1, i, 0);
            __raw_writel (r , reg_irq_en );
        }
        irq_l = irq_l >> 1;
    }
    __raw_writel (0xffffffff, reg_status);
}
#endif

void init_sys_dma (void)
{
    System_DMA = (tsDMA*) sDMA_BASE;
    // Initialize Interrupts - clear all
    __raw_writel (0, &System_DMA->irq_en0);
    __raw_writel (0, &System_DMA->irq_en1);
    __raw_writel (0, &System_DMA->irq_en2);
    __raw_writel (0, &System_DMA->irq_en3);
#ifdef __SDMA_IRQ__
    memset(System_DMA_ctl, 0, sizeof(System_DMA_ctl));
    register_irq (SDMA_IRQ_0, irq_sdma_handler);
#endif
}

inline tsChannelDMA* request_dma_channel (int channel)
{
    return (tsChannelDMA*) DMA4_CHANNEL_ADDR(channel);
}

void set_channel_source_destination_params (tsChannelDMA* chan, \
                                            enum dma_DATATYPE type, \
                                            enum dma_BURST ReadPortAccessType, \
                                            enum dma_BURST WritePortAccessType, \
                                            enum dma_ENDIAN SourceEndiansim, \
                                            enum dma_ENDIAN DestinationEndianism, \
                                            enum dma_WRMODE WriteMode, \
                                            enum dma_PACKET SourcePacked, \
                                            enum dma_PACKET DestinationPacked )
{
    u32 rVal = __raw_readl(&chan->DMA4_CSDPi);
    rVal = sbit_val (rVal, 2, 0, type);
    rVal = sbit_val (rVal, 2, 7, ReadPortAccessType);
    rVal = sbit_val (rVal, 2, 14, WritePortAccessType);
    rVal = sbit_val (rVal, 1, 21, SourceEndiansim);
    rVal = sbit_val (rVal, 1, 19, DestinationEndianism);
    rVal = sbit_val (rVal, 2, 16, WriteMode);
    rVal = sbit_val (rVal, 1, 6, SourcePacked);
    rVal = sbit_val (rVal, 1, 13, DestinationPacked);
    __raw_writel(rVal, &chan->DMA4_CSDPi);
}

void set_channel_setup_control (tsChannelDMA* chan, \
                                enum dma_ADDR_MODE ReadPortAccessMode, \
                                enum dma_ADDR_MODE WritePortAccessMode, \
                                enum dma_PRIORITY ReadPriority, \
                                enum dma_PRIORITY WritePriority, \
                                enum dma_PREFETCH Prefetch, \
                                u32 en_transparent_copy, \
                                u32 en_fill_copy, \
                                u32 block_sync, \
                                u32 frame_sync, \
                                u32 frame_sync_src, \
                                u32 sync_channel )
{
    u32 rVal = __raw_readl(&chan->DMA4_CCRi);
    rVal = sbit_val (rVal, 2, 12, ReadPortAccessMode);
    rVal = sbit_val (rVal, 2, 14, WritePortAccessMode);
    rVal = sbit_val (rVal, 1, 6,  ReadPriority);
    rVal = sbit_val (rVal, 1, 26, WritePriority);
    rVal = sbit_val (rVal, 1, 23, Prefetch);
    rVal = sbit_val (rVal, 1, 17, en_transparent_copy);   // transparent copy disabled
    rVal = sbit_val (rVal, 1, 16, en_fill_copy);   // fill copy disabled
    rVal = sbit_val (rVal, 1, 18, block_sync);   // block sync
    rVal = sbit_val (rVal, 1, 5,  frame_sync);    // Frame sync ON
    rVal = sbit_val (rVal, 1, 24, frame_sync_src);   // Frame sync on source
    if( frame_sync_src ){
        rVal = sbit_val (rVal, 5, 0, sync_channel + 1);
        rVal = sbit_val (rVal, 2, 19, (sync_channel + 1) >> 5);
    }
    else{
        /* set zero for disable sync */
        rVal = sbit_val (rVal, 5, 0, 0);
        rVal = sbit_val (rVal, 2, 19, 0);
    }
    __raw_writel(rVal, &chan->DMA4_CCRi);
}

void disable_dma_channel (tsChannelDMA* chan)
{
    __raw_writel(chan->DMA4_CCRi & ~(1 << 7), &chan->DMA4_CCRi);
}

void enable_dma_channel (tsChannelDMA* chan)
{
    __raw_writel(chan->DMA4_CCRi | (1 << 7), &chan->DMA4_CCRi);
}

int channel_dma_is_busy (tsChannelDMA* chan)
{
    return __raw_readl(&chan->DMA4_CCRi) & (1 << 7);
}

void channel_dma_wait_complete (tsChannelDMA* chan)
{
    while(channel_dma_is_busy (chan));
}

void set_channel_setup_tranfer (tsChannelDMA* chan, \
                                u32 ElementPerFrame, \
                                u32 FramePerTranferBlock, \
                                u32 SourceStartAddress, \
                                u32 DestinationStartAddress)
{
    __raw_writel(ElementPerFrame, &chan->DMA4_CENi);
    __raw_writel(FramePerTranferBlock, &chan->DMA4_CFNi);
    __raw_writel(SourceStartAddress, &chan->DMA4_CSSAi);
    __raw_writel(DestinationStartAddress, &chan->DMA4_CDSAi);
}

void set_channel_setup_index (tsChannelDMA* chan, \
                              u32 SourceElementIndex, \
                              u32 SourceFrameIndex, \
                              u32 DestinationElementIndex, \
                              u32 DestinationFrameIndex)
{
    __raw_writel (SourceElementIndex, &chan->DMA4_CSEIi);
    __raw_writel (SourceFrameIndex, &chan->DMA4_CSFIi);
    __raw_writel (DestinationElementIndex, &chan->DMA4_CDEIi);
    __raw_writel (DestinationFrameIndex, &chan->DMA4_CDFIi);
}

#ifdef __SDMA_IRQ__
void set_channel_interrupt_request (tsChannelDMA* chan, u32 irq, u32 service_request)
{
    u32 irq_v;
    u32 chan_num = DMA4_CHANNEL_ID((unsigned int) chan);
    // Set the IRQ service request
    __raw_writel(service_request, &chan->DMA4_CICRi);
    // Set IRQ Enable
    switch(irq){
        case SDMA_IRQ_0:
            irq_v = __raw_readl(&System_DMA->irq_en0);
            irq_v |= (1 << chan_num);
            __raw_writel(irq_v , &System_DMA->irq_en0 );
            break;
        case SDMA_IRQ_1:
            irq_v = __raw_readl(&System_DMA->irq_en1);
            irq_v |= (1 << chan_num);
            __raw_writel(irq_v , &System_DMA->irq_en1 );
            break;
        case SDMA_IRQ_2:
            irq_v = __raw_readl(&System_DMA->irq_en2);
            irq_v |= (1 << chan_num);
            __raw_writel(irq_v , &System_DMA->irq_en2 );
            break;
        case SDMA_IRQ_3:
            irq_v = __raw_readl(&System_DMA->irq_en3);
            irq_v |= (1 << chan_num);
            __raw_writel(irq_v , &System_DMA->irq_en3 );
            break;
    }
}

void set_channel_trs_ctl_setup (u32 channel, u32 src, u32 dst, u32 bSZ, u32 irq, __sdma_handler handler, void *u_p )
{
    tsDMA_ctl* nt = malloc (sizeof(tsDMA_ctl));
    nt->channel = channel;
    nt->src = src;
    nt->dst = dst;
    nt->bSZ = bSZ;
    nt->dma_status = 0;
    nt->trs_st = t_dma_WAIT;
    nt->handler = handler;
    nt->irq = irq;
    nt->user_p = u_p;
    System_DMA_ctl[channel] = nt;
}
#endif

#ifdef __SYS_DMA_DEBUG__
void print_channel (tsChannelDMA* chan)
{
    printf("IGEP-X-Loader: System DMA Channel dump (0x%.8p)\n", chan);
    printf("IGEP-X-Loader: CCR........[0x%.8p](0x%.8x)\n", &chan->DMA4_CCRi, chan->DMA4_CCRi);
    printf("IGEP-X-Loader: CLNK_CTRL..[0x%.8p](0x%.8x)\n", &chan->DMA4_CLNK_CTRLi, chan->DMA4_CLNK_CTRLi);
    printf("IGEP-X-Loader: CICR.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CICRi, chan->DMA4_CICRi);
    printf("IGEP-X-Loader: CSR........[0x%.8p](0x%.8x)\n", &chan->DMA4_CSRi, chan->DMA4_CSRi);
    printf("IGEP-X-Loader: CSDP.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CSDPi, chan->DMA4_CSDPi);
    printf("IGEP-X-Loader: CEN........[0x%.8p](0x%.8x)\n", &chan->DMA4_CENi,chan->DMA4_CENi);
    printf("IGEP-X-Loader: CFN........[0x%.8p](0x%.8x)\n", &chan->DMA4_CFNi, chan->DMA4_CFNi);
    printf("IGEP-X-Loader: CSSA.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CSSAi, chan->DMA4_CSSAi);
    printf("IGEP-X-Loader: CDSA.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CDSAi, chan->DMA4_CDSAi);
    printf("IGEP-X-Loader: CSEI.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CSEIi, chan->DMA4_CSEIi);
    printf("IGEP-X-Loader: CSFI.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CSFIi, chan->DMA4_CSFIi);
    printf("IGEP-X-Loader: CDEI.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CDEIi, chan->DMA4_CDEIi);
    printf("IGEP-X-Loader: CDFI.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CDFIi, chan->DMA4_CDFIi);
    printf("IGEP-X-Loader: CSAC.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CSACi, chan->DMA4_CSACi);
    printf("IGEP-X-Loader: CDAC.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CDACi, chan->DMA4_CDACi);
    printf("IGEP-X-Loader: CCEN.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CCENi, chan->DMA4_CCENi);
    printf("IGEP-X-Loader: CCFN.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CCFNi, chan->DMA4_CCFNi);
    printf("IGEP-X-Loader: COLOR......[0x%.8p](0x%.8x)\n", &chan->DMA4_COLORi, chan->DMA4_COLORi);
    printf("IGEP-X-Loader: CDP........[0x%.8p](0x%.8x)\n", &chan->DMA4_CDPi, chan->DMA4_CDPi);
    printf("IGEP-X-Loader: CNDP.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CNDPi, chan->DMA4_CNDPi);
    printf("IGEP-X-Loader: CCDN.......[0x%.8p](0x%.8x)\n", &chan->DMA4_CCDNi, chan->DMA4_CCDNi);
}

void show_dma_info (void)
{
    u32 *src, *dest;
    int i = 0, count_ok = 0, count_fail = 0;
    tsChannelDMA* chan = request_dma_channel (0);
    printf("IGEP-X-Loader: System DMA Registers dump\n");
    printf("IGEP-X-Loader: Revision: 0x%.8x\n", System_DMA->rev);
    printf("IGEP-X-Loader: IRQ Status 0 (0x48056008): 0x%.8x\n", System_DMA->irq_st0 );
    printf("IGEP-X-Loader: IRQ Status 1 (0x4805600C): 0x%.8x\n", System_DMA->irq_st1 );
    printf("IGEP-X-Loader: IRQ Status 2 (0x48056010): 0x%.8x\n", System_DMA->irq_st2 );
    printf("IGEP-X-Loader: IRQ Status 3 (0x48056014): 0x%.8x\n", System_DMA->irq_st3 );
    printf("IGEP-X-Loader: IRQ Enable 0 (0x48056018): 0x%.8x\n", System_DMA->irq_en0 );
    printf("IGEP-X-Loader: IRQ Enable 1 (0x4805601C): 0x%.8x\n", System_DMA->irq_en1 );
    printf("IGEP-X-Loader: IRQ Enable 2 (0x48056020): 0x%.8x\n", System_DMA->irq_en2 );
    printf("IGEP-X-Loader: IRQ Enable 3 (0x48056024): 0x%.8x\n", System_DMA->irq_en3 );
    printf("IGEP-X-Loader: System DMA Status (0x48056028): 0x%.8x\n", System_DMA->status );
    printf("IGEP-X-Loader: OCP Sys Config (0x4805602C): 0x%.8x\n", System_DMA->ocp_syscfg );
    printf("IGEP-X-Loader: CAPS0 (0x48056064): 0x%.8x\n", System_DMA->caps0 );
    printf("IGEP-X-Loader: CAPS2 (0x4805606C): 0x%.8x\n", System_DMA->caps2 );
    printf("IGEP-X-Loader: CAPS3 (0x48056070): 0x%.8x\n", System_DMA->caps3 );
    printf("IGEP-X-Loader: CAPS4 (0x48056074): 0x%.8x\n", System_DMA->caps4 );
    printf("IGEP-X-Loader: GCR (0x48056078): 0x%.8x\n", System_DMA->gcr );
    printf("IGEP-X-Loader: *********************************\n");
    /* test dma */
    src = malloc (1024);
    dest = malloc (2048);
    start_count();
    memset(src, 0xfAfAfAfA, 1024);
    invalidate_dcache(get_device_type());
    end_count();
    printf("memset time: %u ticks 1/26M\n", getCountTicks());
    memset(dest, 0x00000000, 1024);
    start_count();
    dma_memcpy(1, src, dest, 1024, 1);
#ifndef __SDMA_IRQ__
    end_count();
#endif
    printf("dma copy time: %u ticks 1/26M\n", getCountTicks());
    for (i=0; i < 1024 / 4; i++){
        if(src[i] == dest[i])
            count_ok++;
        else
            count_fail++;
    }
    printf("IGEP-X-Loader: DMA Test: OK = %d Fail = %d\n", count_ok, count_fail);
    printf("IGEP-X-Loader: *********************************\n");
    if(count_fail > 0){
        mem_dump(src, 1024/4);
        printf("IGEP-X-Loader: *********************************\n");
        mem_dump(dest, 1024/4);
        printf("IGEP-X-Loader: *********************************\n");
    }
}

#endif

#define DMA_TRANSPARENT_COPY_EN     1
#define DMA_TRANSPARENT_COPY_DIS    0
#define DMA_FILL_COPY_EN            1
#define DMA_FILL_COPY_DIS           0
#define DMA_BLOCK_SYNC_EN           1
#define DMA_BLOCK_SYNC_DIS          0
#define DMA_FRAME_SYNC_EN           1
#define DMA_FRAME_SYNC_DIS          0
#define DMA_FRAME_SYNC_SRC_EN       1
#define DMA_FRAME_SYNC_SRC_DIS      0

#define S_DMA_3                     3

void dma_memcpy (int channel, u32 source, u32 dest, u32 size, int wait_complete)
{
    tsChannelDMA* chan = request_dma_channel (channel);
    disable_dma_channel(chan);
#ifdef __SDMA_IRQ__
    set_channel_trs_ctl_setup(channel, source, dest, size, SDMA_IRQ_0, 0, 0);
#endif
    set_channel_source_destination_params(chan, t_dma_32BITS, t_dma_64B, t_dma_64B, t_dma_LE, t_dma_LE, t_dma_NONPOSTED, t_dma_NOPACKET, t_dma_NOPACKET);
    set_channel_setup_control(chan, t_dma_SINGLE_IDX, t_dma_SINGLE_IDX, t_dma_LOW_PRIO, t_dma_LOW_PRIO, t_dma_PREFETCH_DIS, DMA_TRANSPARENT_COPY_DIS, DMA_FILL_COPY_DIS, DMA_BLOCK_SYNC_DIS, DMA_FRAME_SYNC_DIS, DMA_FRAME_SYNC_SRC_DIS, 0);
    set_channel_setup_tranfer (chan, size / 4, 1, source, dest);
    set_channel_setup_index (chan, 1, 1, 1, 1);
#ifdef __SDMA_IRQ__
    set_channel_interrupt_request(chan, SDMA_IRQ_0, DMA_ALIGN_ERR | DMA_SUPER_ERR | DMA_TX_ERROR | DMA_LAST);
#endif
    enable_dma_channel(chan);
    if(wait_complete)
        channel_dma_wait_complete(chan);
}

static u32 vFill = 0;

void dma_fill (int channel, u32 dest, u32 size, int wait_complete, u32 value)
{
    tsChannelDMA* chan = request_dma_channel (channel);
    disable_dma_channel(chan);
    vFill = value;
#ifdef __SDMA_IRQ__
    set_channel_trs_ctl_setup(channel, source, dest, size, SDMA_IRQ_0, 0, 0);
#endif
    set_channel_source_destination_params(chan, t_dma_32BITS, t_dma_64B, t_dma_64B, t_dma_LE, t_dma_LE, t_dma_NONPOSTED, t_dma_NOPACKET, t_dma_NOPACKET);
    set_channel_setup_control(chan, t_dma_CONST, t_dma_SINGLE_IDX, t_dma_LOW_PRIO, t_dma_LOW_PRIO, t_dma_PREFETCH_DIS, DMA_TRANSPARENT_COPY_DIS, DMA_FILL_COPY_DIS, DMA_BLOCK_SYNC_DIS, DMA_FRAME_SYNC_DIS, DMA_FRAME_SYNC_SRC_DIS, 0);
    set_channel_setup_tranfer (chan, size / 4, 1, &vFill, dest);
    set_channel_setup_index (chan, 1, 1, 1, 1);
#ifdef __SDMA_IRQ__
    set_channel_interrupt_request(chan, SDMA_IRQ_0, DMA_ALIGN_ERR | DMA_SUPER_ERR | DMA_TX_ERROR | DMA_LAST);
#endif
    enable_dma_channel(chan);
    if(wait_complete)
        channel_dma_wait_complete(chan);
}


int dma_is_transfer_complete (int channel)
{
    return !channel_dma_is_busy( request_dma_channel(channel) );
}

int dma_check_transfer_error (u32 channel)
{
    tsChannelDMA* chan = request_dma_channel (channel);
    return __raw_readl ( &chan->DMA4_CSRi ) & (DMA_ALIGN_ERR | DMA_SUPER_ERR | DMA_TX_ERROR);
}

void dma_gpmc_transfer (int channel, u32 src, u32 dest, u32 bSZ)
{
    tsChannelDMA* chan = request_dma_channel (channel);
    disable_dma_channel(chan);
    set_channel_source_destination_params(chan, t_dma_32BITS, t_dma_64B, t_dma_64B, t_dma_LE, t_dma_LE, t_dma_NONPOSTED, t_dma_PACKET, t_dma_PACKET);
    set_channel_setup_control(chan, t_dma_CONST, t_dma_SINGLE_IDX, t_dma_HIGH_PRIO, t_dma_HIGH_PRIO, t_dma_PREFETCH_DIS, DMA_TRANSPARENT_COPY_DIS, DMA_FILL_COPY_DIS, DMA_BLOCK_SYNC_DIS, DMA_FRAME_SYNC_EN, DMA_FRAME_SYNC_SRC_EN, S_DMA_3);
    set_channel_setup_tranfer (chan, 64/4 , (bSZ / 64), src, dest);
    set_channel_setup_index (chan, 1, 64, 1, 64);
    enable_dma_channel(chan);
}

void dma_mmc_transfer (int channel, u32 src, u32 dest, u32 bSZ)
{
    tsChannelDMA* chan = request_dma_channel (channel);
    disable_dma_channel(chan);
    set_channel_source_destination_params(chan, t_dma_32BITS, t_dma_32B, t_dma_32B, t_dma_LE, t_dma_LE, t_dma_NONPOSTED, t_dma_NOPACKET, t_dma_NOPACKET);
    set_channel_setup_control(chan, t_dma_CONST, t_dma_SINGLE_IDX, t_dma_HIGH_PRIO, t_dma_HIGH_PRIO, t_dma_PREFETCH_DIS, DMA_TRANSPARENT_COPY_DIS, DMA_FILL_COPY_DIS, DMA_BLOCK_SYNC_DIS, DMA_FRAME_SYNC_DIS, DMA_FRAME_SYNC_SRC_DIS, 0);
    set_channel_setup_tranfer (chan, bSZ / 4, 1, src, dest);
    set_channel_setup_index (chan, 1, 1, 1, 1);
    enable_dma_channel(chan);
}
/* End Of File */
