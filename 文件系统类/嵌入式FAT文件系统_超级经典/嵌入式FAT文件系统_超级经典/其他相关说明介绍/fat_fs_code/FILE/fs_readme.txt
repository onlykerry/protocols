File system general information
-------------------------------

This directory contains 3 file system modules: 

- file system ISO9660
    iso9660.c
    iso9660.h

- file system FAT12/16
    fat.c
    fat.h

- file system FAT32
    fat32.c
    fat32.h

file.c and file.h contains all high levels functions/macro/definition for your application.

fs_variable.c contains all definitions of variables that can be shared with the all file systems.


config.h must contain the definition of the file system used by your application.
For example:

#define MEM_CHIP_FS     FS_FAT_12_16         /* _FAT_12_16  _FAT_32  _ISO _NONE     */
#define MEM_CARD_FS     FS_FAT_ISO           /* _FAT_12_16  _FAT_32  _ISO _NONE     */

This lines will set FAT12/16 file system for your on-board memory (dataflash, nandflash, ...)
and ISO9660 for your card (in this case, a Compact-Disc reader).

Location of files must be defined in your project.



