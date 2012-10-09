#include <common.h>
typedef struct {
	unsigned int DataType;
	unsigned int ReadPortAccessType;
	unsigned int WritePortAccessType;
	unsigned int SourceEndiansim;
	unsigned int DestinationEndianism;
	unsigned int WriteMode;
	unsigned int SourcePacked;
	unsigned int DestinationPacked;
	unsigned int NumberOfElementPerFrame;
	unsigned int NumberOfFramePerTransferBlock;
	unsigned int SourceStartAddress;
	unsigned int DestinationStartAddress;
	unsigned int SourceElementIndex;
	unsigned int SourceFrameIndex;
	unsigned int DestinationElementIndex;
	unsigned int DestinationFrameIndex;
	unsigned int ReadPortAccessMode;
	unsigned int WritePortAccessMode;
	unsigned int ReadPriority;
	unsigned int WritePriority;
	unsigned int ReadRequestNumber;
	unsigned int WriteRequestNumber;
}DMA4_t;


DMA4_t DMA4;

#define DMA4_CSDP_CH10 (*((volatile unsigned int *) (0x48056090 + 0x60*10)))
#define DMA4_CEN_CH10 (*((volatile unsigned int *) (0x48056094 + 0x60*10)))
#define DMA4_CFN_CH10 (*((volatile unsigned int *) (0x48056098 + 0x60*10)))
#define DMA4_CSSA_CH10 (*((volatile unsigned int *) (0x4805609C + 0x60*10)))
#define DMA4_CDSA_CH10 (*((volatile unsigned int *) (0x480560A0 + 0x60*10)))
#define DMA4_CCR_CH10 (*((volatile unsigned int *) (0x48056080 + 0x60*10)))
#define DMA4_CSEI_CH10 (*((volatile unsigned int *) (0x480560A4 + 0x60*10)))
#define DMA4_CSFI_CH10 (*((volatile unsigned int *) (0x480560A8 + 0x60*10)))
#define DMA4_CDEI_CH10 (*((volatile unsigned int *) (0x480560AC + 0x60*10)))
#define DMA4_CDFI_CH10 (*((volatile unsigned int *) (0x480560B0 + 0x60*10)))
#define DMA4_CSRI_CH10 (*((volatile unsigned int *) (0x4805608C + 0x60*10)))
#define DMA4_CDACI_CH10 (*((volatile unsigned int *) (0x4805608B + 0x60*10)))

void dma_copy(unsigned int source, unsigned int dest,
	      unsigned int row_size,
	      unsigned int num_rows)
{
	unsigned int RegVal = 0;

	DMA4_CCR_CH10 &= ~(1 << 7); /* disable channel */


	/* Init. parameters */
	DMA4.DataType = 0x2; // DMA4_CSDPi[1:0] -> 0: 8 bits .. 1: 16 bits .. 2: 32 bits
	DMA4.ReadPortAccessType = 0; // DMA4_CSDPi[8:7]
	DMA4.WritePortAccessType = 0; // DMA4_CSDPi[15:14]
	DMA4.SourceEndiansim = 0; // DMA4_CSDPi[21]
	DMA4.DestinationEndianism = 0; // DMA4_CSDPi[19]
	DMA4.WriteMode = 0; // DMA4_CSDPi[17:16]
	DMA4.SourcePacked = 0; // DMA4_CSDPi[6]
	DMA4.DestinationPacked = 0; // DMA4_CSDPi[13]
	DMA4.NumberOfElementPerFrame = row_size; // DMA4_CENi
	DMA4.NumberOfFramePerTransferBlock = num_rows; // DMA4_CFNi
	DMA4.SourceStartAddress = source; // DMA4_CSSAi
	DMA4.DestinationStartAddress = dest; // DMA4_CDSAi
	DMA4.SourceElementIndex = 1; // DMA4_CSEIi
	DMA4.SourceFrameIndex = 1; // DMA4_CSFIi
	DMA4.DestinationElementIndex = 1; // DMA4_CDEIi
	DMA4.DestinationFrameIndex = 1; // DMA4_CDFIi
	DMA4.ReadPortAccessMode = 0; // DMA4_CCRi[13:12] - Constat address mode
	DMA4.WritePortAccessMode = 2; // DMA4_CCRi[15:14] - Double index
	DMA4.ReadPriority = 0; // DMA4_CCRi[6]
	DMA4.WritePriority = 0; // DMA4_CCRi[23]
	DMA4.ReadRequestNumber = 0; // DMA4_CCRi[4:0]
	DMA4.WriteRequestNumber = 0; // DMA4_CCRi[20:19]
	/* 1) Configure the transfer parametres in the logical DMA registers */
	/*-------------------------------------------------------------------*/
	/* a) Set the data type CSDP[1:0], the Read/Write Port access type CSDP[8:7]/[15:14], the
	Source/dest endiansim CSDP[21]/CSDP[19], write mode CSDP[17:16], source/dest packed or nonpacked
	CSDP[6]/CSDP[13]*/
	// Read CSDP
	RegVal = DMA4_CSDP_CH10;
	// Build reg
	RegVal = ((RegVal & ~ 0x3) | DMA4.DataType );
	RegVal = ((RegVal & ~(0x3 << 7)) | (DMA4.ReadPortAccessType << 7));
	RegVal = ((RegVal & ~(0x3 << 14)) | (DMA4.WritePortAccessType << 14));
	RegVal = ((RegVal & ~(0x1 << 21)) | (DMA4.SourceEndiansim << 21));
	RegVal = ((RegVal & ~(0x1 << 19)) | (DMA4.DestinationEndianism << 19));
	RegVal = ((RegVal & ~(0x3 << 16)) | (DMA4.WriteMode << 16));
	RegVal = ((RegVal & ~(0x1 << 6)) | (DMA4.SourcePacked << 6));
	RegVal = ((RegVal & ~(0x1 << 13)) | (DMA4.DestinationPacked << 13));
	RegVal = RegVal | (0x0 << 7);
	RegVal = RegVal | (0x0 << 14);
	// Write CSDP
	DMA4_CSDP_CH10 = RegVal;
	/* b) Set the number of element per frame CEN[23:0]*/
	DMA4_CEN_CH10 = DMA4.NumberOfElementPerFrame;
	/* c) Set the number of frame per block CFN[15:0]*/
	DMA4_CFN_CH10 = DMA4.NumberOfFramePerTransferBlock;
	/* d) Set the Source/dest start address index CSSA[31:0]/CDSA[31:0]*/
	DMA4_CSSA_CH10 = DMA4.SourceStartAddress; // address start
	DMA4_CDSA_CH10 = DMA4.DestinationStartAddress; // address dest
	/* e) Set t1he Read Port adressing mode CCR[13:12], the Write Port adressing mode CCR[15:14],
	read/write priority CCR[6]/CCR[26], the current LCH CCR[20:19]=00 and CCR[4:0]=00000*/
	// Read CCR
	RegVal = DMA4_CCR_CH10;
	// Build reg
	RegVal = ((RegVal & ~(0x3 << 12)) | (DMA4.ReadPortAccessMode << 12));
	RegVal = ((RegVal & ~(0x3 << 14)) | (DMA4.WritePortAccessMode << 14));
	RegVal = ((RegVal & ~(0x1 << 6)) | (DMA4.ReadPriority << 6));
	RegVal = ((RegVal & ~(0x1 << 26)) | (DMA4.WritePriority << 26));
	RegVal&= 0xFFCFFFE0 ;
	// Write CCR
	DMA4_CCR_CH10 = RegVal;
	/* f)- Set the source element index CSEI[15:0]*/
	DMA4_CSEI_CH10 = DMA4.SourceElementIndex;
	/* g)- Set the source frame index CSFI[15:0]*/
	DMA4_CSFI_CH10 = DMA4.SourceFrameIndex ;
	/* h)- Set the destination element index CDEI[15:0]*/
	DMA4_CDEI_CH10 = DMA4.DestinationElementIndex;
	/* i)- Set the destination frame index CDFI[31:0]*/
	DMA4_CDFI_CH10 = DMA4.DestinationFrameIndex;
	/* 2) Start the DMA transfer by Setting the enable bit CCR[7]=1 */
	/*--------------------------------------------------------------*/
	//write enable bit
	DMA4_CCR_CH10 |= 1 << 7; /* start */

	// while(DMA4_CCR_CH10 & (1<< 7));
}

int dma_is_busy ()
{
    return DMA4_CCR_CH10 & (1<< 7);
}

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

u32 dma_get_channel_status ()
{
	return DMA4_CSRI_CH10;
}

void dma_reset_status ()
{
	DMA4_CSRI_CH10 = 0;
}

/*u32 dma_current_dest_address ()
{
	return DMA4_CDACI_CH10;
}*/


#ifdef __notdef
unsigned int do_dma_test(unsigned int source, unsigned int dest,
	      unsigned int size)
{
	unsigned int i, j, err = 0;
	unsigned int *tmp_src = (unsigned char *)source;
	unsigned int *tmp_dst = (unsigned char *)dest;
	for (i = 0; i < size/4; i++)
		*tmp_src++ = i;

	dma_copy(source, dest, size/16/4, 16);

	tmp_src = source;
	for (i = 0; i < size/4; i++)
		if (*tmp_dst++ != *tmp_src++) {
			printf("\nDMA test failed at 0x%08x", tmp_dst-1);
			err = 1;
		}
	return err;
}

void dma_test(void)
{
	unsigned int i = 0, j, ret, chunk;
	unsigned int steps = 1, step_size ;
	while(1) {
		printf("Iteration %d: ", ++i);
		for (j = 0; j < steps; j++) {
			step_size = 512 / steps;
			ret = do_dma_test(0x80000000 + j * step_size * 1024 * 1024,
				          0xa0000000 + j * step_size * 1024 * 1024,
				          step_size * 1024 * 1024);
			if (ret)
				return;
		}
		printf("successful!\n", i);
	}

}
#endif
