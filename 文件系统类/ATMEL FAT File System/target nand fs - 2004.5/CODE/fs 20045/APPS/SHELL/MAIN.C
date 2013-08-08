/***********************************************************************/
/*                                                                     */
/*   Module:  main.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2004.2                                                   */
/*   Purpose: main function for sample application                     */
/*                                                                     */
/***********************************************************************/
#include <errno.h>
#include <targetos.h>
#include <kernel.h>
#include <sys.h>
#include <pccard.h>
#include "../../posix.h"
#include "shellp.h"

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define ID_REG          0
#define CWD_WD1         1
#define CWD_WD2         2
#define ZFS_NAME        "zfs"
#define RFS_NAME        "rfs"

/***********************************************************************/
/* Global Data Declarations                                            */
/***********************************************************************/
static char *Volume[] =
{
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
#if NUM_RFS_VOLS
  RFS_NAME,
#endif
#if NUM_ZFS_VOLS
  ZFS_NAME,
#endif
};
extern unsigned char ZImage[];

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*        main: Application Entry Point                                */
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
  /* Set our priority.                                                 */
  /*-------------------------------------------------------------------*/
  taskSetPri(RunningTask, 30);

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
  /* Try to mount each volume.                                         */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < sizeof(Volume) / sizeof(char *); ++i)
  {
    vol_name = Volume[i];

    printf("Mounting \"/%s\"", vol_name);
    if (mount(vol_name) == 0)
      printf(", successful.\n");
    else
    {
      printf(", failed.\n");

      /*---------------------------------------------------------------*/
      /* Since mount failed, try to format this volume.                */
      /*---------------------------------------------------------------*/
      printf("Formatting \"/%s\"", vol_name);
      if (format(vol_name))
        printf(", failed.\n");

      /*---------------------------------------------------------------*/
      /* Else format succeeded, make another attempt to mount volume.  */
      /*---------------------------------------------------------------*/
      else
      {
        printf(", successful.\nRetrying mount, ");
        if (mount(vol_name) == 0)
          printf("successful.\n");
        else
          printf("failed.\n");
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Start the shell with stdin and stdout.                            */
  /*-------------------------------------------------------------------*/
  Shell(stdin, stdout);

#if NUM_RFS_VOLS
  /*-------------------------------------------------------------------*/
  /* Free up TargetRFS resources.                                      */
  /*-------------------------------------------------------------------*/
  if (unmount(RFS_NAME) || RfsDelVol(RFS_NAME))
    SysFatalError(errno);
#endif

#if NUM_ZFS_VOLS
  /*-------------------------------------------------------------------*/
  /* Free up TargetZFS resources.                                      */
  /*-------------------------------------------------------------------*/
  if (unmount(ZFS_NAME) || ZfsDelVol(ZFS_NAME))
    SysFatalError(errno);
#endif
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
/*     FsGetId: Get the user and group ID of process                   */
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
/*     FsSetId: Set the user and group ID of process                   */
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

