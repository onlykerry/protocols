/*
+FHDR------------------------------------------------------------------
Copyright (c),
Tony Yang –51,AVR,ARM firmware developer  
Contact:qq 292942278  e-mail:tony_yang123@sina.com.cn

Abstract:
$Id: fat.c,v 1.1.1.1 2007/05/11 03:53:06 design Exp $
-FHDR-------------------------------------------------------------------
*/ 
#include<include\types.h>
#include<Flash_Management\Flash_Management.h>   
#include<include\FAT_cfg.h>         
//Current Directory Entry 
static struct Directory_Entry_ Directory_Entry;
//CORE of FAT filesystem
static struct core_ CORE;
//BPB
static struct partition_bpb BPB; 
//Define FCBs for FileRead/Write  
struct FileControlBlock FCB[MaximumFCB];
/*
===============================================================================
函数
字符串转换为大写
入口：*string:字符串首地址
出口：SUCC
===============================================================================
*/ 
static u8 UPCASE(u8* string) 
{ 
 while(*string) 
 { 
     if(*string >='a' && *string <= 'z')
      {
  	   *string -= 32;
 	   
      }
	 string++;
   }    
 return(SUCC);
}
/*
===============================================================================
函数
测试字符串长度
入口：*string:字符串首地址
出口：字符串长度
===============================================================================
*/ 
static u16 LengthofString(u8 * string)
{ 
 u16 i;
 i = 0;
 while(*string)
  {
    i++;
    string++;
  }
 return(i);
} 
/*
===============================================================================
函数
连接字符串2到字符串1之后,连接后字符串2无变化
入口：*string1:字符串1首地址,*string2:字符串2首地址
出口：SUCC,FAIL
===============================================================================
*/ 
static u8 concanenateString(u8 *string1,u8 *string2)
{
  u8 len,i;
  len = LengthofString(string1);
  i = 0;
  while(string2[i])
  {
    string1[len] = string2[i];  
    len++;
	i++;
  }
  string1[len] = 0;
  return(SUCC);
}

/*
===============================================================================
函数
字符串copy
入口：*string1:源字符串首地址；*string2:目标字符串首地址
出口：SUCC
===============================================================================
*/ 
static u8 stringcpy(u8 *string1,u8 *string2)
{
 while(*string1) 
   { 
     *string2 = *string1;
     string1++;
     string2++;
   }  
 *string2 = 0;
 return(SUCC);
}
/*
===============================================================================
函数
字符串比较(不区分大小写)
入口：*string1:字符串1首地址；*string2:字符串2首地址
出口：SUCC,FAIL
===============================================================================
*/ 
static u8 stringcmp(u8 *string1,u8 *string2)
{
 UPCASE(string1);
 UPCASE(string2);  
 while((*string1) && (*string2)) 
   {
     if((*string1) == (*string2))
      {
         string1++;
         string2++;
      }
     else
       return(FAIL);
   }   
   
 if( ((*string1) == 0) && ((*string2) == 0))
 {
     return(SUCC);
 }
 else
     return(FAIL);
}

/*
===============================================================================
函数
在簇链里找当前簇的下一簇
入口：Cluster:当前簇号
出口: 返回下一簇号
===============================================================================
*/ 
static u32 Get_Next_Cluster_From_Current_Cluster(u32 Cluster)
{
   u8 buf[512]; 
   u32 ThisFATSecNum,ThisFATEntOffset;
   //FAT16
   ThisFATEntOffset = Cluster * 2;
   ThisFATSecNum = CORE.relative_sector + BPB.reserved_sector 
                   + (ThisFATEntOffset / BPB.bytes_per_sector);
   ThisFATEntOffset = ThisFATEntOffset % BPB.bytes_per_sector; 
   read_flash_sector(buf,ThisFATSecNum);
   return((u32)buf[ThisFATEntOffset] + ((u32)buf[ThisFATEntOffset + 1]) * 256);
}

/*
===============================================================================
函数
Given any valid data cluster number N,
Return the sector number of the first sector of that cluster
入口：u32 N:data cluster number N
出口: RETURN first sector of that cluster
===============================================================================
*/  
static u32 FirstSectorofCluster(u32 N)
{
 return((N - 2) * BPB.sector_per_cluster + CORE.FirstDataSector);
}

/*
===============================================================================
函数(CORE_INIT_1)
根据boot sector分区表计算得出分区partition_ID的relative_sector,total_sector等
入口：partition_ID(支持4个分区0,1,2,3)
出口: 无
===============================================================================
*/
static void BPB_INIT_1(u8 partition_ID,u8 *buf)
{
  //read_flash_sector(buf,0); //read MBR 
  CORE.relative_sector = 0;
  CORE.total_sector =  0xfffffff;
  CORE.system_id = buf[0x1c2]; //Partition Type 0C-FAT32,06-FAT16 ect..
  CORE.PartitionID= 'C' + partition_ID; //从C开始到Z结束      
}  
/*
===============================================================================
函数(CORE_INIT_1)
从root sector BPB计算FirstDataSector,FirstRootDirSecNum,etc..
入口：无
出口: 无
===============================================================================
*/
static void BPB_INIT_2(void)
{  
  CORE.RootDirSectors = (BPB.boot_entries * 32 + (BPB.bytes_per_sector - 1)) / BPB.bytes_per_sector;                             
  //The start of the data region, the first sector of cluster 2                               
  CORE.FirstDataSector = CORE.relative_sector + BPB.reserved_sector + BPB.sectors_per_FAT
                               * BPB.numbers_of_FAT + CORE.RootDirSectors;
  CORE.FirstRootDirSecNum = CORE.relative_sector + BPB.reserved_sector+ BPB.sectors_per_FAT
                               * BPB.numbers_of_FAT;
  ////////////////////////////printf("CORE.total_sector:%ld",CORE.total_sector);
  CORE.DataSec = CORE.total_sector - BPB.reserved_sector - CORE.RootDirSectors
                 - BPB.sectors_per_FAT * BPB.numbers_of_FAT;
  CORE.CountofClusters = CORE.DataSec / BPB.sector_per_cluster;
  CORE.FirstSectorofFAT1 = CORE.relative_sector + BPB.reserved_sector;
  CORE.FirstSectorofFAT2 = CORE.relative_sector + BPB.reserved_sector + BPB.sectors_per_FAT;
}
/*
===============================================================================
函数
Read Partition PBP
入口：Partition ID
出口：无
===============================================================================
*/ 
static u8 Read_partition_PBP(u8 partition_ID)
{  
  u8 *ptr1,*ptr2; 
  u16 i; 
  u8 buf[512];  
   if ((CORE.PartitionID - 'C') ==  partition_ID) 
    { 
	  //Specific BPB is already readed in the buffer
      return(SUCC);
    }  
   read_flash_sector(buf,0); //read MBR 
   if ( buf[0x1be] == 0x00 || buf[0x1be] == 0x80) // check boot indicator 00 or 0x80
    {
     BPB_INIT_1(partition_ID,buf);
     //read DBR of the specific partition into buffer 
     if ( read_flash_sector(buf,CORE.relative_sector) ) 
      if ( buf[510] == 0x55 && buf[511] == 0xaa)
       {
        ptr1 = (u8 *)(&BPB.bytes_per_sector);
        ptr2 = buf + 0xb/*BPB position offset in MBR*/;         
        for (i = 0;i < sizeof(BPB); i ++)   //copy BPB(BIOS Parameter Block) to RAM buffer
          *(ptr1 + i) = *(ptr2 + i);
        BPB_INIT_2();
        return(SUCC);
       }     
     }
     else
     {
	   CORE.relative_sector = 0; 
       CORE.total_sector =  0x3ad10;
       CORE.system_id = buf[0x1c2]; //Partition Type 0C-FAT32,06-FAT16 ect..
       CORE.PartitionID= 'C' + partition_ID; //从C开始到Z结束 
	   if ( buf[510] == 0x55 && buf[511] == 0xaa)
       {
          //u16 bytes_per_sector;//每扇区字节数
          BPB.bytes_per_sector = buf[0xb] + buf[0xc] * 256;
          //  u8 sector_per_cluster; //每簇扇区数
          BPB.sector_per_cluster = buf[0xd];
          //  u16 reserved_sector;  //保留扇区数
          BPB.reserved_sector = buf[14] + buf[15] * 256;
          //  u8 numbers_of_FAT;//FAT副本数
          BPB.numbers_of_FAT = buf[16];
          //  u16 boot_entries;//根目录项数，供FAT12/16使用
          BPB.boot_entries = buf[17] + buf[18] * 256;
          //  u16 TotSec16; //This field is the old 16-bit total count of sectors on the volume.
          BPB.TotSec16 = buf[19] + buf[20] * 256;
          BPB.TotSec16 = 0xffff;
          //  u8 media_descriptor; //媒体描述符
          BPB.media_descriptor = buf[21];
          //  u16 sectors_per_FAT; //每个FAT表占用的扇区数，供FAT12/16使用
          BPB.sectors_per_FAT =  buf[22] + buf[23] * 256;
          CORE.system_id = 0x6; //Partition Type 0C-FAT32,06-FAT16 ect..
          BPB_INIT_2();
          return(SUCC);
       } 
	 }  
  return(FAIL);
}  

/*
===============================================================================
函数
从路径中读一个entry
入口：
出口：SUCC,FAIL
===============================================================================
*/
static u8 SplitNameFromPath(u8 *Path,u8 *Return_Entry_Name,u8 *FileExtension,u8 *RemovedCharsFromPath)
{  
  u8 i,flag,j;
  flag = FILE_NAME;
  *RemovedCharsFromPath = 0; 
  CORE.CurPathType = DirectoryPath; 
  i = 0; 
  j = 0;
  do{           
     if( ( * Path) != 0 && ( * Path ) != '\\') //Path分离得到Entry name and file extension 
       { 
        (*RemovedCharsFromPath)++; 
        if( flag == FILE_NAME)
         {            
          if(*Path == '.')
             {
              flag  = FILE_EXTENSION;
              Path ++; 
             }  
          else
            {
             Return_Entry_Name[i] =  *Path;
             i++; 
             Path++;
            }
          }
        else if( flag  == FILE_EXTENSION)
         {
          if( j >= 3 )
            return(FAIL);
          FileExtension[j] =  *Path;
          j++;
          Path++;
         } 
      }
    else
      {
       if(!( * Path))
        {
          if(CORE.FullPathType == FilePath)
		  {
		  CORE.CurPathType = FilePath;
		  }

          FileExtension[j] = 0;
          Return_Entry_Name[i] = 0;
          return(LastSplitedNameofPath);  
        } 
       (*RemovedCharsFromPath)++; 
       FileExtension[j] = 0;
       Return_Entry_Name[i] = 0;	   
       break;
      }
 }while(1); 
 return(SUCC);
} 
/*
===============================================================================
函数
Directory Entry offset+32 
入口：buf--Current Sector Buffer
出口：SUCC,FAIL
===============================================================================
*/             
static u8 CORE_offset_add_32(u8 *buf)
{
  CORE.offset += 32;
  if (CORE.offset >= 512)
  {
  if (CORE.DirectoryType == RootDirectory)
   {
        if (CORE.SectorNum < ( CORE.RootDirSectors +  CORE.FirstRootDirSecNum))
         {
           CORE.SectorNum++;
           CORE.offset = 0; 
           read_flash_sector(buf,CORE.SectorNum);
		   if(buf[CORE.offset] == 0 && buf[CORE.offset+1] == 0)  //End of the directory
		     return(FAIL);
           return(SUCC);
         }
        else
           return(FAIL);
     }
    else  
     {
        if( (CORE.SectorNum - FirstSectorofCluster(CORE.ClusterNum) + 1) >= BPB.sector_per_cluster)
         {
           CORE.ClusterNum = Get_Next_Cluster_From_Current_Cluster(CORE.ClusterNum);
           if( CORE.ClusterNum >= 2 && CORE.ClusterNum <= 0xfff7)
            {
               CORE.SectorNum = FirstSectorofCluster(CORE.ClusterNum); 
               CORE.offset = 0;
               read_flash_sector(buf,CORE.SectorNum);
			   if(buf[CORE.offset] == 0 && buf[CORE.offset+1] == 0)  //End of the directory
                   return(FAIL);
               return(SUCC);
            }
           else
                return(FAIL);
         }
        else
         {
            CORE.SectorNum++; 
            CORE.offset = 0;
            read_flash_sector(buf,CORE.SectorNum);
			if(buf[CORE.offset] == 0 && buf[CORE.offset+1] == 0)  //End of the directory
                return(FAIL);
            return(SUCC);
         }
     }
  }
  if(buf[CORE.offset] == 0 && buf[CORE.offset+1] == 0)  //End of the directory
       return(FAIL);
  return(SUCC);
}
/*
===============================================================================
函数
从目录读一个EntryWith 8.3 Name
入口：
出口：SUCC,FAIL
===============================================================================
*/ 
static u8 GetEntryWith8_3Name(u8 *buf,u8* EntryName,u8 *Extension)
{
  u8 j;
  struct Directory_Entry_  *Directory_Entry_Local;  
  u8 *pointer;
  pointer = buf;
  Directory_Entry_Local = (struct Directory_Entry_ *) (pointer + CORE.offset);
  for(j = 0;j < 8;j++)
   {
    if(Directory_Entry_Local->filename[j] == 0x20)
      break;
    EntryName[j] = Directory_Entry_Local->filename[j];
   }   
  EntryName[j] = 0; 
  for(j = 0;j < 3;j++)
   {  
    if(Directory_Entry_Local->file_extention[j] == 0x20)
        break;
    Extension[j] = Directory_Entry_Local->file_extention[j]; 
   }
  Extension[j] = 0;

  if(CORE.FullPathType == FilePath && CORE.CurPathType == FilePath)
     CORE.FileSize = *(u32*)Directory_Entry_Local->file_length;
    Directory_Entry_Local->file_length[0] = 0;
  Directory_Entry_Local->file_length[0] = 0;
  CORE.ClusterOfDirectoryEntry = (Directory_Entry_Local->first_cluster_number_low2bytes[0]) + 
	  (Directory_Entry_Local->first_cluster_number_low2bytes[1]) * 256;
  CORE.PreEntrySectorNum = CORE.SectorNum;
  CORE.PreEntryoffset = CORE.offset;
  CORE_offset_add_32(buf);//Directory Entry offset + 32 
  return(SUCC);
}
/*
===============================================================================
函数
从目录读一个EntryWithLongFileName
入口：
出口：SUCC,FAIL
===============================================================================
*/ 
static u8 GetEntryWithLongFileName(u8 *buf,u8* longFileName,u8 *Extension)
{
 u8 j,FileNameOffset;                  
 u8 flag,k,i;
 u16 len;
 struct LongNameDirectoryEntry *LongNameDirectoryEntry_Local;
 *Extension = 0;
 FileNameOffset = 242;
 LongNameDirectoryEntry_Local = (struct LongNameDirectoryEntry *) (buf + CORE.offset);
 do{
	 //flag = FILE_NAME;
	 k = FileNameOffset;
     for(j = 1;j < 10;j+=2)
      {        
       if (LongNameDirectoryEntry_Local->dir_lname1[j] == 0)
          break;   
       longFileName[k] = LongNameDirectoryEntry_Local->dir_lname1[j];
       k ++;                 
      } 
	 longFileName[k] = 0;
       if(j >= 10)
         { 
           for(j = 0;j < 12;j += 2)
            {  
              if (LongNameDirectoryEntry_Local->dir_lname2[j] == 0)
                 break;

                 longFileName[k] = LongNameDirectoryEntry_Local->dir_lname2[j];
                 k++;           
            }
           if(j >= 12)   
                for(j = 0;j < 4;j += 2)
                 { 
                  if (LongNameDirectoryEntry_Local->dir_lname3[j] == 0)
                     break;       
                    longFileName[k] = LongNameDirectoryEntry_Local->dir_lname3[j];
                    k ++;                   
                 }
          }
	 if(k > 242)
	  longFileName[k] = 0;
	CORE.PreEntrySectorNum = CORE.SectorNum;
    CORE.PreEntryoffset = CORE.offset;
	if(CORE_offset_add_32(buf) == FAIL) //Directory Entry offset + 32 
       return(FAIL);
    FileNameOffset -= 13;
    k = FileNameOffset;
    LongNameDirectoryEntry_Local = (struct LongNameDirectoryEntry *) (buf + CORE.offset);
    if(LongNameDirectoryEntry_Local->dir_attr != ATTR_LONG_NAME)
     { 
           if ( ! (LongNameDirectoryEntry_Local->dir_attr & ATTR_VOLUME_ID)) 
           {
			CORE.ClusterOfDirectoryEntry = LongNameDirectoryEntry_Local->dir_first[0]+
				LongNameDirectoryEntry_Local->dir_first[1] * 256;

            CORE.FileSize = *((u32*)LongNameDirectoryEntry_Local->dir_lname3);
			stringcpy(longFileName+FileNameOffset+13,longFileName);
			len =  LengthofString(longFileName);
			len --;
			i = 0;
			do{
				if(longFileName[len] == '.')
				{
				  longFileName[len] = 0;
                  stringcpy(longFileName + len + 1,Extension);
				  break;
				}
			   len--;
			   i++;
			   if(i >= 4 )
				 break;
			}while(1);

            break;
           }
       flag = FILE_NAME;
       FileNameOffset = 256 - 13;
       k = FileNameOffset; 
	   do{
		  CORE.PreEntrySectorNum = CORE.SectorNum;
          CORE.PreEntryoffset = CORE.offset;
          if(CORE_offset_add_32(buf) == FAIL) //Directory Entry offset + 32 
            return(FAIL); 
          LongNameDirectoryEntry_Local = (struct LongNameDirectoryEntry *) (buf + CORE.offset);
	      if(LongNameDirectoryEntry_Local->dir_lname1[0] == 0xe5)
             continue;
	      if(LongNameDirectoryEntry_Local->dir_attr != ATTR_LONG_NAME)
		  {
	       if ( ! (LongNameDirectoryEntry_Local->dir_attr & ATTR_VOLUME_ID)) 
            return(GetEntryWith8_3Name(buf,longFileName,Extension));
		   else
			 continue;
		  }
	      else
		   break;
	   } while(1);
     } 
  }while(1);  
  return(SUCC);
 }

              
/*
===============================================================================
函数
从目录读一个Entry
入口：mode = 0：返回所有directory entry
出口：SUCC,FAIL
===============================================================================
*/     
static u8 GetEntryFromDirectory(u8 *EntryName, u8 *Extension,u8 mode)
{ 
struct Directory_Entry_  *Directory_Entry_Local; 
struct LongNameDirectoryEntry *LongNameDirectoryEntry_Local; 
u8 flag; 
u8 buf[512];
read_flash_sector(buf,CORE.SectorNum);
do{  
  flag = FILE_NAME;  //or = FILE_EXTENSION 0xfe 
  Directory_Entry_Local = (struct Directory_Entry_ *) (buf + CORE.offset);
  if(Directory_Entry_Local->filename[0] == 0x0)
	  return(FAIL);
  if(Directory_Entry_Local->filename[0] == 0xe5)
  {
   CORE.PreEntrySectorNum = CORE.SectorNum;
   CORE.PreEntryoffset = CORE.offset;
   if(CORE_offset_add_32(buf) == FAIL) //Directory Entry offset + 32 
     return(FAIL);
   continue;
  }  
  switch(Directory_Entry_Local->file_attribute) 
    {
       case ATTR_LONG_NAME:{
          if(GetEntryWithLongFileName(buf,EntryName,Extension) == SUCC)
          {	
		   read_flash_sector(buf,CORE.SectorNum);
           LongNameDirectoryEntry_Local = (struct LongNameDirectoryEntry *)(buf + CORE.offset);
		   CORE.Entry_Attr = LongNameDirectoryEntry_Local->dir_attr;
		   if(mode == Get_Selected_ENTRIES)
		   {
             if(CORE.CurPathType == DirectoryPath && 
               (LongNameDirectoryEntry_Local->dir_attr & ATTR_DIRECTORY))
			 { 
               CORE.ClusterOfDirectoryEntry = *(u16*)LongNameDirectoryEntry_Local->dir_first;
               CORE.PreEntrySectorNum = CORE.SectorNum;
               CORE.PreEntryoffset = CORE.offset;
			   CORE_offset_add_32(buf);//Directory Entry offset + 32 
               return(SUCC);
			 }
             else if ( ! (LongNameDirectoryEntry_Local->dir_attr & ATTR_VOLUME_ID)) 
			 {
              CORE.PreEntrySectorNum = CORE.SectorNum;
             CORE.PreEntryoffset = CORE.offset;
			 CORE_offset_add_32(buf);//Directory Entry offset + 32 
             return(SUCC);
             }	   
		   }
		   else
		   {
			CORE.PreEntrySectorNum = CORE.SectorNum;
            CORE.PreEntryoffset = CORE.offset;
		    CORE.ClusterOfDirectoryEntry = *(u16*)LongNameDirectoryEntry_Local->dir_first;
            CORE_offset_add_32(buf);//Directory Entry offset + 32 
            
			return(SUCC);
		   }
          }
          break;
        }
       case ATTR_DIRECTORY:{
		  CORE.Entry_Attr = Directory_Entry_Local->file_attribute;
		  if(mode == Get_Selected_ENTRIES)
           if(CORE.FullPathType == FilePath && CORE.CurPathType == FilePath)
             break;
          if(GetEntryWith8_3Name(buf,EntryName,Extension)  == SUCC)
             return(SUCC);       
          break;
       }
       case ATTR_VOLUME_ID:CORE.Entry_Attr = Directory_Entry_Local->file_attribute;////////////////////////printf("Tony");break;
       case 0:break;
       default:
        {
		 CORE.Entry_Attr = Directory_Entry_Local->file_attribute;
	     if(mode == Get_Selected_ENTRIES)
          if(CORE.FullPathType == DirectoryPath)
             break;

          return(GetEntryWith8_3Name(buf,EntryName,Extension)); 
        }
     } 
    CORE.PreEntrySectorNum = CORE.SectorNum;
  CORE.PreEntryoffset = CORE.offset;
  if(CORE_offset_add_32(buf) == FAIL) //Directory Entry offset + 32 
    return(FAIL);
 }while(1); 
return(SUCC);
}  
/*
===============================================================================
函数
从目录中找一个Entry
入口：
出口：SUCC,FAIL
===============================================================================
*/
static u8 FindEntryStruct(u8 *floder_name,u8 *file_extension)
{  
   u8 EntryName[256],Extension[4]; 
   u8 Name_Compare_OK,Extension_Compare_OK;   
   do{		   
          if(GetEntryFromDirectory(EntryName,Extension,Get_Selected_ENTRIES) != SUCC)
		  {
            return(FAIL);
		  }
      
       Name_Compare_OK = OK;
       if(stringcmp(EntryName,floder_name) != SUCC)
             Name_Compare_OK = unOK;        
       if(Name_Compare_OK == OK)  //检查文件扩展名
         {      
           if(CORE.FullPathType == FilePath && CORE.CurPathType == FilePath)
              { 
					
                  Extension_Compare_OK = OK;     
                  if(stringcmp(Extension,file_extension) != SUCC)
                     Extension_Compare_OK = unOK;
                  else
                     break;  
              }
            else
			{  	
                if(Extension[0] == 0) 
                 break;

              }  
          }
     }while(1);
   return(SUCC);
}
/*
===============================================================================
函数
Relative Path converts To Sector,SectorOffset,Cluster
入口：u8 *filename
出口：SUCC,FAIL
===============================================================================
*/
static u8 RelativePathToSectorCluster(u8 *RelativePath)
{ 
  u8 floder_name[256],file_extension[4]; 
  u8 Splited_Count;
  u8 Splited_Status;
  Splited_Status = SplitNameFromPath(RelativePath,floder_name,file_extension,&Splited_Count);
  if(Splited_Status == FAIL)
      return(FAIL);
  RelativePath += Splited_Count;  
  if(FindEntryStruct(floder_name,file_extension) != SUCC)
  {
   return(FAIL); 
  }
   if(CORE.CurPathType == DirectoryPath)
     if(CORE.DirectoryType == RootDirectory)
	  {
	  CORE.DirectoryType = NoneRootDirectory; 
	  }
      
  if(Splited_Status == LastSplitedNameofPath)
  {
   return(SUCC); 
  }    
  do{ 
     Splited_Status = SplitNameFromPath(RelativePath,floder_name,file_extension,&Splited_Count);
     if(Splited_Status == FAIL)
	  return(FAIL);    
     else 
       { 
         CORE.ClusterNum = CORE.ClusterOfDirectoryEntry;
         CORE.SectorNum = FirstSectorofCluster(CORE.ClusterNum);
         CORE.offset = 0;  
       }
     RelativePath += Splited_Count;
     if(CORE.CurPathType == DirectoryPath)
      if(CORE.DirectoryType == RootDirectory)
	  {
	  CORE.DirectoryType = NoneRootDirectory; 
	  }
     if(FindEntryStruct(floder_name,file_extension) != SUCC)
	 {
       return(FAIL); 
	 }
     else if(Splited_Status == LastSplitedNameofPath)
	 {
      return(SUCC);
	 }  

    }while(1);
    return(SUCC);
}
/*
===============================================================================
函数
Full Path converts To Sector,SectorOffset,Cluster
入口：u8 *filename
出口：SUCC,FAIL(u32 *cluster_no,u32 *sector,u16 *offset)
===============================================================================
*/
static u8 FullPathToSectorCluster(u8 *filename1)
{
   u8 buf[280];
   u8 *filename;
   filename = buf;
   stringcpy(filename1,filename);
   UPCASE(filename);
     if( ((* filename) >= 'C' && ( * filename ) <= 'Z')||
		((* filename) >= 'c' && ( * filename ) <= 'z') )  //从指定盘符根目录开始寻址
       {
         if(( * (filename + 1)) == ':')
          {                          
           if( *( filename + 2 ) == '\\')
            { 
              if(LengthofString(filename) > Maximum_File_Path_Name)
			      return(EpathLengthsOVERFLOW);
              if(Read_partition_PBP((u8)((*filename) - 'C')) != SUCC)
                 return(FAIL);
              filename += 3; 
              CORE.SectorNum = CORE.FirstRootDirSecNum; 
              CORE.DirectoryType =  RootDirectory;
              CORE.offset = 0;   
             }
          }
         else 
          {   
             if((LengthofString(filename) + LengthofString(CORE.current_folder)) > Maximum_File_Path_Name)
                   return(EpathLengthsOVERFLOW);
             if(CORE.CurrentDirectoryType ==  RootDirectory)
			 {
			    CORE.SectorNum = CORE.FirstRootDirSecNum;
			 }  
             else
              {
                 CORE.ClusterNum = CORE.ClusterNOofCurrentFolder;
                 CORE.SectorNum = FirstSectorofCluster(CORE.ClusterNum); 
              }
             CORE.DirectoryType = CORE.CurrentDirectoryType;
             CORE.offset = 0;	 
          }  
       }
     else if((* filename) == '\\')
            {
             if((LengthofString(filename) + 1) > Maximum_File_Path_Name)
                   return(EpathLengthsOVERFLOW); 

             filename ++;    //从当前盘符，根目录开始寻址
             CORE.SectorNum = CORE.FirstRootDirSecNum; 
             CORE.DirectoryType = RootDirectory;
             CORE.offset = 0;
            }   
  if(*filename)
  return(RelativePathToSectorCluster(filename));
  else
	return(SUCC);
   
}





/*
===============================================================================
函数
FAT file system initialiation
入口：无
出口：无
===============================================================================
*/ 
#if complie_FAT_filesystem_initialiation
u8 FAT_filesystem_initialiation()
{ 
  u8 root[] = "C:\\",i; 
  Directory_Entry.filename[0]  = 0;
  CORE.PartitionID = 0xff;
  CORE.CurrentDirectoryType =  RootDirectory; 
  stringcpy(root,CORE.current_folder);
  for (i = 0; i < MaximumFCB;i++)
  {
   FCB[i].file_openned_flag = UnusedFlag; //UsedFlag
   FCB[i].Modified_Flag = 0;
  }
  //read defalut partition BPB and related information to RAM buffer
  return(Read_partition_PBP(0)); 
} 
#endif

/*
===============================================================================
函数
进入目录--更新当前目录
入口：foldername:目录名，
      mode: 0-- 进入子目录; >0--返回上一层目录
出口：SUCC,FAIL
===============================================================================
*/
#if compile_cd_folder
u8 cd_folder(u8 *foldername,u8 mode)
{ 
  u16 offset;
  if(mode)  //返回上一层目录
   {
    if (CORE.CurrentDirectoryType == RootDirectory)
     return(0x55);
    else
    {
	 CORE.FullPathType = DirectoryPath;
     if(FullPathToSectorCluster(CORE.current_folder) != SUCC)
	   return(FAIL);
     offset = LengthofString(CORE.current_folder);
     offset --;
     do{
      if(CORE.current_folder[offset] != '\\')
		  CORE.current_folder[offset] = 0;
      else
      {
	   if(CORE.current_folder[offset-1] == ':')
		 break;
       CORE.current_folder[offset] = 0;
       break;
      }
       offset--;
     }while(1);        

     if(LengthofString(CORE.current_folder) <= 3)
       CORE.CurrentDirectoryType = RootDirectory;
	 else
       CORE.CurrentDirectoryType = NoneRootDirectory;
     return(SUCC);
    }
   }
   else  //进入子目录
   {
    CORE.FullPathType = DirectoryPath;      
    if(FullPathToSectorCluster(foldername) == SUCC)
	{ 
	  if(((* foldername) >= 'C' && ( * foldername ) <= 'Z') || ((* foldername) >= 'c' && ( * foldername ) <= 'z'))
	  {
       if(* (foldername + 1) == ':' &&  * (foldername + 2 ) == '\\')
		  {
           stringcpy(foldername,CORE.current_folder);  
		  }
		  else
		  { 
			if(LengthofString(CORE.current_folder) != 3)
			{
			 CORE.current_folder[LengthofString(CORE.current_folder)] = '\\';
             CORE.current_folder[LengthofString(CORE.current_folder) + 1] = 0;
			}
		    concanenateString(CORE.current_folder,foldername);
		  }
	  }
	  else if(*foldername == '\\')
	  {
	      stringcpy(foldername,CORE.current_folder + 1);
	  }
	 if(LengthofString(CORE.current_folder) <= 3)
	 {
      CORE.SectorNum = CORE.FirstRootDirSecNum; 
	  CORE.CurrentDirectoryType = RootDirectory; 
	 }
	 else
	 {
	   CORE.CurrentDirectoryType = NoneRootDirectory;
       CORE.ClusterNum = CORE.ClusterOfDirectoryEntry;
	   CORE.ClusterNOofCurrentFolder = CORE.ClusterOfDirectoryEntry;
       CORE.SectorNum =  FirstSectorofCluster(CORE.ClusterNum); 
	 }
	 CORE.offset = 0;
	 return(SUCC);
	}   
   }
   return(FAIL);
}
#endif 
/*
===============================================================================
函数
列举目录下所有文件和目录
入口：无
出口：无
===============================================================================
*/  
#if complie_folder_dir
u8 folder_enumeration(u8 *return_string,u8 mode,u8 *ATTR)
{ 
  u8  Extension[4];
  u16 temp;
  if(mode == 0x0)
  {
   CORE.FullPathType = DirectoryPath; 
   if(cd_folder(CORE.current_folder,0) != SUCC)
	   return(FAIL);
   CORE.offset = 0;
   if(CORE.CurrentDirectoryType ==  RootDirectory)
      CORE.SectorNum = CORE.FirstRootDirSecNum; 
   else
     {
       CORE.ClusterNum = CORE.ClusterNOofCurrentFolder;
       CORE.SectorNum = FirstSectorofCluster(CORE.ClusterNum); 
     }
   CORE.DIR_ENUM_ClusterNum = CORE.ClusterNum;   //存放当前Enumerated Directory Entry所在Directory的ClusterNum,SectorNum,offset
   CORE.DIR_ENUM_SectorNum =CORE.SectorNum ;
   CORE.DIR_ENUM_offset = CORE.offset;
   CORE.DIR_ENUM_ClusterOfDirectoryEntry = CORE.ClusterOfDirectoryEntry;  //存放Directory Entry32字节中对应first Cluster Num
   CORE.DIR_ENUM_DirectoryType = CORE.DirectoryType; 
   CORE.DIR_ENUM_FullPathType = CORE.FullPathType;
   CORE.DIR_ENUM_CurPathType = CORE.CurPathType; 
  } 
   CORE.ClusterNum = CORE.DIR_ENUM_ClusterNum;   //存放当前Enumerated Directory Entry所在Directory的ClusterNum,SectorNum,offset
   CORE.SectorNum = CORE.DIR_ENUM_SectorNum;
   CORE.offset = CORE.DIR_ENUM_offset;
   CORE.ClusterOfDirectoryEntry = CORE.DIR_ENUM_ClusterOfDirectoryEntry;  //存放Directory Entry32字节中对应first Cluster Num
   CORE.DirectoryType = CORE.DIR_ENUM_DirectoryType; 
   CORE.FullPathType = CORE.DIR_ENUM_FullPathType;
   CORE.CurPathType = CORE.DIR_ENUM_CurPathType; 
  stringcpy(CORE.current_folder,return_string);
  temp = LengthofString(return_string);
  if(return_string[temp-1] != '\\')
  {
   return_string[temp] = '\\';
   return_string[temp+1] = 0;
  }
  if(GetEntryFromDirectory(return_string+LengthofString(return_string), Extension,Get_All_Entries) == SUCC)
  {
   temp = LengthofString(return_string);
   *ATTR = CORE.Entry_Attr;
   if(temp > 0 && (!((*ATTR) & ATTR_DIRECTORY)))
   {
    if(Extension[0] != 0)
	{
     return_string[temp] = '.';
     return_string[temp+1] = 0;
	 concanenateString(return_string,Extension);
	}
   }
   CORE.DIR_ENUM_ClusterNum = CORE.ClusterNum;   //存放当前Enumerated Directory Entry所在Directory的ClusterNum,SectorNum,offset
   CORE.DIR_ENUM_SectorNum =CORE.SectorNum ;
   CORE.DIR_ENUM_offset = CORE.offset;
   CORE.DIR_ENUM_ClusterOfDirectoryEntry = CORE.ClusterOfDirectoryEntry;  //存放Directory Entry32字节中对应first Cluster Num
   CORE.DIR_ENUM_DirectoryType = CORE.DirectoryType; 
   CORE.DIR_ENUM_FullPathType = CORE.FullPathType;
   CORE.DIR_ENUM_CurPathType = CORE.CurPathType; 
   return(SUCC);
  }
  return(FAIL);
} 
#endif
/*
===============================================================================
函数
列举disk中所有文件和目录
入口：无
出口：无
===============================================================================
*/ 
u8 disk_enumeration(u8 *return_string,u8 mode,u8* ATTR)
{
 u16 temp; 
 do{ 
    if( ! mode)
	{
     cd_folder("C:\\",0);
	 if(folder_enumeration(return_string,0,ATTR) == FAIL)
         return(FAIL);
	 mode = 1;
	}
    else if(folder_enumeration(return_string,1,ATTR) == FAIL)
	{
		do{ 		  
          if(cd_folder("C",1) == 0x55)
		    return(FAIL);
   CORE.DIR_ENUM_ClusterNum = CORE.ClusterNum;   //存放当前Enumerated Directory Entry所在Directory的ClusterNum,SectorNum,offset
   CORE.DIR_ENUM_SectorNum =CORE.SectorNum ;
   CORE.DIR_ENUM_offset = CORE.offset;
   CORE.DIR_ENUM_ClusterOfDirectoryEntry = CORE.ClusterOfDirectoryEntry;  //存放Directory Entry32字节中对应first Cluster Num
   CORE.DIR_ENUM_DirectoryType = CORE.CurrentDirectoryType; 
   CORE.DIR_ENUM_FullPathType = CORE.FullPathType;
   CORE.DIR_ENUM_CurPathType = CORE.CurPathType; 
	      if(folder_enumeration(return_string,1,ATTR) == SUCC)
		    break;
	  }while(1);
	}
  if((*ATTR) & ATTR_DIRECTORY)
  {   
      temp = LengthofString(return_string);
	  if(return_string[temp - 1] == '.' || (return_string[temp - 1] == '.' &&
		  return_string[temp - 2] == '.') )
		  continue;
    #if filter_hidden_entry
	  if ((*ATTR) & ATTR_HIDDEN)
		  continue;
    #endif 
	  if(cd_folder(return_string,0) == FAIL)
	    return(FAIL);
   CORE.DIR_ENUM_ClusterNum = CORE.ClusterNum;   //存放当前Enumerated Directory Entry所在Directory的ClusterNum,SectorNum,offset
   CORE.DIR_ENUM_SectorNum =CORE.SectorNum ;
   CORE.DIR_ENUM_offset = CORE.offset;
   CORE.DIR_ENUM_ClusterOfDirectoryEntry = CORE.ClusterOfDirectoryEntry;  //存放Directory Entry32字节中对应first Cluster Num
   CORE.DIR_ENUM_DirectoryType = CORE.DirectoryType; 
   CORE.DIR_ENUM_FullPathType = CORE.FullPathType;
   CORE.DIR_ENUM_CurPathType = CORE.CurPathType; 
	}
    return(SUCC);
 }while(1);
  
}


/*
===============================================================================
函数
查询分区容量和剩余空间 --通过检查分区FAT表来实现
入口：partition_id(Supported ID:form 'C' to 'F'),u32 *volume_capacity, u32 *volume_free_space
出口：SUCC  (返回的分区容量和剩余空间以512 bytes扇区为单位)
===============================================================================
*/  
#if complie_volume_inquiry 
u8 volume_inquiry(u8 partition_id,u32 *volume_capacity, u32 *volume_free_space)
{   
  u16 i,j,x;
  u8 buf[512];
  if(partition_id >= 'c' && partition_id <= 'f')
       partition_id -= 32;   //convert to upper case
  if ( ! (partition_id >= 'C' && partition_id <= 'F'))
    return(FAIL); 
  Read_partition_PBP((u8)(partition_id - 'C'));
   if (CORE.system_id == 0x6)//FAT16
    { 
     *volume_free_space = 0;
	 i = 0;
	 x = 0;
	 while(i < (CORE.CountofClusters) + 2){
      if(read_flash_sector(buf,CORE.FirstSectorofFAT1 + x) == SUCC) 
       {
        for (j = 0;j < 512;j +=2)
         { 
          if(buf[j] == 0 && buf[j + 1] == 0)
             (*volume_free_space) ++;
		  i++;
		  if(i >= (CORE.CountofClusters + 2))
			  break;
         }
		 x++;
        }
       else
        {
          *volume_capacity = 0;
          *volume_free_space = 0;
          return(FAIL);
         } 
	 }
       *volume_free_space *=  BPB.sector_per_cluster;
       *volume_capacity =  CORE.total_sector;  
       return(SUCC);
   }
   else
   {
      *volume_capacity = 0;
      *volume_free_space = 0;
      return(FAIL);   
   }
}
#endif
/*
+FFTR--------------------------------------------------------------------
$Log: fat.c,v $
Revision 1.15  2007/05/11 03:35:26  design
在函数FullPathToSectorCluster()增加一个280字节的缓冲区，
用于存入上层软件提供的path，然后使用UPCASE(),
解决上层软件path使用rom存入，不能执行UPCASE()的问题

Revision 1.14  2007/05/11 03:00:55  design
Remove_LONGFILENAME函数bug removed,
解决连续删除文件，一些目录项不能删除后建立文件出错的问题,
解决目录枚举目录最后标记(目录项第一个字节为0)判断失误的问题

Revision 1.13  2007/05/10 11:13:23  design
改进函数static u32 Get_Previous_Cluster_From_Current_Cluster(u32 Cluster)
解决连续删除文件死机的问题。
测试方法：在OK目录下连续写500个长文件名文件，再删除测试成功

Revision 1.12  2007/03/31 14:53:51  design
Bug Removed--函数GetEntryFromDirectory(u8 *EntryName, u8 *Extension,u8 mode)
 bug语句：if(mode = Seclect..)
改下后正确语名为：if(mode == Select..)

Revision 1.11  2007/03/31 14:25:34  design
给read_file(),write_file()增加一种优化的实现方法
对应fat_cfg.h增加两个编绎宏
#define Read_File_Optimization_Selector 1
#define Write_File_Optimization_Selector 1

Revision 1.10  2007/03/25 03:42:03  design
create_file(),create_folder(),rename_file()测试通过
create_file(),create_folder()测试方法如下：
在目录test,ok下分别建500个文件，目录，之后用disk_enumeration分
离成功
rename_file()测试方法如下：
将文件"C:\TEST.PDF"改名为"DFDFDFDFDFDFDFDFSDFSDTONY.TXT"成功

Revision 1.9  2007/03/17 03:15:06  design
write_file测试成功
测试方法：
写3m rar文件，再读出，解压通过
 
Revision 1.8  2007/03/11 13:39:47  yangwenbin
改进get longfilename entry函数，
解决扩展名符号"."在第二个entry中的bug

Revision 1.7  2007/03/11 10:23:44  yangwenbin
disk_enumeration,folder_enumeration函数测试成功
测试方法:
分离大小为127M IMG的所含文件，如rar,pdf等

Revision 1.6  2007/03/06 14:48:58  yangwenbin
cd_folder()函数测试成功

Revision 1.5  2007/03/06 12:27:05  yangwenbin
函数cd_folder()完成， mode 0/mode1两种模式测试通过

Revision 1.4  2007/03/04 03:01:07  yangwenbin
open_file(),f_seek(),read_file(),
文件：C:\longfilename directory for test\tony yang and\test_test.txt
测试成功

Revision 1.3  2007/03/03 15:39:30  yangwenbin
open_file,f_seek,read_file
读1.9M文件测试成功

Revision 1.2  2007/02/28 13:39:41  Design
函数open_file打开绝对路径+短文件名测试通过
使用C代码如下：
open_file("C:\\HELLO.TXT")

Revision 1.1.1.1  2007/02/26 14:01:12  Design
volume_inquire函数测试成功

-FFTR--------------------------------------------------------------------
*/
