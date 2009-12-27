/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_CLOSE

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/close.c,v $
    $Revision: 1.4 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdClose)
#pragma alloc_text(PAGE, Ext2FsdQueueCloseRequest)
#pragma alloc_text(PAGE, Ext2FsdDeQueueCloseRequest)
#endif

NTSTATUS
Ext2FsdClose (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   NTSTATUS Status = STATUS_SUCCESS;
   PFSD_VCB Vcb = NULL;
   BOOLEAN VcbResourceAcquired = FALSE;
   PFILE_OBJECT FileObject = NULL;
   PFSD_FCB Fcb = NULL;
   BOOLEAN FcbResourceAcquired = FALSE;
   PFSD_CCB Ccb = NULL;
   BOOLEAN FreeVcb = FALSE;

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
         Vcb->ReferenceCount--;

         if (!Vcb->ReferenceCount && FlagOn (Vcb->Flags, VCB_DISMOUNT_PENDING))
         {
            FreeVcb = TRUE;
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

      Ccb = (PFSD_CCB) FileObject->FsContext2;

      ASSERT (Ccb != NULL);

      ASSERT ((Ccb->Identifier.Type == CCB)
              && (Ccb->Identifier.Size == sizeof (FSD_CCB)));

      Fcb->ReferenceCount--;

      Vcb->ReferenceCount--;

      if (!Vcb->ReferenceCount && FlagOn (Vcb->Flags, VCB_DISMOUNT_PENDING))
      {
         FreeVcb = TRUE;
      }

      Ext2FsdFreeCcb (Ccb);

      if (!Fcb->ReferenceCount)
      {
         ExReleaseResourceForThreadLite (&Fcb->MainResource,
                                         ExGetCurrentResourceThread ());

         FcbResourceAcquired = FALSE;

         Ext2FsdFreeFcb (Fcb);
      }

      Status = STATUS_SUCCESS;
   }
   __finally
   {
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
            //
            // Close should never return STATUS_PENDING
            //

            Status = STATUS_SUCCESS;

            if (IrpContext->Irp != NULL)
            {
               IrpContext->Irp->IoStatus.Status = Status;

               FsdCompleteRequest (IrpContext->Irp,
                                   (CCHAR) (NT_SUCCESS (Status) ?
                                            IO_DISK_INCREMENT :
                                            IO_NO_INCREMENT));

               IrpContext->Irp = NULL;
            }

            Ext2FsdQueueCloseRequest (IrpContext);
         }
         else
         {
            if (IrpContext->Irp != NULL)
            {
               IrpContext->Irp->IoStatus.Status = Status;

               FsdCompleteRequest (IrpContext->Irp,
                                   (CCHAR) (NT_SUCCESS (Status) ?
                                            IO_DISK_INCREMENT :
                                            IO_NO_INCREMENT));
            }

            Ext2FsdFreeIrpContext (IrpContext);

            if (FreeVcb)
            {
               Ext2FsdFreeVcb (Vcb);
            }
         }
      }
   }

   return Status;
}

VOID
Ext2FsdQueueCloseRequest (IN PFSD_IRP_CONTEXT IrpContext)
{
   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   // IsSynchronous means we can block (so we don't requeue it)
   IrpContext->IsSynchronous = TRUE;

   ExInitializeWorkItem (&IrpContext->WorkQueueItem, Ext2FsdDeQueueCloseRequest,
                         IrpContext);

   ExQueueWorkItem (&IrpContext->WorkQueueItem, CriticalWorkQueue);
}

VOID
Ext2FsdDeQueueCloseRequest (IN PVOID Context)
{
   PFSD_IRP_CONTEXT IrpContext;

   IrpContext = (PFSD_IRP_CONTEXT) Context;

   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   __try
   {
      __try
      {
         FsRtlEnterFileSystem ();

         Ext2FsdClose (IrpContext);
      }
      __except (FsdExceptionFilter (IrpContext, GetExceptionCode ()))
      {
         FsdExceptionHandler (IrpContext);
      }
   }
   __finally
   {
      FsRtlExitFileSystem ();
   }
}
