/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_DEVICE_CONTROL

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/devctl.c,v $
    $Revision: 1.6 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdDeviceControl)
#pragma alloc_text(PAGE, Ext2FsdPrepareToUnload)
#pragma alloc_text(PAGE, Ext2FsdDeviceControlNormal)
#pragma alloc_text(PAGE, Ext2FsdDeviceControlCompletion)
#endif


NTSTATUS
Ext2FsdDeviceControl (IN PFSD_IRP_CONTEXT IrpContext)
{
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   ULONG IoControlCode;
   NTSTATUS Status;

   ASSERT (IrpContext != NULL);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   Irp = IrpContext->Irp;

   IrpSp = IoGetCurrentIrpStackLocation (Irp);

   IoControlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

   switch (IoControlCode)
   {
#if DBG
   case IOCTL_PREPARE_TO_UNLOAD:
      Status = Ext2FsdPrepareToUnload (IrpContext);
      break;
#endif

   default:
      Status = Ext2FsdDeviceControlNormal (IrpContext);
   }

   return Status;
}

#if DBG

/*
 * A file system driver that has called IoRegisterFileSystem can not be
 * unloaded untill it has called IoUnregisterFileSystem. Therefor we implement
 * an IOCTL_PREPARE_TO_UNLOAD that an application can call to make the driver
 * ready to unload.
 */

NTSTATUS
Ext2FsdPrepareToUnload (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   BOOLEAN GlobalDataResourceAcquired = FALSE;

   __try
   {
      ASSERT (IrpContext != NULL);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      if (DeviceObject != FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      ExAcquireResourceExclusiveLite (&FsdGlobalData.Resource, TRUE);

      GlobalDataResourceAcquired = TRUE;

      if (FlagOn (FsdGlobalData.Flags, FSD_UNLOAD_PENDING))
      {
         KdPrint ((DRIVER_NAME ": *** Already ready to unload ***\n"));

         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      if (!IsListEmpty (&FsdGlobalData.VcbList))
      {
         KdPrint ((DRIVER_NAME ": *** Mounted volumes exists ***\n"));

         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      IoUnregisterFileSystem (FsdGlobalData.DeviceObject);

//#if (VER_PRODUCTBUILD >= 2600)
      IoDeleteDevice (FsdGlobalData.DeviceObject);
//#endif

      FsdGlobalData.DriverObject->DriverUnload = DriverUnload;

      SetFlag (FsdGlobalData.Flags, FSD_UNLOAD_PENDING);

      KdPrint ((DRIVER_NAME ": Driver is ready to unload\n"));

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (GlobalDataResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&FsdGlobalData.Resource,
                                         ExGetCurrentResourceThread ());
      }

//        if (!IrpContext->ExceptionInProgress)
      if (!AbnormalTermination ())
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

#endif // DBG

NTSTATUS
Ext2FsdDeviceControlNormal (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   BOOLEAN CompleteRequest;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb;
   PIRP Irp;
   PDEVICE_OBJECT TargetDeviceObject;

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

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      Irp = IrpContext->Irp;

      TargetDeviceObject = Vcb->TargetDeviceObject;

      //
      // Pass on the IOCTL to the driver below
      //

      CompleteRequest = FALSE;

#if DBG
      IoCopyCurrentIrpStackLocationToNext (Irp);

      IoSetCompletionRoutine (Irp, Ext2FsdDeviceControlCompletion, DeviceObject,
                              FALSE, TRUE, TRUE);
#else
      IoSkipCurrentIrpStackLocation (Irp);
#endif

      Status = IoCallDriver (TargetDeviceObject, Irp);
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

#if DBG

NTSTATUS
Ext2FsdDeviceControlCompletion (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp,
                                IN PVOID Context)
{
   if (Irp->PendingReturned)
   {
      IoMarkIrpPending (Irp);
   }

   KdPrint ((DRIVER_NAME ": %-16.16s %-31s *** Status: %s (%#x) ***\n",
             FsdGetCurrentProcessName (), "IRP_MJ_DEVICE_CONTROL",
             FsdNtStatusToString (Irp->IoStatus.Status), Irp->IoStatus.Status));

   return STATUS_SUCCESS;
}

#endif // DBG
