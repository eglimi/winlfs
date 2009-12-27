/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_LOCK_CONTROL

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/lockctl.c,v $
    $Revision: 1.4 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdLockControl)
#endif


NTSTATUS
Ext2FsdLockControl (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   BOOLEAN CompleteRequest;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFILE_OBJECT FileObject;
   PFSD_FCB Fcb;
   PIRP Irp;

   __try
   {
      ASSERT (IrpContext != NULL);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         CompleteRequest = TRUE;
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      FileObject = IrpContext->FileObject;

      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb != NULL);

      if (Fcb->Identifier.Type == VCB)
      {
         CompleteRequest = TRUE;
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

      if (FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
      {
         CompleteRequest = TRUE;
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      Irp = IrpContext->Irp;

      //
      // While the file has any byte range locks we set IsFastIoPossible to
      // FastIoIsQuestionable so that the FastIoCheckIfPossible function is
      // called to check the locks for any fast I/O read/write operation.
      //
      if (Fcb->CommonFCBHeader.IsFastIoPossible != FastIoIsQuestionable)
      {
         KdPrint ((DRIVER_NAME ": %-16.16s %-31s %s\n",
                   FsdGetCurrentProcessName (), "FastIoIsQuestionable",
                   Fcb->AnsiFileName.Buffer));

         Fcb->CommonFCBHeader.IsFastIoPossible = FastIoIsNotPossible;
      }

      //
      // FsRtlProcessFileLock acquires FileObject->FsContext->Resource while
      // modifying the file locks and calls IoCompleteRequest when it's done.
      //

      CompleteRequest = FALSE;

      Status = FsRtlProcessFileLock (&Fcb->FileLock, Irp, NULL);

      if (Status != STATUS_SUCCESS)
      {
         KdPrint ((DRIVER_NAME ": %-16.16s %-31s *** Status: %s (%#x) ***\n",
                   FsdGetCurrentProcessName (), "IRP_MJ_LOCK_CONTROL",
                   FsdNtStatusToString (Status), Status));
      }
   }
   __finally
   {
      if (!IrpContext->ExceptionInProgress)
      {
         if (CompleteRequest)
         {
            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest (IrpContext->Irp,
                                (CCHAR) (NT_SUCCESS (Status) ? IO_DISK_INCREMENT
                                         : IO_NO_INCREMENT));
         }

         Ext2FsdFreeIrpContext (IrpContext);
      }
   }

   return Status;
}
