/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Dispatch and exception handling functions

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/fsd.c,v $
    $Revision: 1.5 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdBuildRequest)
#pragma alloc_text(PAGE, FsdQueueRequest)
#pragma alloc_text(PAGE, FsdDeQueueRequest)
#pragma alloc_text(PAGE, FsdDispatchRequest)
#pragma alloc_text(PAGE, FsdExceptionFilter)
#pragma alloc_text(PAGE, FsdExceptionHandler)
#pragma alloc_text(PAGE, FsdLockUserBuffer)
#pragma alloc_text(PAGE, FsdGetUserBuffer)
#endif

NTSTATUS
Ext2FsdBuildRequest (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
   BOOLEAN IsTopLevelIrp = FALSE;
   PFSD_IRP_CONTEXT IrpContext = NULL;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   BOOLEAN ResourceAcquired = FALSE;

   __try
   {
      __try
      {
         FsRtlEnterFileSystem ();


         DbgPrint ("Request # %i: ", FsdGlobalData.Irps);
         Ext2FsdDbgPrintCall (DeviceObject, Irp);
         FsdGlobalData.Irps++;



         if (!IoGetTopLevelIrp ())
         {
            IsTopLevelIrp = TRUE;
            IoSetTopLevelIrp (Irp);
         }

         IrpContext = Ext2FsdAllocateIrpContext (DeviceObject, Irp);

         if (!IrpContext)
         {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            DbgPrint ("ERROR BuildRequest : !IrpContext");
            Irp->IoStatus.Status = Status;
            FsdCompleteRequest (Irp, IO_NO_INCREMENT);
         }
         else
         {
            Status = FsdDispatchRequest (IrpContext);
         }
      }
      __except (FsdExceptionFilter (IrpContext, GetExceptionCode ()))
      {
         Status = FsdExceptionHandler (IrpContext);
      }
   }
   __finally
   {
      if (IsTopLevelIrp)
      {
         IoSetTopLevelIrp (NULL);
      }

      FsRtlExitFileSystem ();
   }

   return Status;
}

NTSTATUS
FsdQueueRequest (IN PFSD_IRP_CONTEXT IrpContext)
{
   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   // IsSynchronous means we can block (so we don't requeue it)
   IrpContext->IsSynchronous = TRUE;

   IoMarkIrpPending (IrpContext->Irp);

   ExInitializeWorkItem (&IrpContext->WorkQueueItem, FsdDeQueueRequest,
                         IrpContext);

   ExQueueWorkItem (&IrpContext->WorkQueueItem, CriticalWorkQueue);

   return STATUS_PENDING;
}

VOID
FsdDeQueueRequest (IN PVOID Context)
{
   PFSD_IRP_CONTEXT IrpContext = NULL;

   IrpContext = (PFSD_IRP_CONTEXT) Context;

   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   __try
   {
      __try
      {
         FsRtlEnterFileSystem ();

         if (!IrpContext->IsTopLevel)
         {
            IoSetTopLevelIrp ((PIRP) FSRTL_FSP_TOP_LEVEL_IRP);
         }

         FsdDispatchRequest (IrpContext);
      }
      __except (FsdExceptionFilter (IrpContext, GetExceptionCode ()))
      {
         FsdExceptionHandler (IrpContext);
      }
   }
   __finally
   {
      IoSetTopLevelIrp (NULL);

      FsRtlExitFileSystem ();
   }
}

NTSTATUS
FsdDispatchRequest (IN PFSD_IRP_CONTEXT IrpContext)
{
   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));



   switch (IrpContext->MajorFunction)
   {

   case IRP_MJ_CREATE:
      return Ext2FsdCreate (IrpContext);

   case IRP_MJ_CLOSE:
      return Ext2FsdClose (IrpContext);

   case IRP_MJ_READ:
      return Ext2FsdRead (IrpContext);

   case IRP_MJ_QUERY_INFORMATION:
      return Ext2FsdQueryInformation (IrpContext);

   case IRP_MJ_SET_INFORMATION:
      return Ext2FsdSetInformation (IrpContext);

   case IRP_MJ_QUERY_VOLUME_INFORMATION:
      return Ext2FsdQueryVolumeInformation (IrpContext);

   case IRP_MJ_DIRECTORY_CONTROL:
      return Ext2FsdDirectoryControl (IrpContext);

   case IRP_MJ_FILE_SYSTEM_CONTROL:
      return Ext2FsdFileSystemControl (IrpContext);

   case IRP_MJ_DEVICE_CONTROL:
      return Ext2FsdDeviceControl (IrpContext);

   case IRP_MJ_LOCK_CONTROL:
      return Ext2FsdLockControl (IrpContext);

   case IRP_MJ_CLEANUP:
      return Ext2FsdCleanup (IrpContext);

   case IRP_MJ_WRITE:
      return FsdWrite (IrpContext);


   default:
      KdPrint ((DRIVER_NAME
                ": FsdDispatchRequest: Unexpected major function: %#x\n",
                IrpContext->MajorFunction));
      IrpContext->Irp->IoStatus.Status = STATUS_DRIVER_INTERNAL_ERROR;
      FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
      Ext2FsdFreeIrpContext (IrpContext);
      return STATUS_DRIVER_INTERNAL_ERROR;
   }
}

NTSTATUS
FsdExceptionFilter (IN PFSD_IRP_CONTEXT IrpContext, IN NTSTATUS ExceptionCode)
{
   NTSTATUS Status;

   //
   // Only use a valid IrpContext
   //
   if (IrpContext)
   {
      if ((IrpContext->Identifier.Type != ICX)
          || (IrpContext->Identifier.Size != sizeof (FSD_IRP_CONTEXT)))
      {
         IrpContext = NULL;
      }
   }

   //
   // If the exception is expected execute our handler
   //
   if (FsRtlIsNtstatusExpected (ExceptionCode))
   {
      KdPrint ((DRIVER_NAME ": FsdExceptionFilter: Catching exception %#x\n",
                ExceptionCode));

      Status = EXCEPTION_EXECUTE_HANDLER;

      if (IrpContext)
      {
         IrpContext->ExceptionInProgress = TRUE;
         IrpContext->ExceptionCode = ExceptionCode;
      }
   }
   //
   // else continue search for an higher level exception handler
   //
   else
   {
      KdPrint ((DRIVER_NAME ": FsdExceptionFilter: Passing on exception %#x\n",
                ExceptionCode));

      Status = EXCEPTION_CONTINUE_SEARCH;

      if (IrpContext)
      {
         Ext2FsdFreeIrpContext (IrpContext);
      }
   }

   return Status;
}

NTSTATUS
FsdExceptionHandler (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   if (IrpContext)
   {
      Status = IrpContext->ExceptionCode;

      if (IrpContext->Irp)
      {
         IrpContext->Irp->IoStatus.Status = Status;

         FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
      }

      Ext2FsdFreeIrpContext (IrpContext);
   }
   else
   {
      DbgPrint ("ERROR BuildRequest : else");
      Status = STATUS_INSUFFICIENT_RESOURCES;
   }

   return Status;
}

NTSTATUS
FsdLockUserBuffer (IN PIRP Irp, IN ULONG Length, IN LOCK_OPERATION Operation)
{
   NTSTATUS Status;

   ASSERT (Irp != NULL);

   if (Irp->MdlAddress != NULL)
   {
      return STATUS_SUCCESS;
   }

   IoAllocateMdl (Irp->UserBuffer, Length, FALSE, FALSE, Irp);

   if (Irp->MdlAddress == NULL)
   {
      DbgPrint ("ERROR LockUserBuffer : Irp->MdlAddress == NULL");

      return STATUS_INSUFFICIENT_RESOURCES;
   }

   __try
   {
      MmProbeAndLockPages (Irp->MdlAddress, Irp->RequestorMode, Operation);

      Status = STATUS_SUCCESS;
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      IoFreeMdl (Irp->MdlAddress);

      Irp->MdlAddress = NULL;

      Status = STATUS_INVALID_USER_BUFFER;
   }

   return Status;
}

PVOID
FsdGetUserBuffer (IN PIRP Irp)
{
   ASSERT (Irp != NULL);

   if (Irp->MdlAddress)
   {
      return MmGetSystemAddressForMdl (Irp->MdlAddress);
   }
   else
   {
      return Irp->UserBuffer;
   }
}
