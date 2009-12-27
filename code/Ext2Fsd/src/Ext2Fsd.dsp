# Microsoft Developer Studio Project File - Name="Ext2Fsd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Ext2Fsd - Win32 Checked
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Ext2Fsd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Ext2Fsd.mak" CFG="Ext2Fsd - Win32 Checked"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Ext2Fsd - Win32 Free" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Ext2Fsd - Win32 Checked" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Free"
# PROP BASE Intermediate_Dir "Free"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Free"
# PROP Intermediate_Dir "Free"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /WX /Oy /Gy /D "WIN32" /D "_WINDOWS" /Oxs /c
# ADD CPP /nologo /Gz /W3 /WX /Oy /Gy /I "$(BASEDIR)\inc" /I "$(BASEDIR)\inc\ddk\wxp" /I "$(CPU)\\" /I "." /D WIN32=100 /D "_WINDOWS" /D "STD_CALL" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D _NT1X_=100 /D WINNT=1 /D _WIN32_WINNT=0x0500 /D WIN32_LEAN_AND_MEAN=1 /D DEVL=1 /D FPO=1 /D "_IDWBUILD" /D "NDEBUG" /D _DLL=1 /D _X86_=1 /D $(CPU)=1 /Oxs /Zel -cbstring /QIfdiv- /QIf /GF /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "$(BASEDIR)\inc\ddk\wxp" /i "$(BASEDIR)\inc" /i "." /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /machine:IX86
# ADD LINK32 ntoskrnl.lib hal.lib wmilib.lib /nologo /base:"0x10000" /version:4.0 /entry:"DriverEntry" /pdb:none /debug /debugtype:coff /machine:IX86 /nodefaultlib /out:".\free\Ext2Fsd.sys" /libpath:"$(BASEDIR)\libfre\i386" /driver /debug:notmapped,MINIMAL /IGNORE:4001,4037,4039,4065,4070,4078,4087,4089,4096 /MERGE:_PAGE=PAGE /MERGE:_TEXT=.text /SECTION:INIT,d /MERGE:.rdata=.text /FULLBUILD /RELEASE /FORCE:MULTIPLE /OPT:REF /OPTIDATA /align:0x20 /osversion:4.00 /subsystem:native

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "objchk\i386"
# PROP BASE Intermediate_Dir "objchk\i386"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Checked"
# PROP Intermediate_Dir "Checked"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Gz /W3 /Z7 /Oi /Gy /I "$(BASEDIR)\inc" /I "$(BASEDIR)\inc\ddk" /I "$(BASEDIR)\inc\ddk\wxp" /I "." /D _WIN32_WINNT=0x0500 /D WIN32=100 /D "_DEBUG" /D "_WINDOWS" /D "STD_CALL" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D _NT1X_=100 /D WINNT=1 /D WIN32_LEAN_AND_MEAN=1 /D DBG=1 /D DEVL=1 /D FPO=0 /D "NDEBUG" /D _DLL=1 /D _X86_=1 /FR /YX /FD /Zel -cbstring /QIfdiv- /QIf /GF /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i "$(BASEDIR)\inc\ddk\wxp" /i "$(BASEDIR)\inc" /i "." /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /machine:IX86
# ADD LINK32 ntoskrnl.lib hal.lib wmilib.lib /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry" /pdb:none /debug /debugtype:both /machine:IX86 /nodefaultlib /out:".\checked\Ext2Fsd.sys" /libpath:"$(BASEDIR)\libchk\i386" /driver /debug:notmapped,FULL /IGNORE:4001,4037,4039,4065,4070,4078,4087,4089,4096 /MERGE:_PAGE=PAGE /MERGE:_TEXT=.text /SECTION:INIT,d /MERGE:.rdata=.text /FULLBUILD /RELEASE /FORCE:MULTIPLE /OPT:REF /OPTIDATA /align:0x20 /osversion:4.00 /subsystem:native

!ENDIF 

# Begin Target

# Name "Ext2Fsd - Win32 Free"
# Name "Ext2Fsd - Win32 Checked"
# Begin Group "Source Files"

# PROP Default_Filter ".c;.cpp"
# Begin Source File

SOURCE=.\alloc.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\blockdev.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cache.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\char.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\cleanup.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\close.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\create.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\debug.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\devctl.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dirctl.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ext2.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\fileinfo.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\fsctl.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\fsd.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\group.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\init.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\inode.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\io.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lockctl.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\read.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\super.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\test.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\volinfo.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\write.c

!IF  "$(CFG)" == "Ext2Fsd - Win32 Free"

!ELSEIF  "$(CFG)" == "Ext2Fsd - Win32 Checked"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ".h"
# Begin Group "linux"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\linux\ext2_fs.h
# End Source File
# Begin Source File

SOURCE=.\linux\types.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\cache.h
# End Source File
# Begin Source File

SOURCE=.\ext2.h
# End Source File
# Begin Source File

SOURCE=.\fsd.h
# End Source File
# Begin Source File

SOURCE=.\io.h
# End Source File
# End Group
# End Target
# End Project
