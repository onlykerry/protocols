/***********************************************************************/
/*                                                                     */
/*   Module:  pccard.h                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: PC Card public prototypes, declarations, and constants   */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*               Copyright 2004, Blunk Microsystems                    */
/*                      ALL RIGHTS RESERVED                            */
/*                                                                     */
/*   Licensees have the non-exclusive right to use, modify, or extract */
/*   this computer program for software development at a single site.  */
/*   This program may be resold or disseminated in executable format   */
/*   only. The source code may not be redistributed or resold.         */
/*                                                                     */
/***********************************************************************/
#ifndef _PCCARD_H /* Don't include this file more than once */
#define _PCCARD_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
/*
** PC Card Socket Type
*/
#define PCCS_PCMCIA     1   /* 16-bit PC Card */
#define PCCS_PCCARD     2
/*
** PC Card Socket flags
*/
#define PCCF_INSERTED   (1 << 0)
#define PCCF_ADDED      (1 << 1)
#define PCCF_ENABLED    (1 << 2)
#define PCCF_USED       (1 << 3)

/*
** PC Card Card Type
*/
#define PCCC_UNKNOWN    0
#define PCCC_UART       1
#define PCCC_ATA        2

/*
** Address Space Symbols
*/
#define PCCM_COM_MEM    0
#define PCCM_ATT_MEM    1
#define PCCM_IO         2

/*
** Card Voltage Symbols
*/
#define PCCV_DEFAULT    0
#define PCCV_3V         1
#define PCCV_5V         2

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
/*
** PC Card Function Chaining Structure
*/
typedef struct pcc_func
{
  void (*func)(void *pcc_sock);
  struct pcc_func *next;
} PccFunc;

typedef struct pcc_socket pccSocket;
struct pcc_socket
{
  PccFunc action;               /* queue action structure */
  pccSocket *next;              /* pointer to next linked socket */
  void *data;                   /* used by class drivers */
  void *driver;                 /* used by class drivers */
  ui32 attrBase;                /* base of attribute memory */
  ui32 cmemBase;                /* base of common memory */

  /*
  ** Socket Services callback pointers
  */
  ui32 (*map_addr)(ui8 busSize, ui8 memType, ui32 win_size);
  int (*unmap_addr)(ui32 addr);
  int (*power_up)(int voltage);
  void (*power_dn)(void);
  int (*ready)(void);
  void (*enable_irq)(void);
  void (*disable_irq)(void);

  /*
  ** Class Driver callback pointer
  */
  void (*card_isr)(struct pcc_socket *sock);    /* interrupt handler */

  /*
  ** Card Data
  */
  struct card
  {
    ui32 base;                  /* card window base addr */
    ui32 wsize;                 /* card window size */
    ui8 type;
    ui8 subtype;
    vui8 accessOk;              /* true if card can be accessed */
    ui8 corValue;
  } card;
  ui8  type;                    /* 16-bit PCMCIA or 32-bit PCCARD */
  ui8  flags;
};

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
int  pccInit(void);
void pccSoftReset(pccSocket *sock);
void pccPoll(void);

/*
** Application Interface
*/
int  pccAddUART(pccSocket *sock, const char *name);
int  pccAddATA(pccSocket *sock, const char *name);

/*
** Driver Interface
*/
void pccPollReq(pccSocket *sock);
void pccAddCard(pccSocket *sock);
void pccDelCard(pccSocket *sock);

/***********************************************************************/
/* Error Codes (written to errno)                                      */
/***********************************************************************/
typedef enum pccard_errs
{
  PCCARD_ERR_ISR_CREATE_FAILED = 800,
  PCCARD_ERR_TASK_CREATE_FAILED,
  PCCARD_ERR_TOO_MANY_WINDOWS,
  PCCARD_ERR_BAD_WINDOW_SIZE,
  PCCARD_ERR_BAD_UNMAP_BASE,
  PCCARD_ERR_CIS_NOT_SUPPORTED,
  PCCARD_ERR_CIS_INVALID_TUPLE_LINK,
  PCCARD_ERR_CIS_NO_MAP_INFO,
  PCCARD_ERR_CIS_NO_MANFID_TUPLE,
  PCCARD_ERR_CIS_NO_CONFIG_TUPLE,
  PCCARD_ERR_CIS_NO_CFTABL_EENTRY_TUPLE
} PCCardErrors;

#ifdef __cplusplus
}
#endif

#endif /* _PCCARD_H */
