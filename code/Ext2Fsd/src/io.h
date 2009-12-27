/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Headerfile for io.c

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/io.h,v $
    $Revision: 1.5 $
*/

#ifndef _IO_H_
#define _IO_H_

#include "ext2.h"

#ifdef DRIVER
#define MALLOC(m) FsRtlAllocatePoolWithTag(PagedPool, m, '2txE')
#define FREE(m)   ExFreePool(m)
#define DEBUG     DbgPrint
#else
#define ASSERT(m) if(!m) exit(1)
#define MALLOC(m) malloc(m)
#define FREE(m)   free(m)
#define DEBUG     printf
#endif

_u32 READDISK (ext2_t * this, _u8 * buffer, _u64 offset, _u32 size);

#endif
