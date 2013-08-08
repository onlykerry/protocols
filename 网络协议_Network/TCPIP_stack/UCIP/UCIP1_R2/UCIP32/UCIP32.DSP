# Microsoft Developer Studio Project File - Name="ucip32" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=ucip32 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ucip32.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ucip32.mak" CFG="ucip32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ucip32 - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "ucip32 - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ucip32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /Zp1 /MD /W3 /GX /O2 /I "..\src\if_dev" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "OS_WIN32" /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "ucip32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /Zp1 /MDd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "OS_WIN32" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "ucip32 - Win32 Release"
# Name "ucip32 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\InetAddr.c
# End Source File
# Begin Source File

SOURCE=..\src\net.c
# End Source File
# Begin Source File

SOURCE=..\src\netaddrs.c
# End Source File
# Begin Source File

SOURCE=..\src\netarp.c
# End Source File
# Begin Source File

SOURCE=..\src\netauth.c
# End Source File
# Begin Source File

SOURCE=..\src\netbootp.c
# End Source File
# Begin Source File

SOURCE=..\src\netbuf.c
# End Source File
# Begin Source File

SOURCE=..\src\netchap.c
# End Source File
# Begin Source File

SOURCE=..\src\netchat.c
# End Source File
# Begin Source File

SOURCE=..\src\netchpms.c
# End Source File
# Begin Source File

SOURCE=..\src\netdebug.c
# End Source File
# Begin Source File

SOURCE=..\src\netdhcp.c
# End Source File
# Begin Source File

SOURCE=..\src\neteth.c
# End Source File
# Begin Source File

SOURCE=..\src\netether.c
# End Source File
# Begin Source File

SOURCE=..\src\netfsm.c
# End Source File
# Begin Source File

SOURCE=..\src\neticmp.c
# End Source File
# Begin Source File

SOURCE=..\src\netip.c
# End Source File
# Begin Source File

SOURCE=..\src\netipcp.c
# End Source File
# Begin Source File

SOURCE=..\src\netlcp.c
# End Source File
# Begin Source File

SOURCE=..\src\netmagic.c
# End Source File
# Begin Source File

SOURCE=..\src\netmd5.c
# End Source File
# Begin Source File

SOURCE=..\src\netpap.c
# End Source File
# Begin Source File

SOURCE=..\src\netppp.c
# End Source File
# Begin Source File

SOURCE=..\src\netrand.c
# End Source File
# Begin Source File

SOURCE=..\src\netsock.c
# End Source File
# Begin Source File

SOURCE=..\src\netsocka.c
# End Source File
# Begin Source File

SOURCE=..\src\nettcp.c
# End Source File
# Begin Source File

SOURCE=..\src\nettimer.c
# End Source File
# Begin Source File

SOURCE=..\src\netudp.c
# End Source File
# Begin Source File

SOURCE=..\src\netvj.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\devio.h
# End Source File
# Begin Source File

SOURCE=..\src\InetAddr.h
# End Source File
# Begin Source File

SOURCE=..\src\net.h
# End Source File
# Begin Source File

SOURCE=..\src\netaddrs.h
# End Source File
# Begin Source File

SOURCE=..\src\netarp.h
# End Source File
# Begin Source File

SOURCE=..\src\netauth.h
# End Source File
# Begin Source File

SOURCE=..\src\netbootp.h
# End Source File
# Begin Source File

SOURCE=..\src\netbuf.h
# End Source File
# Begin Source File

SOURCE=..\src\netchap.h
# End Source File
# Begin Source File

SOURCE=..\src\netchat.h
# End Source File
# Begin Source File

SOURCE=..\src\netchpms.h
# End Source File
# Begin Source File

SOURCE=..\src\netconf.h
# End Source File
# Begin Source File

SOURCE=..\src\netdebug.h
# End Source File
# Begin Source File

SOURCE=..\src\netdhcp.h
# End Source File
# Begin Source File

SOURCE=..\src\neteth.h
# End Source File
# Begin Source File

SOURCE=..\src\netether.h
# End Source File
# Begin Source File

SOURCE=..\src\netfsm.h
# End Source File
# Begin Source File

SOURCE=..\src\neticmp.h
# End Source File
# Begin Source File

SOURCE=..\src\netifdev.h
# End Source File
# Begin Source File

SOURCE=..\src\netip.h
# End Source File
# Begin Source File

SOURCE=..\src\netipcp.h
# End Source File
# Begin Source File

SOURCE=..\src\netiphdr.h
# End Source File
# Begin Source File

SOURCE=..\src\netlcp.h
# End Source File
# Begin Source File

SOURCE=..\src\netmagic.h
# End Source File
# Begin Source File

SOURCE=..\src\netmd5.h
# End Source File
# Begin Source File

SOURCE=..\src\netos.h
# End Source File
# Begin Source File

SOURCE=..\src\netpap.h
# End Source File
# Begin Source File

SOURCE=..\src\netppp.h
# End Source File
# Begin Source File

SOURCE=..\src\netrand.h
# End Source File
# Begin Source File

SOURCE=..\src\netsock.h
# End Source File
# Begin Source File

SOURCE=..\src\netsocka.h
# End Source File
# Begin Source File

SOURCE=..\src\netsockd.h
# End Source File
# Begin Source File

SOURCE=..\src\nettcp.h
# End Source File
# Begin Source File

SOURCE=..\src\nettcphd.h
# End Source File
# Begin Source File

SOURCE=..\src\nettimer.h
# End Source File
# Begin Source File

SOURCE=..\src\nettypes.h
# End Source File
# Begin Source File

SOURCE=..\src\netudp.h
# End Source File
# Begin Source File

SOURCE=..\src\netvj.h
# End Source File
# End Group
# Begin Group "IF_DEV Files"

# PROP Default_Filter "*.*"
# Begin Source File

SOURCE=..\src\if_dev\cs89x\if_cs89d.c
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\cs89x\if_cs89d.h
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\cs89x\if_cs89r.h
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\cs89x\if_cs89x.c
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\cs89x\if_cs89x.h
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\if_devio.h
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\ne2k\if_ne2k.h
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\ne2k\if_ne2kd.c
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\ne2k\if_ne2kr.h
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\ne2k\if_os.c
# End Source File
# Begin Source File

SOURCE=..\src\if_dev\ne2k\if_os.h
# End Source File
# End Group
# End Target
# End Project
