/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Helper functions for Char <-> WChar conversion

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/char.c,v $
    $Revision: 1.4 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FsdCharToWchar)
#pragma alloc_text(PAGE, FsdWcharToChar)
#endif

VOID
FsdCharToWchar (IN OUT PWCHAR Destination, IN PCHAR Source, IN ULONG Length)
{
   ULONG Index;

   ASSERT (Destination != NULL);
   ASSERT (Source != NULL);

   for (Index = 0; Index < Length; Index++)
   {
      Destination[Index] = (WCHAR) Source[Index];
   }
}

NTSTATUS
FsdWcharToChar (IN OUT PCHAR Destination, IN PWCHAR Source, IN ULONG Length)
{
   ULONG Index;
   NTSTATUS Status;

   ASSERT (Destination != NULL);
   ASSERT (Source != NULL);

   for (Index = 0, Status = STATUS_SUCCESS; Index < Length; Index++)
   {
      Destination[Index] = (CHAR) Source[Index];

      //
      // Check that the wide character fits in a normal character
      // but continue with the conversion anyway
      //
      if (((WCHAR) Destination[Index]) != Source[Index])
      {
         Status = STATUS_OBJECT_NAME_INVALID;
      }
   }

   return Status;
}
