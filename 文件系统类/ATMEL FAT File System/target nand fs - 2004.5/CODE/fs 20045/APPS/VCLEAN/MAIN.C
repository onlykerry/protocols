/***********************************************************************/
/*                                                                     */
/*   Module:  main.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: TargetFFS vclean() Test                                  */
/*                                                                     */
/***********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <targetos.h>
#include <kernel.h>
#include <sys.h>
#include "../../posix.h"

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define ID_REG          0
#define CWD_WD1         1
#define CWD_WD2         2
#define BUF_SIZE        100

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*       error: Print error message and call fatal error handler       */
/*                                                                     */
/*       Input: msg = pointer to error message                         */
/*                                                                     */
/***********************************************************************/
static void error(char *msg)
{
  perror(msg);
  taskSleep(50);
  SysFatalError(errno);
}

/***********************************************************************/
/*   make_file: Create file, fill it with 512000 of data, and close it */
/*                                                                     */
/***********************************************************************/
static int make_file(char *path)
{
  int fid, i, rc;

  /*-------------------------------------------------------------------*/
  /* Create a file.                                                    */
  /*-------------------------------------------------------------------*/
  fid = creat(path, 0666);
  if (fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Write data to it. Close and return -1 if error.                   */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < 512; ++i)
  {
    rc = write(fid, path, 1000);
    if (rc != 1000)
    {
      printf("Last file size: %u\n", i * 1000 + rc);
      close(fid);
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Close it and return 0 for success.                                */
  /*-------------------------------------------------------------------*/
  close(fid);
  return 0;
}

/***********************************************************************/
/*      report: Report a measurement in seconds and tenths of seconds  */
/*                                                                     */
/*      Inputs: str = message about the measurement                    */
/*              sample = the measurement                               */
/*                                                                     */
/***********************************************************************/
static void report(char *str, clock_t sample)
{
  int secs, tenths;

  /*-------------------------------------------------------------------*/
  /* Round up in the tenths place.                                     */
  /*-------------------------------------------------------------------*/
  sample += CLOCKS_PER_SEC / 20;

  /*-------------------------------------------------------------------*/
  /* Calculate the seconds and tenths of seconds.                      */
  /*-------------------------------------------------------------------*/
  secs = sample / CLOCKS_PER_SEC;
  tenths = (10 * (sample % CLOCKS_PER_SEC)) / CLOCKS_PER_SEC;

  /*-------------------------------------------------------------------*/
  /* Print the report.                                                 */
  /*-------------------------------------------------------------------*/
  printf("%s: %u.%u\n", str, secs, tenths);
}

/***********************************************************************/
/*         app: Various tests of vclean                                */
/*                                                                     */
/*       Input: vol_name = pointer to volume name                      */
/*                                                                     */
/***********************************************************************/
static void app(char *vol_name)
{
  int i, rc;
  char path[BUF_SIZE];
  union vstat stats;
  clock_t delta, sample;
  ui32 sects_2recycle;

  /*-------------------------------------------------------------------*/
  /* Format and mount the volume.                                      */
  /*-------------------------------------------------------------------*/
  printf("Formatting and mounting \"%s\"\n", vol_name);
  if (format(vol_name))
    error("format flash failed");
  if (mount(vol_name))
    error("Unable to mount flash volume!");
  if (chdir(vol_name))
    error("Unable to 'cd' to flash directory!");

  /*-------------------------------------------------------------------*/
  /* Measure time to create one file.                                  */
  /*-------------------------------------------------------------------*/
  sample = clock();
  sprintf(path, "//%s//file0", vol_name);
  if (make_file(path))
  {
    perror("make_file");
    return;
  }
  delta = clock() - sample;
  report("Time to create first file", delta);

  /*-------------------------------------------------------------------*/
  /* Fill the volume with a multitude of files and data.               */
  /*-------------------------------------------------------------------*/
  for (i = 1;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Create a file, fill it with data, and close it.                 */
    /*-----------------------------------------------------------------*/
    sample = clock();
    sprintf(path, "//%s//file%u", vol_name, i);
    rc = make_file(path);
    delta = clock() - sample;
    sprintf(path, "Time to create file %u", i);
    report(path, delta);
    taskSleep(3);
    if (rc) break;
  }

  /*-------------------------------------------------------------------*/
  /* Unlink first file.                                                */
  /*-------------------------------------------------------------------*/
  sprintf(path, "//%s//file0", vol_name);
  sample = clock();
  unlink(path);
  delta = clock() - sample;
  report("Time to unlink first file", delta);

  /*-------------------------------------------------------------------*/
  /* Measure time to re-create it.                                     */
  /*-------------------------------------------------------------------*/
  sample = clock();
  sprintf(path, "//%s//file0", vol_name);
  if (make_file(path))
  {
    perror("make_file");
    return;
  }
  delta = clock() - sample;
  report("Time to recreate first file", delta);

  /*-------------------------------------------------------------------*/
  /* Delete every file.                                                */
  /*-------------------------------------------------------------------*/
  for (i = 1;; ++i)
  {
    sprintf(path, "//%s//file%u", vol_name, i);
    if (unlink(path))
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Track the number of consumable free sectors until next recycle.   */
  /*-------------------------------------------------------------------*/
  for (sects_2recycle = 0;; sects_2recycle = stats.ffs.sects_2recycle)
  {
    vstat(vol_name, &stats);
    printf("Consumable sectors before next recycle = %u\n",
           stats.ffs.sects_2recycle);
    if (sects_2recycle == stats.ffs.sects_2recycle)
      break;
    sects_2recycle = stats.ffs.sects_2recycle;
    taskSleep(3);
  }

  /*-------------------------------------------------------------------*/
  /* Unmount the volume.                                               */
  /*-------------------------------------------------------------------*/
  printf("Unmounting \"%s\"\n", vol_name);
  if (unmount(vol_name))
    error("Unable to unmount flash volume!");
}

/***********************************************************************/
/*     cleaner: Reclaims dirty sectors in 'background'. If the flash   */
/*              driver is guaranteed not to block (i.e. uses polling), */
/*             these vclean() calls may be moved to the idle task.     */
/*                                                                     */
/***********************************************************************/
static void cleaner(ui32 unused)
{
  for (;;)
  {
#if INC_NOR_FS
    while (vclean("nor") > 0) ;
#endif
#if INC_NAND_FS
    while (vclean("nand") > 0) ;
#endif
    taskSleep(2 * OsTicksPerSec);
  }
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*        main: Application Entry Point                                */
/*                                                                     */
/***********************************************************************/
void main(ui32 unused)
{
  /*-------------------------------------------------------------------*/
  /* Lower interrupt mask and start scheduling.                        */
  /*-------------------------------------------------------------------*/
  OsStart();

  /*-------------------------------------------------------------------*/
  /* Lower our priority and display message.                           */
  /*-------------------------------------------------------------------*/
  taskSetPri(RunningTask, 20);
  printf("For best results, EXTRA_FREE should be >= 512000.\n");

  /*-------------------------------------------------------------------*/
  /* Null CWD variables and assign this task's user and group ID.      */
  /*-------------------------------------------------------------------*/
  FsSetId(0, 0);
  FsSaveCWD(0, 0);

  /*-------------------------------------------------------------------*/
  /* Initialize the file system.                                       */
  /*-------------------------------------------------------------------*/
  if (InitFFS())
    error("Unable to initialize flash file system!");

  /*-------------------------------------------------------------------*/
  /* Create low-level task to clean flash volume in the background.    */
  /*-------------------------------------------------------------------*/
  taskCreate("cleaner", STACK_4KB, 3, cleaner, 0, 0);

  /*-------------------------------------------------------------------*/
  /* Various vclean() tests.                                           */
  /*-------------------------------------------------------------------*/
  for (;;)
  {
    taskSleep(1);
#if INC_NOR_FS
    app("nor");
#endif
#if INC_NAND_FS
    app("nand");
#endif
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
  for (;;)
    OsAuditStacks();
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

