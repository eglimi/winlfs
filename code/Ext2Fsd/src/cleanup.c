/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_CLEANUP

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/cleanup.c,v $
    $Revision: 1.4 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdCleanup)
#endif


NTSTATUS
Ext2FsdCleanup (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   NTSTATUS Status = STATUS_SUCCESS;
   PFSD_VCB Vcb = NULL;
   BOOLEAN VcbResourceAcquired = FALSE;
   PFILE_OBJECT FileObject = NULL;
   PFSD_FCB Fcb = NULL;
   BOOLEAN FcbResourceAcquired = FALSE;
   PIRP Irp = NULL;

   __try
   {
      ASSERT (IrpContext != NULL);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      ASSERT (DeviceObject);

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         Status = STATUS_SUCCESS;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb);

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      if (!ExAcquireResourceExclusiveLite
          (&Vcb->MainResource, IrpContext->IsSynchronous))
      {
         Status = STATUS_PENDING;
         __leave;
      }

      VcbResourceAcquired = TRUE;

      FileObject = IrpContext->FileObject;

      ASSERT (FileObject);

      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb != NULL);

      if (Fcb->Identifier.Type == VCB)
      {
         if (FlagOn (Vcb->Flags, VCB_VOLUME_LOCKED))
         {
            ClearFlag (Vcb->Flags, VCB_VOLUME_LOCKED);

            FsdClearVpbFlag (Vcb->Vpb, VPB_LOCKED);
         }

         Status = STATUS_SUCCESS;

         __leave;
      }

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

#ifndef FSD_RO
      if (!FlagOn (Fcb->Flags, FCB_PAGE_FILE))
#endif
      {
         if (!ExAcquireResourceExclusiveLite
             (&Fcb->MainResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         FcbResourceAcquired = TRUE;
      }


      Irp = IrpContext->Irp;

      ASSERT (Irp);

      Fcb->OpenHandleCount--;

      Vcb->OpenFileHandleCount--;

      CcUninitializeCacheMap (FileObject, NULL, NULL);

      if (FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
      {
         FsRtlNotifyCleanup (Vcb->NotifySync, &Vcb->NotifyList,
                             FileObject->FsContext2);
      }
      else
      {
         //
         // Drop any byte range locks this process may have on the file.
         //
         FsRtlFastUnlockAll (&Fcb->FileLock, FileObject,
                             IoGetRequestorProcess (Irp), NULL);

         //
         // If there are no byte range locks owned by other processes on the
         // file the fast I/O read/write functions doesn't have to check for
         // locks so we set IsFastIoPossible to FastIoIsPossible again.
         //
         if (!FsRtlGetNextFileLock (&Fcb->FileLock, TRUE))
         {
            if (Fcb->CommonFCBHeader.IsFastIoPossible != FastIoIsPossible)
            {
               Fcb->CommonFCBHeader.IsFastIoPossible = FastIoIsNotPossible;
            }
         }
      }

#ifndef FSD_RO
      IoRemoveShareAccess (FileObject, &Fcb->ShareAccess);
#endif

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (IrpContext->FileObject)
      {
         SetFlag (IrpContext->FileObject->Flags, FO_CLEANUP_COMPLETE);
      }

      if (FcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Fcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

      if (!IrpContext->ExceptionInProgress)
      {
         if (Status == STATUS_PENDING)
         {
            FsdQueueRequest (IrpContext);
         }
         else
         {
            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest (IrpContext->Irp,
                                (CCHAR) (NT_SUCCESS (Status) ? IO_DISK_INCREMENT
                                         : IO_NO_INCREMENT));

            Ext2FsdFreeIrpContext (IrpContext);
         }
      }
   }

   return Status;
}
