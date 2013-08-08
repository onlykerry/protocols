/***********************************************************************/
/*                                                                     */
/*   Module:  sys.h                                                    */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: System prototypes and symbol definitions                 */
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
#ifndef _SYS_H   /* Don't include this file more than once */
#define _SYS_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef enum
{
  kInitMod,
  kAppName,
  kStart,
  kReadParms,
  kEditParms,
  kShowParms,
  kTtyInit,
  kErrorLookup,
  kInitNi,
  kNiAddr,
  kParseMonCmd,
  kFormat,
  kUnformat,
  kMount,
  kVolName,
  kCardInserted,
  kCardRemoved,
  kCardSearch,
  kpccUartDriver,
  kPoll,
  kDev
} SysModCmds;

void *pccModule(int req, ...);
void *AppModule(int req, ...);
void *pccDvrModule(int req, ...);

typedef void *(*Module)(int code, ...);

/***********************************************************************/
/* Variable Declarations                                               */
/***********************************************************************/
#define ModuleList  ModuleListFFS
extern const Module ModuleList[];

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
void SysWait50ms(void);

/*
** File Systems and their driver modules
*/
void *RfsModule(int req, ...);
void *FfsModule(int req, ...);
void *NandDriverModule(int req, ...);
void *NorDriverModule(int req, ...);
void *FatModule(int req, ...);
void *FatDriverModule(int req, ...);
void *FtlNandDriverModule(int req, ...);

extern Module StartModule;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_H */

