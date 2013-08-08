/*H**************************************************************************
* NAME:         fs.h         
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.1     
*----------------------------------------------------------------------------
* PURPOSE:
* This file contains the system configuration definition
*****************************************************************************/

#ifndef _FS_H_
#define _FS_H_

/*File system definition */
#define     FS_ISO                1
#define     FS_FAT_12_16          2
#define     FS_FAT_32             3
#define     FS_NONE               255

#define     FS_MEM_FAT                ((bit)0)
#define     FS_MEM_ISO                ((bit)1)

#endif    /* _FS_H_ */
