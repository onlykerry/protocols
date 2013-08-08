/***********************************************************************/
/*                                                                     */
/*   Module:  main.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2004.3                                                   */
/*   Purpose: Implement the file system stdio test application         */
/*                                                                     */
/***********************************************************************/
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <targetos.h>
#include <kernel.h>
#include <sys.h>
#include <pccard.h>
#include "..\..\ffs_stdio.h"
#include "..\..\posix.h"

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define ID_REG          0
#define CWD_WD1         1
#define CWD_WD2         2
#define BUF_LEN         1024
#define RFS_NAME        "rfs"

/***********************************************************************/
/* Global Data Declarations                                            */
/***********************************************************************/
static char *Volume[] =
{
#if NUM_RFS_VOLS
  RFS_NAME,
#endif
#if PCCARD_SUPPORT
  "ata",
#endif
#if INC_FAT_FIXED
  "fat",
#endif
#if NUM_NAND_FTLS
  "ftld",
#endif
#if INC_NAND_FS
  "nand",
#endif
#if INC_NOR_FS
  "nor",
#endif
};
static char Buf[BUF_LEN];

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*  lseek_test: lseek() test                                           */
/*                                                                     */
/*      Return: 0 if successful, else -1                               */
/*                                                                     */
/***********************************************************************/
static int lseek_test(const char *vol_name)
{
  int fid, words, offset, prevOffset, res;
  char *buf;
  FILE_FFS *stream;

  /*-------------------------------------------------------------------*/
  /* Initialize the buffer we will be using.                           */
  /*-------------------------------------------------------------------*/
  buf = (char *)calloc(64 * 1024, 1);

  /*-------------------------------------------------------------------*/
  /* Create file and write 64 KBytes using one write() call.           */
  /*-------------------------------------------------------------------*/
  fid = open("test.dat", O_RDWR | O_CREAT | O_EXCL, 0777);
  words = write(fid, buf, 64 * 1024);
  if (words != 64 * 1024)
  {
    perror("write()");
    free(buf);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check whether lseek() to current position returns file size.      */
  /*-------------------------------------------------------------------*/
  offset = lseek(fid, (off_t)0, SEEK_CUR);
  if (words != offset)
  {
    printf("Wrote %d bytes, but lseek() returned %d for current\n"
           "position\n", words, offset);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Close the first file.                                             */
  /*-------------------------------------------------------------------*/
  if (close(fid))
  {
    perror("close");
    res = -1;
    goto exit;
  }

  /*-------------------------------------------------------------------*/
  /* Create file and write 64 KBytes using putc() in a loop.           */
  /*-------------------------------------------------------------------*/
  fid = open("test2.dat", O_RDWR | O_CREAT | O_EXCL, 0777);
  stream = fdopenFFS(fid, "r+");
  for (words = 0; words < 64 * 1024; words++)
    if (putcFFS(words & 0x7F, stream) < 0)
      return -1;

  /*-------------------------------------------------------------------*/
  /* Check whether lseek() to current position returns file size.      */
  /*-------------------------------------------------------------------*/
  offset = lseek(fid, (off_t)0, SEEK_CUR);
  if (words != offset)
  {
    printf("Wrote %d bytes, but lseek() returned %d for the current\n"
           "position\n", words, offset);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Close the second file.                                            */
  /*-------------------------------------------------------------------*/
  if (close(fid))
  {
    perror("close");
    res = -1;
    goto exit;
  }

  /*-------------------------------------------------------------------*/
  /* Re-open the second file for reading and writing.                  */
  /*-------------------------------------------------------------------*/
  fid = open("test2.dat", O_RDWR, 0777);
  if (fid < 0)
  {
    perror("open");
    res = -1;
    goto exit;
  }

  /*-------------------------------------------------------------------*/
  /* Read BUF_LEN bytes.                                               */
  /*-------------------------------------------------------------------*/
  words = read(fid, buf, BUF_LEN);

  /*-------------------------------------------------------------------*/
  /* Move the file position backwards BUF_LEN bytes, write BUF_LEN     */
  /* bytes, and then read BUF_LEN bytes. Loop until all read.          */
  /*-------------------------------------------------------------------*/
  for (prevOffset = 0; words == BUF_LEN;)
  {
    /*-----------------------------------------------------------------*/
    /* Seek backwards BUF_LEN bytes and then write BUF_LEN bytes.      */
    /*-----------------------------------------------------------------*/
    lseek(fid, (off_t) -BUF_LEN, SEEK_CUR);
    words = write(fid, buf, BUF_LEN);
    if (words != BUF_LEN)
    {
      printf("write() returned %d, expected %d\n", words, BUF_LEN);
      res = -1;
      goto exit;
    }

    /*-----------------------------------------------------------------*/
    /* Get current position via lseek().                               */
    /*-----------------------------------------------------------------*/
    offset = lseek(fid, (off_t)0, SEEK_CUR);

    /*-----------------------------------------------------------------*/
    /* Ensure lseek() delta before and after write() equals BUF_LEN.   */
    /*-----------------------------------------------------------------*/
    if (BUF_LEN != offset - prevOffset)
    {
      printf("Wrote %d bytes, but lseek() delta equals %d\n", BUF_LEN,
             offset - prevOffset);
      res = -1;
      goto exit;
    }

    /*-----------------------------------------------------------------*/
    /* Read BUF_LEN bytes.                                             */
    /*-----------------------------------------------------------------*/
    words = read(fid, buf, BUF_LEN);

    /*-----------------------------------------------------------------*/
    /* Save position before read() and get current position.           */
    /*-----------------------------------------------------------------*/
    prevOffset = offset;
    offset = lseek(fid, (off_t)0, SEEK_CUR);

    /*-----------------------------------------------------------------*/
    /* Ensure lseek() delta before and after read() equals amount read.*/
    /*-----------------------------------------------------------------*/
    if (words != offset - prevOffset)
    {
      printf("Read %d bytes, but lseek() delta equals %d\n", words,
             offset - prevOffset);
      res = -1;
      goto exit;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Close second file and delete both files.                          */
  /*-------------------------------------------------------------------*/
  close(fid);
  res = unlink("test.dat");
  res |= unlink("test2.dat");
  if (res)
    perror("unlink");

  /*-------------------------------------------------------------------*/
  /* Free the buffer and return success.                               */
  /*-------------------------------------------------------------------*/
exit:
  free(buf);
  return res;
}

/***********************************************************************/
/*  stdio_test: Test various stdio calls in file system                */
/*                                                                     */
/*       Input: vol_name = name of file system volume                  */
/*                                                                     */
/***********************************************************************/
static int stdio_test(char *vol_name)
{
  FILE_FFS *stream;
  int i;
  struct stat abuf;
  fpos_tFFS pos;

  /*-------------------------------------------------------------------*/
  /* Fill buffer with data.                                            */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < BUF_LEN; ++i)
    Buf[i] = 'a' + i % 50;

  /*-------------------------------------------------------------------*/
  /* Open write.txt for writing/reading.                               */
  /*-------------------------------------------------------------------*/
  stream = fopenFFS("write.txt", "w+");
  if (stream == NULL)
  {
    perror("error opening the file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Write 678 bytes from buffer to "write.txt".                       */
  /*-------------------------------------------------------------------*/
  if (fwriteFFS(Buf, 1, 678, stream) != 678)
  {
    perror("error writing to file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Seek back 352 bytes from the end of the file.                     */
  /*-------------------------------------------------------------------*/
  if (fseekFFS(stream, -352, SEEK_CUR))
  {
    perror("error seeking in file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Write another 797 bytes, 352 of which will be over old data.      */
  /*-------------------------------------------------------------------*/
  if (fwriteFFS(Buf, 1, 797, stream) != 797)
  {
    perror("error writing to file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Do a stat on the file to check its size is the correct one.       */
  /*-------------------------------------------------------------------*/
  if (stat("write.txt", &abuf))
  {
    perror("error getting stats for file!");
    return -1;
  }
  if (abuf.st_size != 1123)
  {
    printf("TEST FAILED!\n");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Seek past file's end and ensure that its length hasn't changed.   */
  /*-------------------------------------------------------------------*/
  if (fseekFFS(stream, 100, SEEK_END))
  {
    perror("error seeking in file!");
    return -1;
  }
  if (fstat(filenoFFS(stream), &abuf))
  {
    perror("error getting stats for file!");
    return -1;
  }
  if (abuf.st_size != 1123)
  {
    printf("TEST FAILED!\n");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Write one additional byte and verify that file length has grown.  */
  /*-------------------------------------------------------------------*/
  if (fwriteFFS(Buf, 1, 1, stream) != 1)
  {
    perror("error writing to file!");
    return -1;
  }
  if (fstat(filenoFFS(stream), &abuf))
  {
    perror("error getting stats for file!");
    return -1;
  }
  if (abuf.st_size != 1123 + 101)
  {
    printf("TEST FAILED!\n");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Verify that the gap has been filled with zeros.                   */
  /*-------------------------------------------------------------------*/
  if (fseekFFS(stream, -101, SEEK_CUR))
  {
    perror("error seeking in file!");
    return -1;
  }
  for (i = 0; i < 100; ++i)
  {
    char c;

    if (freadFFS(&c, 1, 1, stream) != 1)
    {
      perror("error reading from file!");
      return -1;
    }
    if (c)
    {
      printf("TEST FAILED!\n");
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Do a set and get pos to check them.                               */
  /*-------------------------------------------------------------------*/
  rewindFFS(stream);
  if (fgetposFFS(stream, &pos))
  {
    perror("error getting position for file!");
    return -1;
  }
  if (fseekFFS(stream, 1120, SEEK_CUR))
  {
    perror("error seeking in file!");
    return -1;
  }
  if (freadFFS(Buf, 1, 3, stream) != 3)
  {
    perror("error reading from file!");
    return -1;
  }
  if (fsetposFFS(stream, &pos))
  {
    perror("error setting position for file!");
    return -1;
  }
  if (freadFFS(Buf, 1, 800, stream) != 800)
  {
    perror("error reading from file!");
    return -1;
  }
  if (freadFFS(Buf, 1, 323, stream) != 323)
  {
    perror("error reading from file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Close the file.                                                   */
  /*-------------------------------------------------------------------*/
  if (fcloseFFS(stream))
  {
    perror("error closing file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Rename file.                                                      */
  /*-------------------------------------------------------------------*/
  if (renameFFS("write.txt", "sooner.txt"))
  {
    perror("error renaming file!");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Remove the file.                                                  */
  /*-------------------------------------------------------------------*/
  if (removeFFS("sooner.txt"))
  {
    perror("error removing file!");
    return -1;
  }

  return 0;
}

/***********************************************************************/
/*     hexDump: Display string in ASCII and in hexadecimal             */
/*                                                                     */
/***********************************************************************/
static void hexDump(char *data, int len)
{
  int i;
  ui8 ch;

  /*-------------------------------------------------------------------*/
  /* Print ASCII value.                                                */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < len; ++i)
  {
    ch = data[i];
    if (!isprint(ch))
      ch = '.';
    putchar(ch);
  }
  printf("     ");

  /*-------------------------------------------------------------------*/
  /* Print hex value.                                                  */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < len; ++i)
    printf(" %02X", data[i]);
  putchar('\n');
}

/***********************************************************************/
/*   overwrite: Test file overwriting and truncation                   */
/*                                                                     */
/***********************************************************************/
static int overwrite(char *vol_name)
{
  int i;
  void *fp;
  char string1[4] = {'a', 'b', 'c', 0};
  char string2[8] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 0};
  char string3[10];

  /*-------------------------------------------------------------------*/
  /* Initialize file if it already exists.                             */
  /*-------------------------------------------------------------------*/
  fp = fopenFFS("xyz", "rb");
  if (fp)
  {
    memset(string3, 0, sizeof(string3));
    i = freadFFS(string3, 1, 10, fp);
    printf("\n\rFS_Read count = %d\n", i);
    hexDump(string3, i);
    fcloseFFS(fp);
  }

  /*-------------------------------------------------------------------*/
  /* Write 8 bytes to file.                                            */
  /*-------------------------------------------------------------------*/
  fp = fopenFFS("xyz", "wb");
  if (fp == NULL)
  {
    perror("error creating file!");
    return -1;
  }
  i = fwriteFFS(string2, 1, 8, fp);
  printf("\n\rFS_Write count = %d\n", i);
  if (i != 8)
  {
    perror("error writing file!");
    return -1;
  }
  fcloseFFS(fp);
  hexDump(string2, i);

  /*-------------------------------------------------------------------*/
  /* Read everything from file.                                        */
  /*-------------------------------------------------------------------*/
  fp = fopenFFS("xyz", "rb");
  memset(string3, 0, sizeof(string3));
  i = freadFFS(string3, 1, 10, fp);
  printf("\n\rFS_Read2 count = %d\n", i);
  if (i != 8)
  {
    perror("error reading file!");
    return -1;
  }
  hexDump(string3, i);
  fcloseFFS(fp);

  /*-------------------------------------------------------------------*/
  /* Write 4 bytes to file.                                            */
  /*-------------------------------------------------------------------*/
  fp = fopenFFS("xyz", "wb");
  i = fwriteFFS(string1, 1, 4 , fp);
  printf("\n\rFS_Write2 count = %d\n", i);
  if (i != 4)
  {
    perror("error writing file!");
    return -1;
  }
  fcloseFFS(fp);
  hexDump(string1, i);

  /*-------------------------------------------------------------------*/
  /* Read everything from file.                                        */
  /*-------------------------------------------------------------------*/
  fp = fopenFFS("xyz", "rb");
  memset(string3, 0, sizeof(string3));
  i = freadFFS(string3, 1, 10, fp);
  printf("\n\rFS_Read3 count = %d\n", i);
  if (i != 4)
  {
    perror("error reading file!");
    return -1;
  }
  hexDump(string3, i);
  fcloseFFS(fp);
  return 0;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*        main: Application entry point                                */
/*                                                                     */
/***********************************************************************/
void main(ui32 unused)
{
  int i;
  char *vol_name;

  /*-------------------------------------------------------------------*/
  /* Lower interrupt mask and start scheduling.                        */
  /*-------------------------------------------------------------------*/
  OsStart();

  /*-------------------------------------------------------------------*/
  /* Lower our priority.                                               */
  /*-------------------------------------------------------------------*/
  taskSetPri(RunningTask, 20);

  /*-------------------------------------------------------------------*/
  /* Null CWD variables and assign this task's user and group ID.      */
  /*-------------------------------------------------------------------*/
  FsSetId(0, 0);
  FsSaveCWD(0, 0);

  /*-------------------------------------------------------------------*/
  /* Initialize the file system.                                       */
  /*-------------------------------------------------------------------*/
  if (InitFFS())
    SysFatalError(errno);

#if NUM_RFS_VOLS
  /*-------------------------------------------------------------------*/
  /* Add the TargetRFS volume.                                         */
  /*-------------------------------------------------------------------*/
  if (RfsAddVol(RFS_NAME))
    SysFatalError(errno);
#endif

  /*-------------------------------------------------------------------*/
  /* Run the test programs in an endless loop.                         */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    printf("\n****************** Test Loop %u ******************\n", i);
    vol_name = Volume[i % (sizeof(Volume) / sizeof(char *))];
    printf("Beginning \"%s\" test\n", vol_name);

    /*-----------------------------------------------------------------*/
    /* Format the volume.                                              */
    /*-----------------------------------------------------------------*/
    if (format(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Mount the volume and change to its root directory.              */
    /*-----------------------------------------------------------------*/
    if (mount(vol_name))
      SysFatalError(errno);
    if (chdir(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test lseek().                                                   */
    /*-----------------------------------------------------------------*/
    if (lseek_test(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test file overwriting and truncation.                           */
    /*-----------------------------------------------------------------*/
    if (overwrite(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test various file system stdio calls.                           */
    /*-----------------------------------------------------------------*/
    if (stdio_test(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Unmount volume before starting next test.                       */
    /*-----------------------------------------------------------------*/
    if (unmount(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Announce that the test was successful.                          */
    /*-----------------------------------------------------------------*/
    printf("\"%s\" test completed successfully\n", vol_name);
  }
}

/***********************************************************************/
/*  OsIdleTask: kernel idle task                                       */
/*                                                                     */
/***********************************************************************/
void OsIdleTask(ui32 unused)
{
  /*-------------------------------------------------------------------*/
  /* Loop forever. Feel free to add code here, but nothing that could  */
  /* block (Don't try a printf()).                                     */
  /*-------------------------------------------------------------------*/
  for (;;) OsAuditStacks();
}

/***********************************************************************/
/*   AppModule: Application interface to software module manager       */
/*                                                                     */
/*       Input: req = module request code                              */
/*              ... = additional parameters specific to request        */
/*                                                                     */
/***********************************************************************/
void *AppModule(int req, ...)
{
  switch (req)
  {
#if PCCARD_SUPPORT
    va_list ap;
    pccSocket *sock;

    case kCardInserted:
      printf("Known card inserted\n");

      /*---------------------------------------------------------------*/
      /* Use va_arg mechanism to fetch pointer to command string.      */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      sock = va_arg(ap, pccSocket *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Parse card type.                                              */
      /*---------------------------------------------------------------*/
      switch (sock->card.type)
      {
        case PCCC_ATA:
          printf("ATA Drive\n");
          if (pccAddATA(sock, "ata"))
          {
            perror("pccAddATA");
            break;
          }
          break;

        default:
          printf("unk type = %d\n", sock->card.type);
          break;
      }
      break;

    case kCardRemoved:
      printf("Card removed\n");
      break;
#endif
  }

  return NULL;
}

/***********************************************************************/
/*     FsGetId: Get process user and group ID                          */
/*                                                                     */
/*      Inputs: uid = place to store user ID                           */
/*              gid = place to store group ID                          */
/*                                                                     */
/***********************************************************************/
void FsGetId(uid_t *uid, gid_t *gid)
{
  ui32 id = taskGetReg(RunningTask, ID_REG);

  *uid = (uid_t)id;
  *gid = id >> 16;
}

/***********************************************************************/
/*     FsSetId: Set process user and group ID                          */
/*                                                                     */
/*      Inputs: uid = user ID                                          */
/*              gid = group ID                                         */
/*                                                                     */
/***********************************************************************/
void FsSetId(uid_t uid, gid_t gid)
{
  ui32 id = (gid << 16) | uid;

  taskSetReg(RunningTask, ID_REG, id);
}

/***********************************************************************/
/*   FsSaveCWD: Save per-task current working directory state          */
/*                                                                     */
/*      Inputs: word1 = 1 of 2 words to save                           */
/*              word2 = 2 of 2 words to save                           */
/*                                                                     */
/***********************************************************************/
void FsSaveCWD(ui32 word1, ui32 word2)
{
  taskSetReg(RunningTask, CWD_WD1, word1);
  taskSetReg(RunningTask, CWD_WD2, word2);
}

/***********************************************************************/
/*   FsReadCWD: Read per-task current working directory state          */
/*                                                                     */
/*     Outputs: word1 = 1 of 2 words to retrieve                       */
/*              word2 = 2 of 2 words to retrieve                       */
/*                                                                     */
/***********************************************************************/
void FsReadCWD(ui32 *word1, ui32 *word2)
{
  *word1 = taskGetReg(RunningTask, CWD_WD1);
  *word2 = taskGetReg(RunningTask, CWD_WD2);
}

