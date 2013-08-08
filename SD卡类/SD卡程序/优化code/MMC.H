
#ifndef __MMC_H___
#define __MMC_H___

#ifdef   MMC_GLOBALS
#define  MMC_EXT
#else
#define  MMC_EXT extern
#endif

//If use MMC-Card then set to 1
#define USE_MMC                 1       //EXPERIMENTAL NOT READY!!!!
#define HW_SPI_Mode             1       //1:hardware SPI 0:software SPI
/******************************************************************************/
/*M128-SPI*/
#define MMC_SS          PB0
#define MMC_SCK         PB1
#define MMC_MOSI        PB2
#define MMC_MISO        PB3

#define MMC_DDR         DDRB
#define MMC_PORT        PORTB
#define MMC_PIN         PINB

#define MMC_SS_H        MMC_PORT |= (1<<MMC_SS)
#define MMC_SS_L        MMC_PORT &= ~(1<<MMC_SS)
#define MMC_SCK_H       MMC_PORT |= (1<<MMC_SCK)
#define MMC_SCK_L       MMC_PORT &= ~(1<<MMC_SCK)
#define MMC_MOSI_H      MMC_PORT |= (1<<MMC_MOSI)
#define MMC_MOSI_L      MMC_PORT &= ~(1<<MMC_MOSI)

#define MMC_MISO_IN     (MMC_PIN&(1<<MMC_MISO))
//------------------------------------------------------------
// Error define
//-------------------------------------------------------------
#define INIT_CMD0_ERROR         0xFF
#define INIT_CMD1_ERROR         0xFE
#define WRITE_BLOCK_ERROR       0xFD
#define READ_BLOCK_ERROR        0xFC

// MMC/SD commands
#define MMC_RESET		0x40 + 0
#define MMC_INIT		0x40 + 1
#define MMC_READ_CSD		0x40 + 9
#define MMC_READ_CID		0x40 + 10
#define MMC_STOP_TRANSMISSION	0x40 + 12
#define MMC_SEND_STATUS		0x40 + 13
#define MMC_SET_BLOCKLEN	0x40 + 16
#define MMC_READ_BLOCK		0x40 + 17
#define MMC_READ_MULTI_BLOCK	0x40 + 18
#define MMC_WRITE_BLOCK		0x40 + 24
#define MMC_WRITE_MULTI_BLOCK	0x40 + 25

//set MMC_Chip_Select to high (MMC/SD-Card Invalid)
#define MMC_Disable()   MMC_SS_H
//set MMC_Chip_Select to low (MMC/SD-Card Active)
#define MMC_Enable()    MMC_SS_L


MMC_EXT uint8 MMC_Init(void);
MMC_EXT uint8 MMC_write_sector(uint32 addr,uint8 *Buffer);
MMC_EXT uint8 MMC_read_sector(uint32 addr,uint8 *Buffer);

#endif/*__MMC_H___*/