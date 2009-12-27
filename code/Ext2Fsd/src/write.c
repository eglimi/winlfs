/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/write.c,v $
    $Revision: 1.18 $
*/

/*!
 * \file write.c 
 * \brief Functions to handle IRP_MJ_WRITE.
 *
 * This file contains the functions needed for physically write down the
 * data.
 */

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FsdWrite)
#pragma alloc_text(PAGE, FsdWriteNormal)
#pragma alloc_text(PAGE, FsdWriteComplete)
#endif

/*!
 * \brief Dispatch Method for writing.
 *
 * This function calls FsdWriteNormal() when IRP_MN_NORMAL is set or
 * FsdWriteComplete() when IPR_MN_COMPLETE is set.
 *
 * @param  IrpContext Pointer to the context of the IRP
 * @return Status of called Function FsdWriteNormal() or FsdWriteComplete()
 */
NTSTATUS
FsdWrite (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   if (FlagOn (IrpContext->MinorFunction, IRP_MN_COMPLETE))
   {
      Status = FsdWriteComplete (IrpContext);
   }
   else
   {
      Status = FsdWriteNormal (IrpContext);
   }

   return Status;
}

/*!
 * \brief Writes down the data to the disk.
 *
 * @warning This function is not yet implemented.
 *
 * @param IrpContext Pointer to the context of the IRP
 * @return Status if the data were writing down successfully.
 */
NTSTATUS
FsdWriteNormal (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   __try
   {
      DbgPrint ("FsdWriteNormal");

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      DbgPrint ("FsdWriteNormal - finally");
   }

   return Status;
}

/*!
 * \brief Completes the writing request for a cached file.
 *
 * @warning This function is not yet implemented.
 *
 * It should call CcMdlWriteComplete() to free the memory descriptor lists.
 *
 * @param IrpContext Pointer to the context of the IRP
 * @return Status if the completiton of writing was successful.
 */
NTSTATUS
FsdWriteComplete (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   __try
   {
      DbgPrint ("FsdWriteComplete");

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      DbgPrint ("FsdWriteComplete - finally");
   }

   return Status;
}
