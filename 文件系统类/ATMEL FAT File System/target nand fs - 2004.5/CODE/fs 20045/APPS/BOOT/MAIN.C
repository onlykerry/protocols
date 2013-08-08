/***********************************************************************/
/*                                                                     */
/*   Module:  main.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2003.1                                                   */
/*   Purpose: TargetFFS Powerloss Recovery Test                        */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*   This program simulates using TargetFFS to hold both the current   */
/*   version of an application and an upgrade that is being downloaded */
/*   over a network. After the download is complete, the current copy  */
/*   is deleted. Subsequent boots use the latest version.              */
/*                                                                     */
/*   This application tests TargetFFS's powerloss recovery. Two files  */
/*   are used: "exe1" and "exe2". At startup, the program checks that  */
/*   at least one of these files exists and ends with a valid four     */
/*   byte checksum. When files are written, they are padded to a       */
/*   multiple of four bytes before the checksum is appended.           */
/*                                                                     */
/*   If both files exist and have valid checksums, the most recent     */
/*   becomes the boot file. The other file is used for the next down-  */
/*   load. The program enters an infinite loop in which a new upgrade  */
/*   is loaded, the old boot file is deleted, and then the boot and    */
/*   upgrade file names are swapped.                                   */
/*                                                                     */
/*   The test is performed by "randomly" removing power while the      */
/*   application is running. It is a fatal error if the program is not */
/*   able to mount the flash volume and find at least one valid file.  */
/*                                                                     */
/*   Before the test can be run, the volume must be formatted and      */
/*   written with at least one valid file.                             */
/*                                                                     */
/***********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
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
/*    download: Simulate downloading a new executable over a network   */
/*              connection. Just write a file with semi-random values  */
/*              with a length that is a multiple of four bytes, then   */
/*              append a 32-bit checksum.                              */
/*                                                                     */
/*       Input: fname = the name of the file to be created             */
/*                                                                     */
/***********************************************************************/
static void download(char *fname)
{
  int fid, length, rc;
  ui32 i, checksum;

  /*-------------------------------------------------------------------*/
  /* Create the download file.                                         */
  /*-------------------------------------------------------------------*/
  fid = open(fname, O_WRONLY | O_CREAT | O_TRUNC,
             S_IRUSR | S_IWUSR | S_IXUSR);

  /*-------------------------------------------------------------------*/
  /* Choose a somewhat random length >= 128KB.                         */
  /*-------------------------------------------------------------------*/
  length = (128 * 1024 + (rand() % 2048)) & ~3;

  /*-------------------------------------------------------------------*/
  /* Fill the download file with data.                                 */
  /*-------------------------------------------------------------------*/
  length >>= 2;
  checksum = 0;
  for (i = 0; i < length; ++i)
  {
    checksum += i;
    rc = write(fid, &i, sizeof(i));
    if (rc != sizeof(i))
      error("Unable to write file data");
  }

  /*-------------------------------------------------------------------*/
  /* Write the 32-bit checksum.                                        */
  /*-------------------------------------------------------------------*/
  rc = write(fid, &checksum, sizeof(checksum));
  if (rc != sizeof(checksum))
    error("Unable to write file checksum");

  /*-------------------------------------------------------------------*/
  /* Close the file, flushing its contents to flash.                   */
  /*-------------------------------------------------------------------*/
  close(fid);
}

/***********************************************************************/
/*       valid: Check if file identifier is valid, the file length is  */
/*              non-zero and a multiple of four bytes, and the file    */
/*              ends with a 32-bit checksum of the previous contents.  */
/*                                                                     */
/*       Input: fid = identifier of file to be checked                 */
/*                                                                     */
/***********************************************************************/
static int valid(int fid)
{
  int rc, length;
  struct stat sbuf;
  ui32 lword, expected, checksum;

  /*-------------------------------------------------------------------*/
  /* Check if the file identifier is valid.                            */
  /*-------------------------------------------------------------------*/
  if (fid == -1)
    return FALSE;

  /*-------------------------------------------------------------------*/
  /* Read the file state information.                                  */
  /*-------------------------------------------------------------------*/
  if (fstat(fid, &sbuf))
    error("fstat() error in valid()");

  /*-------------------------------------------------------------------*/
  /* Return FALSE if the length is zero or not a multiple of four.     */
  /*-------------------------------------------------------------------*/
  length = sbuf.st_size;
  if ((length == 0) || (length & 3))
    return FALSE;

  /*-------------------------------------------------------------------*/
  /* Read and checksum all but the last four bytes.                    */
  /*-------------------------------------------------------------------*/
  length >>= 2;
  expected = checksum = 0;
  while (--length)
  {
    rc = read(fid, &lword, sizeof(lword));
    if (rc == -1)
      error("read() returned -1 while reading data");

    if (lword != expected)
      error("data error");

    checksum += lword;
    ++expected;
  }

  /*-------------------------------------------------------------------*/
  /* Read and verify the checksum.                                     */
  /*-------------------------------------------------------------------*/
  rc = read(fid, &lword, sizeof(lword));
  if (rc == -1)
    error("read() returned -1 while reading checksum");
  return lword == checksum;
}

/***********************************************************************/
/*     prepare: Verify most recent boot image and then load a new one  */
/*                                                                     */
/*       Input: vol_name = pointer to volume name                      */
/*                                                                     */
/***********************************************************************/
static void prepare(char *vol_name)
{
  /*-------------------------------------------------------------------*/
  /* Format and mount the volume.                                      */
  /*-------------------------------------------------------------------*/
  if (format(vol_name))
    error("format flash failed");
  if (mount(vol_name))
    error("Unable to mount flash volume!");
  if (chdir(vol_name))
    error("Unable to 'cd' to flash directory!");

  /*-------------------------------------------------------------------*/
  /* Change to volume's root directory and install "exe1".             */
  /*-------------------------------------------------------------------*/
  download("exe1");

  /*-------------------------------------------------------------------*/
  /* Unmount the volume.                                               */
  /*-------------------------------------------------------------------*/
  if (unmount(vol_name))
    error("Unable to unmount flash volume!");
}

/***********************************************************************/
/*        boot: Verify most recent boot image and then load a new one  */
/*                                                                     */
/*       Input: vol_name = pointer to volume name                      */
/*                                                                     */
/***********************************************************************/
static void boot(char *vol_name)
{
  int fid1, fid2, valid1, valid2;
  char *boot, *path, *upgrade;

  /*-------------------------------------------------------------------*/
  /* Mount the volume and change to its root directory.                */
  /*-------------------------------------------------------------------*/
  if (mount(vol_name))
    error("Unable to mount flash volume!");
  if (chdir(vol_name))
    error("Unable to 'cd' to flash directory!");

  /*-------------------------------------------------------------------*/
  /* Attempt to open both files. Ensure at least one exists.           */
  /*-------------------------------------------------------------------*/
  fid1 = open("exe1", O_RDONLY);
  fid2 = open("exe2", O_RDONLY);
  if ((fid1 == -1) && (fid2 == -1))
    error("Neither file exists!");

  /*-------------------------------------------------------------------*/
  /* Determine validity of both files. Ensure at least one is valid.   */
  /*-------------------------------------------------------------------*/
  valid1 = valid(fid1);
  valid2 = valid(fid2);
  if ((valid1 == FALSE) && (valid2 == FALSE))
    error("Neither file is valid!");

  /*-------------------------------------------------------------------*/
  /* If both files are valid, the most recent is the boot file.        */
  /*-------------------------------------------------------------------*/
  if (valid1 && valid2)
  {
    struct stat buf1, buf2;

    /*-----------------------------------------------------------------*/
    /* Read the file state info for both files.                        */
    /*-----------------------------------------------------------------*/
    if (fstat(fid1, &buf1))
      error("Unable to fstat() exe1");
    if (fstat(fid2, &buf2))
      error("Unable to fstat() exe2");

    /*-----------------------------------------------------------------*/
    /* Use the most recently modified file as the boot file.           */
    /*-----------------------------------------------------------------*/
    if (buf1.st_mtime > buf2.st_mtime)
    {
      boot = "exe1";
      upgrade = "exe2";
    }
    else
    {
      boot = "exe2";
      upgrade = "exe1";
    }
  }

  /*-------------------------------------------------------------------*/
  /* Else, the valid one is the boot and the other is the download.    */
  /*-------------------------------------------------------------------*/
  else
  {
    if (valid1)
    {
      boot = "exe1";
      upgrade = "exe2";
    }
    else
    {
      boot = "exe2";
      upgrade = "exe1";
    }
  }

  /*-------------------------------------------------------------------*/
  /* Close both files.                                                 */
  /*-------------------------------------------------------------------*/
  close(fid1);
  close(fid2);

  /*-------------------------------------------------------------------*/
  /* Download a new upgrade file.                                      */
  /*-------------------------------------------------------------------*/
  path = getcwd(NULL, 0);
  printf("Using \"%s%s\" as the boot file ", path, boot);
  printf("and \"%s%s\" as the download file\n", path, upgrade);
  free(path);
  download(upgrade);

  /*-------------------------------------------------------------------*/
  /* Delete the old boot file.                                         */
  /*-------------------------------------------------------------------*/
  if (unlink(boot))
    error("Unable to delete the old boot file!");

  /*-------------------------------------------------------------------*/
  /* Unmount the volume.                                               */
  /*-------------------------------------------------------------------*/
  if (unmount(vol_name))
    error("Unable to unmount flash volume!");
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
  int i, prep_req = FALSE;

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
    error("Unable to initialize flash file system!");

  /*-------------------------------------------------------------------*/
  /* Prompt for command to initialize flash rather than start test.    */
  /*-------------------------------------------------------------------*/
  printf("\nStarting powerloss recovery test\n"
         "Press 'I' in 2 seconds to initialize flash:");
  for (i = 0; i < 40; ++i)
  {
    if (TtyIoctl(stdin, TTY_KB_HIT) && (getCmdKey() == 'I'))
    {
      printf(" I");
      prep_req = TRUE;
      break;
    }
    SysWait50ms();
  }
  putchar('\n');

  /*-------------------------------------------------------------------*/
  /* If requested, prepare the initial image.                          */
  /*-------------------------------------------------------------------*/
  if (prep_req)
  {
#if INC_NOR_FS
    prepare("nor");
#endif
#if INC_NAND_FS
    prepare("nand");
#endif
    printf("Finished formatting and installing \"exe1\"\n");
  }

  /*-------------------------------------------------------------------*/
  /* Simulate network downloads in an infinite loop. The object is to  */
  /* interrupt this loop by removing power at random points and ensure */
  /* that recovery is successful. A fatal error occurs if there is not */
  /* at least one valid file found during recovery.                    */
  /*-------------------------------------------------------------------*/
  for (;;)
  {
#if INC_NOR_FS
    boot("nor");
#endif
#if INC_NAND_FS
    boot("nand");
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
  for (;;) OsAuditStacks();
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

