/*
+FHDR------------------------------------------------------------------
Copyright (c),
Tony Yang –51,AVR,ARM firmware developer  
Contact:qq 292942278  e-mail:tony_yang123@sina.com.cn
;;;;;;;;;;
Abstract:
Sector Read/Write Driver
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
$Id: flash_management.c,v 1.1.1.1 2007/02/26 14:01:12 Design Exp $
-FHDR-------------------------------------------------------------------
*/

#include "stdio.h"
#include "string.h"
#include <include\types.h>
FILE *file1;
/*
===============================================================================
函数
Read flash sector(512bytes)
入口：u8 *buf:缓冲区首地址,u32 sector:物理扇区号
出口：SUCC
===============================================================================
*/
u8 read_flash_sector(u8 *buf,u32 sector)
{
  fseek(file1,sector * 512,0);
  fread(buf,1,512,file1);
  return(SUCC);
}

/*
===============================================================================
函数
Write flash sector(512bytes)
入口：u8 *buf:缓冲区首地址,u32 sector:物理扇区号
出口：SUCC
===============================================================================
*/
u8 write_flash_sector(u8 *buf,u32 sector)
{
  fseek(file1,sector * 512,0);
  fwrite(buf,1,512,file1);
  return(SUCC);
}

/*
===============================================================================
函数
打开模拟磁盘IMG文件fat16.img
入口：无
出口：无
===============================================================================
*/
u8 flash_management_sysinit()
{
 if ((file1 = fopen("fat16.img","rb+")) == NULL)
     {
      printf("Open file %s failed!\n","fat16.img");   
      return(FAIL);
     }
     return(SUCC);
}

/*
+FFTR--------------------------------------------------------------------
$Log: flash_management.c,v $
Revision 1.1.1.1  2007/02/26 14:01:12  Design
volume_inquire函数测试成功

-FFTR--------------------------------------------------------------------
*/