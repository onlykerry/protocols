/*
+FHDR------------------------------------------------------------------
Copyright (c),
Tony Yang –51,AVR,ARM firmware developer  
Contact:qq 292942278  e-mail:tony_yang123@sina.com.cn
;;;;;;;;;;
Abstract:
Sector Read/Write Driver
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
$Id: flash_management.h,v 1.1.1.1 2007/02/26 14:01:12 Design Exp $
-FHDR-------------------------------------------------------------------
*/

/*
===============================================================================
函数
Read flash sector(512bytes)
入口：u8 *buf:缓冲区首地址,u32 sector:物理扇区号
出口：SUCC
===============================================================================
*/
extern u8 read_flash_sector(u8 *buf,u32 sector);
/*
===============================================================================
函数
Write flash sector(512bytes)
入口：u8 *buf:缓冲区首地址,u32 sector:物理扇区号
出口：SUCC
===============================================================================
*/
extern u8 write_flash_sector(u8 *buf,u32 sector);

/*
===============================================================================
函数
打开模拟磁盘IMG文件fat16.img
入口：无
出口：无
===============================================================================
*/
extern u8 flash_management_sysinit();
/*
+FFTR--------------------------------------------------------------------
$Log: flash_management.h,v $
Revision 1.1.1.1  2007/02/26 14:01:12  Design
volume_inquire函数测试成功

Revision 1.1.1.1  2007/02/24 07:59:45  yangwenbin
增加FAT16文件系统基本代码

-FFTR--------------------------------------------------------------------
*/