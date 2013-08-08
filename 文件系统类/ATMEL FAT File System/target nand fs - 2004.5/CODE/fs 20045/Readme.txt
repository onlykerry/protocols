README for TargetFFS-NAND Release 2004.5
----------------------------------------

The fs directory contains all the necessary code for TargetFFS-
NAND. Below is a short description of what each subdirectory
contains:

apps directory
--------------
  This directory contains several sample applications.

  boot application
  ----------------
  This application demonstrates recovery from unexpected power
  losses. It writes two files, "exe1" and "exe2", alternatively
  to flash. One is considered the boot program and the other an
  upgrade file. Power should be randomly removed while this
  application is running. When power is restored, the application
  fails if it cannot mount the flash volume and find at least one
  valid file.

  bsearch application
  -------------------
  This application times and performs 1000 binary searches on
  a file of sorted records. The elapsed time is printed to stdout.

  file_test application
  ---------------------
  This application repeatedly creates, prints statistics on, and
  then deletes two sample files.

  shell application
  -----------------
  This application implements a command line shell using stdin
  and stdout. Users can adapt this sample to their application,
  modifying and extending the shell with their own commands.

  stdio application
  -----------------
  This application does some tests using TargetFFS's "stdio.h" API.

  vclean application
  -----------------
  This application demonstrates background garbage collection.

bsp directory
-------------
  This directory contains several sample drivers, including some
  that use RAM, which is useful for testing your port before
  having to test your flash driver. The samples show how to
  implement a driver and integrate it with TargetFFS/TargetFAT.

ffs directory
-------------
  This directory contains all flash specific code with the exception
  of the driver functions. None of these files should be modified.

rfs directory
-------------
  This directory contains the implementation of an optional RAM
  file system which uses the same API as TargetFFS. RAM volumes are
  added by calling RfsAddVol() with the volume name. Dynamic calls
  to malloc() and free() are used to allocate and free memory.
  None of these files should be modified.

include directory
-----------------
  This directory contains include files shared by the other directories.
  These files should never be included by the applications. The header
  files that the applications should include are <posix.h> and
  <ffs_stdio.h>, both of which reside in the top most directory.

posixfs directory
-----------------
  This directory contains the implementation of the POSIX functions
  found in the File System API manual. None of these files should be
  modified.

stdio directory
---------------
  This directory contains implementations of all the standard ANSI C
  functions that are file related, minus the ones that refer to stdin
  or stdout (ex: printf(), scanf(), etc.). Two of these files have
  two versions: "stdiop.c" and "stdiop_f.c", and "xscanf.c" and
  "xscanf_f.c". Use the first version if integer-only fprintf() and
  fscanf() support will suffice. Otherwise, use the second version.

sys directory
-------------
  This directory contains "utf.c", which supports UTF file name
  encoding and "sysioctl.c", which contains the start-up
  initialization function InitFFS(). Neither of these files should
  be modified.


Porting

Our recommendation is to build TargetFFS into a library. All the
files in the ffs and posixfs directories should be included. If
you will use the stdio API (which many applications do not), you
should add the files in the stdio directory.
 
"sysioctl.c" from the sys directory should be in the library. Your
application should call the initialization function InitFFS() that
is defined in this file.

The example drivers are in the bsp directory. You can put your real
driver here as well or link it with the application.

To avoid conflict with alternate definitions of FILE and other symbols,
the provided included files should be used instead of your environments
include files. That is why the full path, instead of the "<>" shorthand
is used for the standard headers. The one exception is "stdarg.h". You
must use the "stdarg.h" supplied by your compiler.

To satisfy the calls to TargetOS's semCreate(), semDelete(), semPend(),
and semPost(), you must provide wrappers that implement these routines
using your RTOS. If you are not using an RTOS, these wrappers can be
empty functions. The file "kernel.h", in the "include" directory, has
prototypes for these functions.

OsSecCount is declared in "kernel.h" and is used by TargetFFS to set
file access and modification times. If the target system maintains
the time-of-day, OsSecCount should be initialized using mktime() and
thereafter incremented once a second. If this is not done, the access
and modification times returned by stat() should be ignored.

size_t, time_t, FILENAME_MAX and FOPEN_MAX are defined in posix.h and
in some of the include files in ffs/include. As part of any porting
effort, these definitions must be checked and made to match the
definitions for the intended environment.

RTOS's frequently support task-specific errno values, while polling
environments typically use a global variable. To minimize porting
efforts, TargetFFS does not directly access errno. All accesses to
errno are done using the set_errno() and get_errno() macros defined
in "fsprivate.h". These macros must be modified to support the errno
mechanism used in your environment.

The sample applications can be ported to your environment using your
environment's standard include files. The only TargetFFS include
files your application should use are "posix.h" and, if needed,
"ffs_stdio.h".

