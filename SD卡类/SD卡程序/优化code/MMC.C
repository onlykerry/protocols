/**************************************************************************************
//------------------ MMC/SD-Card Reading and Writing implementation -------------------
//FileName     : mmc.c
//Function     : Connect AVR to MMC/SD
//Created by   : ZhengYanbo
//Created date : 15/08/2005
//Version      : V1.2
//Last Modified: 19/08/2005
//Filesystem   : Read or Write MMC without any filesystem

//CopyRight (c) 2005 ZhengYanbo
//Email: Datazyb_007@163.com
****************************************************************************************/
/*2006-1-7 10:52  重新编写了所有的代码*/
#define   MMC_GLOBALS

#include  "config.h"

#if USE_MMC

// Port Init
void MMC_Port_Init(void)
//****************************************************************************
{
        MMC_PORT        |=  (1<<MMC_SCK)|(1<<MMC_SS);
        MMC_DDR         |=  (1<<MMC_SCK)|(1<<MMC_MOSI)|(1<<MMC_SS);
        MMC_DDR         &= ~(1<<MMC_MISO);
}

//****************************************************************************
//Routine for Init MMC/SD card(SPI-MODE)
uint8 MMC_Init(void)
//****************************************************************************
{
        uint8 retry,temp;
        uint8 i;

        MMC_Port_Init(); //Init SPI port
        SPCR=0x53;
        SPSR=0x00;

        for (i=0;i<0x0f;i++)
        {
                SPI_TransferByte(0xff); //send 74 clock at least!!!
        }

        MMC_Enable();
        
        SPI_TransferByte(MMC_RESET);
        SPI_TransferByte(0x00);
        SPI_TransferByte(0x00);
        SPI_TransferByte(0x00);
        SPI_TransferByte(0x00);
        SPI_TransferByte(0x95);
        
        SPI_TransferByte(0xff);
        SPI_TransferByte(0xff);
        

        retry=0;
        do{ 
                temp=Write_Command_MMC(MMC_INIT,0);
                retry++;
                if(retry==100)
                {
                        MMC_Disable();
                        return(INIT_CMD1_ERROR);
                }
        }while(temp!=0);

        MMC_Disable();
        
        SPCR=0x50;
        SPSR=0x01;
        return(TRUE);
}

//****************************************************************************
//Send a Command to MMC/SD-Card
//Return: the second byte of response register of MMC/SD-Card
uint8 Write_Command_MMC(uint8 cmd,uint32 address)
//****************************************************************************
{
        uint8 tmp;
        uint8 retry=0;
        
        SPI_TransferByte(cmd);
        SPI_TransferByte(address>>24);
        SPI_TransferByte(address>>16);
        SPI_TransferByte(address>>8);
        SPI_TransferByte(address);
        SPI_TransferByte(0xFF);
        
        SPI_TransferByte(0xFF);
        
        do{
                tmp = SPI_TransferByte(0xFF);
                retry++;
        }while((tmp==0xff)&&(retry<8));
        return(tmp);
}


//****************************************************************************
//Routine for writing a Block(512Byte) to MMC/SD-Card
//Return 0 if sector writing is completed.
uint8 MMC_write_sector(uint32 addr,uint8 *Buffer)
//****************************************************************************
{
        uint8 temp;
        uint16 i;
        
        SPI_TransferByte(0xFF);
        MMC_Enable();
        
        temp = Write_Command_MMC(MMC_WRITE_BLOCK,addr<<9);
        if(temp != 0x00)
        {
                MMC_Disable();
                return(temp);
        }
        
        SPI_TransferByte(0xFF);
        SPI_TransferByte(0xFF);
        
        SPI_TransferByte(0xFE);

        for (i=0;i<512;i++)
        {
                SPI_TransferByte(*Buffer++); //send 512 bytes to Card
        }

        //CRC-Byte
        SPI_TransferByte(0xFF); //Dummy CRC
        SPI_TransferByte(0xFF); //CRC Code

        temp = SPI_TransferByte(0xFF);   // read response
        if((temp & 0x1F)!=0x05) // data block accepted ?
        {
                MMC_Disable();
                return(WRITE_BLOCK_ERROR); //Error!
        }
        
        while (SPI_TransferByte(0xFF) != 0xFF);

        MMC_Disable();
        return(TRUE);
}
//****************************************************************************
//Routine for reading Blocks(512Byte) from MMC/SD-Card
//Return 0 if no Error.
uint8 MMC_read_sector(uint32 addr,uint8 *Buffer)
//****************************************************************************
{
        uint8 temp;
        uint16 i;
        
        SPI_TransferByte(0xff);
        
        MMC_Enable();
        
        temp = Write_Command_MMC(MMC_READ_BLOCK,addr<<9);
        
        if(temp != 0x00)
        {
                MMC_Disable();
                return(READ_BLOCK_ERROR);
        }
        
        while(SPI_TransferByte(0xff) != 0xfe);
        
        for(i=0;i<512;i++)
        {
                *Buffer++ = SPI_TransferByte(0xff);
        }
        
        SPI_TransferByte(0xff);
        SPI_TransferByte(0xff);
        
        MMC_Disable();
        return(TRUE);
}

//----------------------------------------------------------------------------
#endif //USE_MMC

