/*
*******************************************************************************
*
*******************************************************************************
*/

/*
*******************************************************************************
*                            CONSTANTS
*******************************************************************************
*/
#define FALSE 0
#define TRUE  1

/*
*******************************************************************************
*                            MACROS
*******************************************************************************
*/

#define  OS_ENTER_CRITICAL() { __asm  CLI }
#define  OS_EXIT_CRITICAL()  { __asm  STI }
#define  OS_TASK_SW()        { __asm  INT   uCOS }

/*
*******************************************************************************
*                            DATA TYPES
*******************************************************************************
*/
#pragma warning (push)
#pragma warning (disable: 4142) // benign redefinition of type

//typedef unsigned char  BOOLEAN;
typedef unsigned char  UBYTE;        // Unsigned  8 bit quantity
typedef   signed char  BYTE;         // Signed    8 bit quantity
typedef unsigned short UWORD;        // Unsigned 16 bit quantity
typedef   signed short WORD;         // Signed   16 bit quantity
typedef unsigned long  ULONG;        // Unsigned 32 bit quantity
typedef   signed long  LONG;         // Signed   32 bit quantity

#pragma warning (pop)
