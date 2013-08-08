/***********************************************************************/
/*                                                                     */
/*   Module:  main.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2004.2                                                   */
/*   Purpose: Binary Search Application                                */
/*                                                                     */
/***********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <targetos.h>
#include <kernel.h>
#include <sys.h>
#include <pccard.h>
#include "../../posix.h"

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define HI_KEY_NUM      15000 /* RAND_MAX */
#define CREATE_DATA     TRUE
#define ID_REG          0
#define CWD_WD1         1
#define CWD_WD2         2
#define RFS_NAME        "rfs"

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#define NUM_FSYS        (sizeof(Volume) / sizeof(char *))

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef struct
{
  ui32 key;
  ui32 data;
} Record;

/***********************************************************************/
/* Global Data Declarations                                            */
/***********************************************************************/
static char *Volume[] =
{
#if INC_NAND_FS
  "nand",
#endif
#if INC_NOR_FS
  "nor",
#endif
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
};

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/* bsearch_app: Perform binary search on file system data              */
/*                                                                     */
/*      Inputs: vol_name = name of file system volume                  */
/*              creat_data = TRUE to format volume and write data      */
/*                                                                     */
/***********************************************************************/
static void bsearch_app(char *vol_name, int creat_data)
{
  off_t low, high, pos, sc;
  int rc, key, fid, i;
  Record record;
  clock_t delta, sample;
  char path[PATH_MAX];
  union vstat stats;

  /*-------------------------------------------------------------------*/
  /* If requested, format volume and write data.                       */
  /*-------------------------------------------------------------------*/
  if (creat_data)
  {
    printf("Formating \"%s\" volume\n", vol_name);
    taskSleep(1);
    sample = clock();
    if (format(vol_name))
      SysFatalError(errno);
    delta = clock() - sample;
    printf("Elapsed time for format is %u seconds\n",
           (delta + (CLOCKS_PER_SEC / 2)) / CLOCKS_PER_SEC);
  }

  /*-------------------------------------------------------------------*/
  /* Mount the volume and change to its root directory.                */
  /*-------------------------------------------------------------------*/
  printf("Mounting \"%s\" volume\n", vol_name);
  taskSleep(1);
  sample = clock();
  if (mount(vol_name))
    SysFatalError(errno);
  delta = clock() - sample;
  printf("Elapsed time for mount is %u seconds\n",
         (delta + (CLOCKS_PER_SEC / 2)) / CLOCKS_PER_SEC);
  snprintf(path, PATH_MAX, "/%s", vol_name);
  if (chdir(path))
    SysFatalError(errno);

  /*-------------------------------------------------------------------*/
  /* Display volume geometry and total size.                           */
  /*-------------------------------------------------------------------*/
  if (vstat(vol_name, &stats))
    SysFatalError(errno);
  else
  {
    long size, ones, tenths;

    printf("Volume has %ld sectors of %ld bytes each: ",
           stats.ffs.num_sects, stats.ffs.sect_size);
    size = stats.ffs.num_sects * stats.ffs.sect_size;
    if (size / (1024 * 1024))
    {
      size += (1024 * 1024) / 20;
      ones = size / (1024 * 1024);
      tenths = (10 * (size % (1024 * 1024))) / (1024 * 1024);
      printf("%d.%dMB\n", ones, tenths);
    }
    else if (size / 1024)
    {
      size += 1024 / 20;
      ones = size / 1024;
      tenths = (10 * (size % 1024)) / 1024;
      printf("%d.%dKB\n", ones, tenths);
    }
    else
      printf("%d bytes\n", size);
  }

  /*-------------------------------------------------------------------*/
  /* Open the file for reads and writes, creating it if necessary.     */
  /*-------------------------------------------------------------------*/
  if (creat_data)
    fid = open("records", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
  else
    fid = open("records", O_RDONLY);
  if (fid == -1)
    SysFatalError(errno);

  /*-------------------------------------------------------------------*/
  /* If needed, create multiple records, with increasing key values.   */
  /*-------------------------------------------------------------------*/
  if (creat_data)
  {
    printf("Creating file with monotonically ordered keys\n");
    taskSleep(1);
    sample = clock();
    record.data = 0x12345678;
    for (i = 0; i <= HI_KEY_NUM; ++i)
    {
      record.key = i;
      rc = write(fid, &record, sizeof(Record));
      if (rc != sizeof(Record))
        SysFatalError(errno);
    }
    delta = clock() - sample;
    printf("Elapsed time to create data is %u seconds\n",
           (delta + (CLOCKS_PER_SEC / 2)) / CLOCKS_PER_SEC);
  }

  /*-------------------------------------------------------------------*/
  /* Flush all record data to the flash volume.                        */
  /*-------------------------------------------------------------------*/
  rc = sync(fid);
  if (rc)
    SysFatalError(errno);

  /*-------------------------------------------------------------------*/
  /* Use repeatable "random" sequence for consistent measurements.     */
  /*-------------------------------------------------------------------*/
  srand(0x1234);

  /*-------------------------------------------------------------------*/
  /* Synchronize with tick interrupt and get a starting time stamp.    */
  /*-------------------------------------------------------------------*/
  printf("Beginning binary search test loop\n");
  taskSleep(OsTicksPerSec / 2);
  sample = clock();

  /*-------------------------------------------------------------------*/
  /* Make multiple queries for records matching key value.             */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < 5000; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Choose a random value to search for.                            */
    /*-----------------------------------------------------------------*/
    key = rand() % HI_KEY_NUM;

    /*-----------------------------------------------------------------*/
    /* Implement binary search algorithm.                              */
    /*-----------------------------------------------------------------*/
    low = 0; high = HI_KEY_NUM;
    for (;;)
    {
      /*---------------------------------------------------------------*/
      /* Seek to half-way point.                                       */
      /*---------------------------------------------------------------*/
      pos = (low + high) >> 1;
      sc = lseek(fid, pos * sizeof(Record), SEEK_SET);
      if (sc == (off_t)-1)
        SysFatalError(errno);

      /*---------------------------------------------------------------*/
      /* Read record at requested offset.                              */
      /*---------------------------------------------------------------*/
      rc = read(fid, &record, sizeof(Record));
      if (rc == -1)
        SysFatalError(errno);

      /*---------------------------------------------------------------*/
      /* Update high or low pointer or break, depending on comparison. */
      /*---------------------------------------------------------------*/
      if (key < record.key)
        high = pos - 1;
      else if (key > record.key)
        low = pos + 1;
      else
        break;
      assert(low <= high);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Get ending time stamp and calculate timing result.                */
  /*-------------------------------------------------------------------*/
  delta = clock() - sample;
  printf("Elapsed time for %u searches is %u seconds\n", i,
         (delta + (CLOCKS_PER_SEC / 2)) / CLOCKS_PER_SEC);
  close(fid);
  taskSleep(50);  /* wait til serial interrupts are over */

  /*-------------------------------------------------------------------*/
  /* Unmount the volume.                                               */
  /*-------------------------------------------------------------------*/
  printf("Unmounting \"%s\" volume\n", vol_name);
  taskSleep(1);
  sample = clock();
  if (unmount(vol_name))
    SysFatalError(errno);
  delta = clock() - sample;
  printf("Elapsed time for unmount is %u seconds\n",
         (delta + (CLOCKS_PER_SEC / 2)) / CLOCKS_PER_SEC);
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
  /* Perform binary search on file system data.                        */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    printf("\n****************** Test Loop %u ******************\n", i);
    bsearch_app(Volume[i % NUM_FSYS], (i <= NUM_FSYS) && CREATE_DATA);
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

