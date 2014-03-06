#ifndef __DM3730_SYSDMA_H__
#define __DM3730_SYSDMA_H__

#define DMA4_CHANNEL_ADDR(n) (0x48056080 + (n * 0x60))
#define DMA4_CHANNEL_ID(n) ((n - 0x48056080) / 0x60)

typedef struct {
    u32 DMA4_CCRi;          /* 0x4805 6080 + (i * 0x60) i = channel (0 to 31) */
    u32 DMA4_CLNK_CTRLi;    /* 0x4805 6084 + (i * 0x60) */
    u32 DMA4_CICRi;         /* 0x4805 6088 + (i * 0x60) */
    u32 DMA4_CSRi;          /* 0x4805 608C + (i * 0x60) */
    u32 DMA4_CSDPi;         /* 0x4805 6090 + (i * 0x60) */
    u32 DMA4_CENi;          /* 0x4805 6094 + (i * 0x60) */
    u32 DMA4_CFNi;          /* 0x4805 6098 + (i * 0x60) */
    u32 DMA4_CSSAi;         /* 0x4805 609C + (i * 0x60) */
    u32 DMA4_CDSAi;         /* 0x4805 60A0 + (i * 0x60) */
    u32 DMA4_CSEIi;         /* 0x4805 60A4 + (i * 0x60) */
    u32 DMA4_CSFIi;         /* 0x4805 60A8 + (i * 0x60) */
    u32 DMA4_CDEIi;         /* 0x4805 60AC + (i * 0x60) */
    u32 DMA4_CDFIi;         /* 0x4805 60B0 + (i * 0x60) */
    u32 DMA4_CSACi;         /* 0x4805 60B4 + (i * 0x60) */
    u32 DMA4_CDACi;         /* 0x4805 60B8 + (i * 0x60) */
    u32 DMA4_CCENi;         /* 0x4805 60BC + (i * 0x60) */
    u32 DMA4_CCFNi;         /* 0x4805 60C0 + (i * 0x60) */
    u32 DMA4_COLORi;        /* 0x4805 60C4 + (i * 0x60) */
    u32 DMA4_res[2];        /* 0x4805 60C8, 0x4805 60CC */
    u32 DMA4_CDPi;          /* 0x4805 60D0 + (i * 0x60) */
    u32 DMA4_CNDPi;         /* 0x4805 60D4 + (i * 0x60) */
    u32 DMA4_CCDNi;         /* 0x4805 60D8 + (i * 0x60) */
} tsChannelDMA;

#define sDMA_BASE        0x48056000

typedef struct {
    u32 rev;            /* 0x48056000 */
    u32 res0;           /* 0x48056004 */
    u32 irq_st0;        /* 0x48056008 + (j * 4) where j = 0 : 0x48056008 */
    u32 irq_st1;        /* 0x48056008 + (j * 4) where j = 1 : 0x4805600C */
    u32 irq_st2;        /* 0x48056008 + (j * 4) where j = 2 : 0x48056010 */
    u32 irq_st3;        /* 0x48056008 + (j * 4) where j = 3 : 0x48056014 */
    u32 irq_en0;        /* 0x48056018 + (j * 4) where j = 0 : 0x48056018 */
    u32 irq_en1;        /* 0x48056018 + (j * 4) where j = 1 : 0x4805601C */
    u32 irq_en2;        /* 0x48056018 + (j * 4) where j = 2 : 0x48056020 */
    u32 irq_en3;        /* 0x48056018 + (j * 4) where j = 3 : 0x48056024 */
    u32 status;         /* 0x48056028 */
    u32 ocp_syscfg;     /* 0x4805602C */
    u32 res1[13];       /* 0x48056030 to 0x48056064 */
    u32 caps0;          /* 0x48056064 */
    u32 res2;           /* 0x48056068 */
    u32 caps2;          /* 0x4805606C */
    u32 caps3;          /* 0x48056070 */
    u32 caps4;          /* 0x48056074 */
    u32 gcr;            /* 0x48056078 */
} tsDMA;

void init_sys_dma (void);
void show_dma_info (void);

tsChannelDMA* request_dma_channel (int channel /*0 to 31*/);
void disable_dma_channel (tsChannelDMA* chan);
void enable_dma_channel (tsChannelDMA* chan);
int channel_dma_is_busy (tsChannelDMA* chan);
void channel_dma_wait_complete (tsChannelDMA* chan);

enum dma_DATATYPE       { t_dma_32BITS = 2, t_dma_16BITS = 1, t_dma_8BITS = 0 };
enum dma_PACKET         { t_dma_NOPACKET = 0 , t_dma_PACKET = 1 };
enum dma_BURST          { t_dma_SINGLE = 0, t_dma_16B = 1, t_dma_32B = 2, t_dma_64B = 3};
// 0x1: 16 bytes or 4x32-bit/2x64-bit burst access
// 0x2: 32 bytes or 8x32-bit/4x64-bit burst access
// 0x3: 64 bytes or 16x32-bit/8x64-bit burst access
enum dma_WRMODE         { t_dma_NONPOSTED = 0, t_dma_POSTED = 1, t_dma_POST_NON_LAST = 2};
enum dma_ENDIAN_LOCK    { t_dma_ADAPT = 0, t_dma_LOCK = 1};
enum dma_ENDIAN         { t_dma_LE = 0, t_dma_BE = 1};

void set_channel_source_destination_params (tsChannelDMA* chan, \
                                            enum dma_DATATYPE type, \
                                            enum dma_BURST ReadPortAccessType, \
                                            enum dma_BURST WritePortAccessType, \
                                            enum dma_ENDIAN SourceEndiansim, \
                                            enum dma_ENDIAN DestinationEndianism, \
                                            enum dma_WRMODE WriteMode, \
                                            enum dma_PACKET SourcePacked, \
                                            enum dma_PACKET DestinationPacked );

enum dma_ADDR_MODE      { t_dma_CONST = 0, t_dma_POST_INC = 1, t_dma_SINGLE_IDX = 2, t_dma_DOUBLE_IDX = 3};
enum dma_PRIORITY       { t_dma_LOW_PRIO = 0, t_dma_HIGH_PRIO = 1 };
enum dma_PREFETCH       { t_dma_PREFETCH_DIS = 0, t_dma_PREFETCH_EN = 1};

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
                                u32 sync_channel );


void set_channel_setup_tranfer (tsChannelDMA* chan, \
                                u32 ElementPerFrame, \
                                u32 FramePerTranferBlock, \
                                u32 SourceStartAddress, \
                                u32 DestinationStartAddress);

void set_channel_setup_index (tsChannelDMA* chan, \
                              u32 SourceElementIndex, \
                              u32 SourceFrameIndex, \
                              u32 DestinationElementIndex, \
                              u32 DestinationFrameIndex);


void dma_memcpy (int channel, u32 source, u32 dest, u32 size, int wait_complete);
void dma_fill (int channel, u32 dest, u32 size, int wait_complete, u32 value);
void dma_gpmc_transfer (int channel, u32 src, u32 dest, u32 bSZ);
void dma_mmc_transfer (int channel, u32 src, u32 dest, u32 bSZ);
int dma_is_transfer_complete (int channel);
int dma_check_transfer_error (u32 channel);

#endif
