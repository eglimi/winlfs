/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: The READDISK Function for user- and kernel mode

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/io.c,v $
    $Revision: 1.4 $
*/

#include "ext2.h"
#include "io.h"

#ifndef DRIVER
#include "stdlib.h"
#include "stdio.h"
#include "windows.h"
#else
#include "fsd.h"
#endif

#ifndef DRIVER

HANDLE fd = 0;


_u32
READDISK (ext2_t * this, _u8 * buffer, _u64 offset, _u32 size)
{
   _u32 len = 0;
   _s32 sectors_count;
   _u64 first_sector;
   _s32 first_sector_offset;

   char *sector_buffer = NULL;

   first_sector_offset = (_u32) (offset.QuadPart % DISK_BLOCK_SIZE);
   first_sector.QuadPart = offset.QuadPart / DISK_BLOCK_SIZE;

   sectors_count =
      (_s32) (((offset.QuadPart + size) / DISK_BLOCK_SIZE) -
              (offset.QuadPart / DISK_BLOCK_SIZE));

   if (sectors_count == 0)
      sectors_count = 1;

   sector_buffer = MALLOC (sectors_count * DISK_BLOCK_SIZE);
   ASSERT (sector_buffer);

   //DEBUG("READDISK : readdisk at offset=%.4X%.4X size=%x!\n", offset.HighPart, offset.LowPart, size);
   //DEBUG("READDISK : readdisk at offset=0000%.4X size=%x!\n", first_sector.LowPart * DISK_BLOCK_SIZE, sectors_count * DISK_BLOCK_SIZE);

   if (fd == 0)
   {
      fd =
         CreateFile (L"e:\\hdb5", GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,
                     0);

      if (fd == INVALID_HANDLE_VALUE)
      {
         printf ("couldn't open the file.\n");
         exit (1);
      }
   }

   if (SetFilePointer
       (fd, first_sector.LowPart * DISK_BLOCK_SIZE, NULL,
        FILE_BEGIN) == 0xFFFFFFFF)
   {
      printf ("Couldn't set filepointer to %X\n", offset);
      exit (1);
   }

   if (!ReadFile
       (fd, sector_buffer, sectors_count * DISK_BLOCK_SIZE, &len, NULL)
       || sectors_count * DISK_BLOCK_SIZE != len)
   {
      printf ("Could not read data offset=%X size=%X len=%X\n", offset, size,
              len);
      exit (1);
   }

   RtlCopyMemory (buffer, sector_buffer + first_sector_offset, size);

   FREE (sector_buffer);

   return size;
}

#else

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, READDISK)
#endif


_u32
READDISK (ext2_t * this, _u8 * buffer, _u64 offset, _u32 size)
{
   NTSTATUS Status;
   _s32 sectors_count;
   _u64 first_sector;
   _s32 first_sector_offset;
   _s8 *sector_buffer = NULL;
   _s64 _offset;

   //This part aligns offset and size to DISK_BLOCK_SIZE
   first_sector_offset = (_u32) (offset.QuadPart % DISK_BLOCK_SIZE);
   first_sector.QuadPart = offset.QuadPart / DISK_BLOCK_SIZE;

   _offset.QuadPart = first_sector.QuadPart * DISK_BLOCK_SIZE;

   sectors_count = (size + first_sector_offset) / DISK_BLOCK_SIZE + 1;  //(_s32)(((offset.QuadPart+size) / DISK_BLOCK_SIZE) - (offset.QuadPart / DISK_BLOCK_SIZE));

   if (sectors_count == 0)
      sectors_count = 1;

   //-----------------------------------------------------

   sector_buffer = MALLOC (sectors_count * DISK_BLOCK_SIZE);
   ASSERT (sector_buffer);

   ASSERT (sector_buffer);

   //Status = FsdReadWriteBlockDevice (IRP_MJ_READ, this->DeviceObject, &_offset, sectors_count * DISK_BLOCK_SIZE, TRUE, sector_buffer);
#ifdef TRACE
   DEBUG
      ("READDISK : reading ... @buffer=%.8X _offset=%.8x%.8x sec=%i fso=%i size=%i",
       buffer, _offset.HighPart, _offset.LowPart, sectors_count,
       first_sector_offset, size);
#endif
   Status =
      Ext2ReadSync (this->DeviceObject, _offset.QuadPart,
                    sectors_count * DISK_BLOCK_SIZE, sector_buffer, TRUE);

#ifdef TRACE
   DEBUG ("READDISK : reading done ");
#endif

   if (!NT_SUCCESS (Status))
   {
#ifdef TRACE
      PUCHAR p = FsdNtStatusToString (Status);

      DEBUG
         ("ERROR READDISK : readdisk failed at offset=%.4X%.4X size=%x err=%s\n",
          offset.HighPart, offset.LowPart, size, p);
#endif

      size = 0;

      //Halt!
      ExRaiseStatus (Status);
   }
   else
   {
      RtlCopyMemory (buffer, sector_buffer + first_sector_offset, size);
   }

   FREE (sector_buffer);

   return size;
}

#endif
