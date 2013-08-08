/*
+FHDR------------------------------------------------------------------
Copyright (c),
Tony Yang –51,AVR,ARM firmware developer  
Contact:qq 292942278  e-mail:tony_yang123@sina.com.cn

Abstract:
$Id: main.C,v 1.12 2007/05/10 11:13:14 design Exp $
-FHDR-------------------------------------------------------------------
*/
#include "stdio.h"
#include "include\types.h"
#include "fat\fat.h"
#include "Flash_Management\Flash_Management.h" 
#include "include\FAT_cfg.h" 

/*
===============================================================================
函数
main();
入口：无
出口：无
===============================================================================
*/ 
void main()
{
  u32 Volume_Capacity,Volume_FreeSpace;
  u8 mode,ATTR;
  u8 buf[280];
  flash_management_sysinit();
  FAT_filesystem_initialiation();
  //运行volume_inquiry
  volume_inquiry('c',&Volume_Capacity,&Volume_FreeSpace);
  //打印volume_inquiry结果
  printf("Volume Capacity: %ld\n",Volume_Capacity);
  printf("Volume FreeSpace: %ld\n",Volume_FreeSpace);
  printf("\nPress any key to continue!");
  scanf("%c",&mode);
  //设置disk_enumeration模式为从C盘根目录开始枚举
  mode = 0; 
  while(disk_enumeration(buf,mode,&ATTR) == SUCC)
  {  mode = 1; 
     //打印成功枚举的directory entry
     printf("\nreaded entry=%s Attr = %x",buf,ATTR);
	 printf("\nPress any key to continue!");
	 scanf("%c",buf);//按任意键继续枚举
  } 

}
   
/*
+FFTR--------------------------------------------------------------------
$Log: main.C,v $
Revision 1.12  2007/05/10 11:13:14  design
改进函数static u32 Get_Previous_Cluster_From_Current_Cluster(u32 Cluster)
解决连续删除文件死机的问题。
测试方法：在OK目录下连续写500个长文件名文件，再删除测试成功

Revision 1.11  2007/03/31 14:25:25  design
给read_file(),write_file()增加一种优化的实现方法
对应fat_cfg.h增加两个编绎宏
#define Read_File_Optimization_Selector 1
#define Write_File_Optimization_Selector 1

Revision 1.10  2007/03/25 03:54:08  design
注：v1.9建目录成功，目录分离未成功

Revision 1.9  2007/03/25 03:42:24  design
create_file(),create_folder(),rename_file()测试通过
create_file(),create_folder()测试方法如下：
在目录test,ok下分别建500个文件，目录，之后用disk_enumeration分
离成功
rename_file()测试方法如下：
将文件"C:\TEST.PDF"改名为"DFDFDFDFDFDFDFDFSDFSDTONY.TXT"成功

Revision 1.8  2007/03/17 03:10:19  design
write_file测试成功
测试方法：
写3m rar文件，再读出，解压通过

Revision 1.7  2007/03/11 10:23:33  yangwenbin
disk_enumeration,folder_enumeration函数测试成功
测试方法:
分离大小为127M IMG的所含文件，如rar,pdf等

Revision 1.6  2007/03/06 14:49:13  yangwenbin
cd_folder()函数测试成功

Revision 1.5  2007/03/06 12:26:49  yangwenbin
函数cd_folder()完成， mode 0/mode1两种模式测试通过

Revision 1.4  2007/03/04 03:00:53  yangwenbin
open_file(),f_seek(),read_file(),
文件：C:\longfilename directory for test\tony yang and\test_test.txt
测试成功

Revision 1.3  2007/03/03 15:39:37  yangwenbin
open_file,f_seek,read_file
读1.9M文件测试成功

Revision 1.2  2007/02/28 13:39:22  Design
函数open_file打开绝对路径+短文件名测试通过
使用C代码如下：
open_file("C:\\HELLO.TXT")

Revision 1.1.1.1  2007/02/24 07:59:45  yangwenbin
增加FAT16文件系统基本代码

-FFTR--------------------------------------------------------------------
*/

