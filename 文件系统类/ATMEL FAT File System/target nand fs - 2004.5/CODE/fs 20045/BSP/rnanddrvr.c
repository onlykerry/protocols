/***********************************************************************/
/*                                                                     */
/*   Module:  rnanddrvr.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: Implements a TargetFFS-NAND driver using RAM             */
/*                                                                     */
/***********************************************************************/
#include "../posix.h"
#if INC_NAND_FS
#include "../include/libc/string.h"
#include "../include/libc/stdlib.h"
#include "../include/targetos.h"
#include "../include/fsprivate.h"
#include "../include/sys.h"

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define NUM_BLOCKS      332
#define MAX_BAD_BLOCKS  2
#define BLOCK_SIZE      (8 * 1024)
#define PAGE_SIZE       512
#define ECC_SIZE        10
#define TYPE_SIZE       4
#define SPARE_SIZE      (ECC_SIZE + TYPE_SIZE)
#define NUM_PAGES       (NUM_BLOCKS * (BLOCK_SIZE / PAGE_SIZE))
#define PAGES_PER_BLOCK (BLOCK_SIZE / PAGE_SIZE)

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef ui32 PageData[PAGE_SIZE >> 2];
typedef ui8 PageSpare[SPARE_SIZE];

/***********************************************************************/
/* Global Variable Definitions                                         */
/***********************************************************************/
static PageData *Data;
static PageSpare *Spare;

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/* page_erased: Check if page is erased, both data and spare           */
/*                                                                     */
/*      Inputs: addr = page address                                    */
/*              vol = driver volume                                    */
/*                                                                     */
/*     Returns: TRUE if erased, FALSE if any bit is not 1              */
/*                                                                     */
/***********************************************************************/
static int page_erased(ui32 addr, void *vol)
{
  ui32 i, page = addr / PAGE_SIZE;

  /*-------------------------------------------------------------------*/
  /* Check if data area is erased.                                     */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < (PAGE_SIZE >> 2); ++i)
    if (Data[page][i] != 0xFFFFFFFF)
      return FALSE;

  /*-------------------------------------------------------------------*/
  /* Check if spare bytes are erased.                                  */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < SPARE_SIZE; ++i)
    if (Spare[page][i] != 0xFF)
      return FALSE;

  return TRUE;
}

/***********************************************************************/
/*     wr_page: ECC encode and write one NAND flash page               */
/*                                                                     */
/*      Inputs: buffer = pointer to buffer containing data to write    */
/*              type = page type flag                                  */
/*              addr = the page address                                */
/*              wear = block wear count, ignore                        */
/*              vol = driver's volume pointer                          */
/*                                                                     */
/*     Returns: 0 for success, -1 for chip error, 1 for verify error   */
/*                                                                     */
/***********************************************************************/
static int wr_page(void *buf, ui32 addr, ui32 type, ui32 wear, void *vol)
{
  ui32 page = addr / PAGE_SIZE;
  ui8 ecc[10];

  /*-------------------------------------------------------------------*/
  /* Make sure address is good and page is erased.                     */
  /*-------------------------------------------------------------------*/
  PfAssert(addr < NUM_BLOCKS * BLOCK_SIZE);
  PfAssert(page_erased(addr, vol));

  /*-------------------------------------------------------------------*/
  /* Compute page's ECC encoding.                                      */
  /*-------------------------------------------------------------------*/
  EncodeEcc(buf, ecc);

  /*-------------------------------------------------------------------*/
  /* Copy page data, ECC, and type. Return success.                    */
  /*-------------------------------------------------------------------*/
  memcpy(Data[page], buf, PAGE_SIZE);
  memcpy(&Spare[page][0], ecc, ECC_SIZE);
  memcpy(&Spare[page][ECC_SIZE], &type, TYPE_SIZE);
  return 0;
}

/***********************************************************************/
/*  write_type: Write page's type value                                */
/*                                                                     */
/*      Inputs: addr = the page address                                */
/*              type = page type (0xFFFF0000)                          */
/*              vol = driver's volume pointer                          */
/*                                                                     */
/*        Note: Updates four-byte type flag written by wr_page()       */
/*                                                                     */
/***********************************************************************/
static int write_type(ui32 addr, ui32 type, void *vol)
{
  ui32 page = addr / PAGE_SIZE;

  PfAssert(addr < NUM_BLOCKS * BLOCK_SIZE);
  memcpy(&Spare[page][ECC_SIZE], &type, TYPE_SIZE);
  return 0;
}

/***********************************************************************/
/*   read_type: Read page's wr_page() type value                       */
/*                                                                     */
/*      Inputs: addr = the page address                                */
/*              vol = driver's volume pointer                          */
/*                                                                     */
/*     Returns: The four byte type value written during wr_page()      */
/*              and possibly updated by write_type().                  */
/*                                                                     */
/***********************************************************************/
static ui32 read_type(ui32 addr, void *vol)
{
  ui32 page = addr / PAGE_SIZE;
  ui32 type;

  PfAssert(addr < NUM_BLOCKS * BLOCK_SIZE);
  memcpy(&type, &Spare[page][ECC_SIZE], TYPE_SIZE);
  return type;
}

/***********************************************************************/
/*   read_page: Read and ECC correct one NAND flash page               */
/*                                                                     */
/*      Inputs: buffer = pointer to buffer to copy data to             */
/*              addr = the page address                                */
/*              wear = wear count of this page's block                 */
/*              vol = driver's volume pointer                          */
/*                                                                     */
/*     Returns: 0 for success, -1 for uncorrectable error              */
/*                                                                     */
/***********************************************************************/
static int read_page(void *buffer, ui32 addr, ui32 wear, void *vol)
{
  ui32 page = addr / PAGE_SIZE;
  ui8 ecc[10];

  PfAssert(addr < NUM_BLOCKS * BLOCK_SIZE);
  /*-------------------------------------------------------------------*/
  /* Read page data and ECC bytes.                                     */
  /*-------------------------------------------------------------------*/
  memcpy(buffer, Data[page], PAGE_SIZE);
  memcpy(ecc, Spare[page], ECC_SIZE);

  /*-------------------------------------------------------------------*/
  /* Perform ECC decoding, return error if uncorrectable error.        */
  /*-------------------------------------------------------------------*/
  if (DecodeEcc(buffer, ecc))
    return -1;
  else
    return 0;
}

/***********************************************************************/
/* erase_block: Erase entire flash block                               */
/*                                                                     */
/*      Inputs: addr = base address of block to be erased              */
/*              vol = driver's volume pointer                          */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int erase_block(ui32 addr, void *vol)
{
  ui32 page = addr / PAGE_SIZE;

  /*-------------------------------------------------------------------*/
  /* Reset block data and spare area.                                  */
  /*-------------------------------------------------------------------*/
  memset(Data[page], 0xFF, BLOCK_SIZE);
  memset(Spare[page], 0xFF, PAGES_PER_BLOCK * SPARE_SIZE);
  return 0;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* NandDriverModule: Flash driver's module list entry                  */
/*                                                                     */
/*      Inputs: req = module request code                              */
/*              ... = additional parameters specific to request        */
/*                                                                     */
/***********************************************************************/
void *NandDriverModule(int req, ...)
{
  int b;
  FfsVol vol;

  if (req == kInitMod)
  {
    /*-----------------------------------------------------------------*/
    /* Allocate volume memory.                                         */
    /*-----------------------------------------------------------------*/
    Data = malloc(NUM_PAGES * PAGE_SIZE);
    if (Data == NULL)
    {
      perror("malloc() failed");
      return NULL;
    }
    Spare = malloc(NUM_PAGES * SPARE_SIZE);
    if (Spare == NULL)
    {
      free(Data);
      perror("malloc() failed");
      return NULL;
    }

    /*-----------------------------------------------------------------*/
    /* Start with RAM device "erased".                                 */
    /*-----------------------------------------------------------------*/
    for (b = 0; b < NUM_BLOCKS; ++b)
      erase_block(b * BLOCK_SIZE, NULL);

    /*-----------------------------------------------------------------*/
    /* Register flash NAND volume with TargetFFS.                      */
    /*-----------------------------------------------------------------*/
    vol.name = "nand";
    vol.page_size = PAGE_SIZE;
    vol.block_size = BLOCK_SIZE;
    vol.num_blocks = NUM_BLOCKS;
    vol.max_bad_blocks = MAX_BAD_BLOCKS;
    vol.mem_base = 0;
    vol.vol = NULL;
    vol.flag = FFS_QUOTA_ENABLED;
    vol.driver.nand.write_page = wr_page;
    vol.driver.nand.write_type = write_type;
    vol.driver.nand.read_page = read_page;
    vol.driver.nand.read_type = read_type;
    vol.driver.nand.page_erased = page_erased;
    vol.driver.nand.erase_block = erase_block;
    if (FsNandAddVol(&vol))
      perror("FsNandAddVol() failed");
  }

  return NULL;
}
#endif /* INC_NAND_FS */

