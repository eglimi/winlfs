/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_READ

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/read.c,v $
    $Revision: 1.5 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdRead)
#pragma alloc_text(PAGE, Ext2FsdReadNormal)
#pragma alloc_text(PAGE, Ext2FsdReadComplete)
#endif


NTSTATUS
Ext2FsdRead (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   if (FlagOn (IrpContext->MinorFunction, IRP_MN_COMPLETE))
   {
      Status = Ext2FsdReadComplete (IrpContext);
   }
   else
   {
      Status = Ext2FsdReadNormal (IrpContext);
   }

   return Status;
}

NTSTATUS
Ext2FsdReadNormal (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb = NULL;
   PFILE_OBJECT FileObject = NULL;
   PFSD_FCB Fcb = NULL;
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   ULONG Length;
   ULONG ReturnedLength;
   LARGE_INTEGER ByteOffset;
   BOOLEAN PagingIo;
   BOOLEAN Nocache;
   BOOLEAN SynchronousIo;
   BOOLEAN VcbResourceAcquired = FALSE;
   BOOLEAN FcbMainResourceAcquired = FALSE;
   BOOLEAN FcbPagingIoResourceAcquired = FALSE;
   PUCHAR UserBuffer;
   PDEVICE_OBJECT DeviceToVerify = NULL;

   __try
   {
      ASSERT (IrpContext);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;
      ASSERT (DeviceObject);

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      FileObject = IrpContext->FileObject;
      ASSERT (FileObject);

      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb);

      if (Fcb->Identifier.Type == VCB)
      {
         Irp = IrpContext->Irp;

         ASSERT (Irp);

         IrpSp = IoGetCurrentIrpStackLocation (Irp);

         ASSERT (IrpSp);

         Length = IrpSp->Parameters.Read.Length;
         ByteOffset = IrpSp->Parameters.Read.ByteOffset;

         PagingIo = (BOOLEAN) FlagOn (Irp->Flags, IRP_PAGING_IO);
         Nocache = (BOOLEAN) FlagOn (Irp->Flags, IRP_NOCACHE);
         SynchronousIo =
            (BOOLEAN) FlagOn (FileObject->Flags, FO_SYNCHRONOUS_IO);

         if (Length == 0)
         {
            Irp->IoStatus.Information = 0;
            Status = STATUS_SUCCESS;
            __leave;
         }

/*
            if (!Nocache ||
                ByteOffset.LowPart & (DISK_BLOCK_SIZE - 1) ||
                Length & (DISK_BLOCK_SIZE - 1))
            {
                Status = STATUS_INVALID_PARAMETER;
                __leave;
            }
*/
         if (!ExAcquireResourceSharedLite
             (&Vcb->MainResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         VcbResourceAcquired = TRUE;

         if (ByteOffset.QuadPart >=
             Vcb->PartitionInformation.PartitionLength.QuadPart)
         {
            Irp->IoStatus.Information = 0;
            Status = STATUS_END_OF_FILE;
            __leave;
         }

         if ((ByteOffset.QuadPart + Length) >
             Vcb->PartitionInformation.PartitionLength.QuadPart)
         {
            Length =
               (ULONG) (Vcb->PartitionInformation.PartitionLength.QuadPart -
                        ByteOffset.QuadPart);

            Length &= ~(DISK_BLOCK_SIZE - 1);
         }

         UserBuffer = FsdGetUserBuffer (Irp);

         if (UserBuffer == NULL)
         {
            Status = STATUS_INVALID_USER_BUFFER;
            __leave;
         }

         Status =
            FsdReadWriteBlockDevice (IRP_MJ_READ, Vcb->TargetDeviceObject,
                                     &ByteOffset, Length, FALSE, UserBuffer);

         if (Status == STATUS_VERIFY_REQUIRED)
         {
            Status = IoVerifyVolume (Vcb->TargetDeviceObject, FALSE);

            if (NT_SUCCESS (Status))
            {
               Status =
                  FsdReadWriteBlockDevice (IRP_MJ_READ, Vcb->TargetDeviceObject,
                                           &ByteOffset, Length, FALSE,
                                           UserBuffer);
            }
         }

         if (NT_SUCCESS (Status))
         {
            Irp->IoStatus.Information = Length;
         }

         __leave;
      }

      DbgPrint ("Ext2FsdReadNormal : FCB");

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

      if (FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
      {
         DbgPrint ("ERROR : Ext2FsdReadNormal : STATUS_INVALID_PARAMETER");
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      Irp = IrpContext->Irp;
      ASSERT (Irp);

      IrpSp = IoGetCurrentIrpStackLocation (Irp);
      ASSERT (IrpSp);

      Length = IrpSp->Parameters.Read.Length;
      ByteOffset = IrpSp->Parameters.Read.ByteOffset;

      PagingIo = (BOOLEAN) FlagOn (Irp->Flags, IRP_PAGING_IO);
      Nocache = TRUE;
      SynchronousIo = (BOOLEAN) FlagOn (FileObject->Flags, FO_SYNCHRONOUS_IO);

      DbgPrint ("Ext2FsdReadNormal : length=%i offset=%i", Length, ByteOffset);

      if (Irp->RequestorMode != KernelMode && !Irp->MdlAddress
          && Irp->UserBuffer)
      {
         ProbeForWrite (Irp->UserBuffer, Length, 1);
      }

      if (Length == 0)
      {
         Irp->IoStatus.Information = 0;
         Status = STATUS_SUCCESS;
         __leave;
      }

/*
        if (Nocache &&
           (ByteOffset.LowPart & (DISK_BLOCK_SIZE - 1) ||
            Length & (DISK_BLOCK_SIZE - 1)))
        {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
        }
*/
      if (FlagOn (IrpContext->MinorFunction, IRP_MN_DPC))
      {
         ClearFlag (IrpContext->MinorFunction, IRP_MN_DPC);
         Status = STATUS_PENDING;
         __leave;
      }

      if (!PagingIo)
      {
         if (!ExAcquireResourceSharedLite
             (&Fcb->MainResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         FcbMainResourceAcquired = TRUE;
      }
      else
      {
         if (!ExAcquireResourceSharedLite
             (&Fcb->PagingIoResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         FcbPagingIoResourceAcquired = TRUE;
      }

      // end of file test

      if (ByteOffset.LowPart >= Fcb->inode->i_size)
      {
         Irp->IoStatus.Information = 0;
         Status = STATUS_END_OF_FILE;
         __leave;
      }

      UserBuffer = FsdGetUserBuffer (Irp);

      if (UserBuffer == NULL)
      {
         Status = STATUS_INVALID_USER_BUFFER;
         __leave;
      }

      DbgPrint ("Ext2FsdReadNormal : ext2_read_inode() ...");

      if (ext2_read_inode
          (Vcb->ext2, Fcb->inode, UserBuffer, ByteOffset.LowPart, Length,
           &Irp->IoStatus.Information) != EXT2_STATUS_OK)
      {
         Status = STATUS_UNSUCCESSFUL;
         DbgPrint ("Ext2FsdReadNormal : ext2_read_inode() failed");
         __leave;
      }

      DbgPrint ("Ext2FsdReadNormal : ext2_read_inode() done");

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (FcbPagingIoResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Fcb->PagingIoResource,
                                         ExGetCurrentResourceThread ());
      }

      if (FcbMainResourceAcquired)
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
            Status = FsdLockUserBuffer (IrpContext->Irp, Length, IoWriteAccess);

            if (NT_SUCCESS (Status))
            {
               Status = FsdQueueRequest (IrpContext);
            }
            else
            {
               IrpContext->Irp->IoStatus.Status = Status;
               FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
               Ext2FsdFreeIrpContext (IrpContext);
            }
         }
         else
         {
            IrpContext->Irp->IoStatus.Status = Status;

            if (SynchronousIo && !PagingIo && NT_SUCCESS (Status))
            {
               FileObject->CurrentByteOffset.QuadPart =
                  ByteOffset.QuadPart + Irp->IoStatus.Information;
            }

            if (!PagingIo && NT_SUCCESS (Status))
            {
               FileObject->Flags &= ~FO_FILE_FAST_IO_READ;
            }

            FsdCompleteRequest (IrpContext->Irp,
                                (CCHAR) (NT_SUCCESS (Status) ? IO_DISK_INCREMENT
                                         : IO_NO_INCREMENT));

            Ext2FsdFreeIrpContext (IrpContext);
         }
      }
   }

   return Status;
}

NTSTATUS
Ext2FsdReadComplete (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFILE_OBJECT FileObject = NULL;
   PIRP Irp = NULL;

   __try
   {
      DbgPrint ("Ext2FsdReadComplete");

      ASSERT (IrpContext);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      FileObject = IrpContext->FileObject;

      ASSERT (FileObject);

      Irp = IrpContext->Irp;

      ASSERT (Irp);

      CcMdlReadComplete (FileObject, Irp->MdlAddress);

      Irp->MdlAddress = NULL;

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (!IrpContext->ExceptionInProgress)
      {
         IrpContext->Irp->IoStatus.Status = Status;

         FsdCompleteRequest (IrpContext->Irp,
                             (CCHAR) (NT_SUCCESS (Status) ? IO_DISK_INCREMENT :
                                      IO_NO_INCREMENT));

         Ext2FsdFreeIrpContext (IrpContext);
      }
   }

   return Status;
}
