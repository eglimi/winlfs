/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions for reading (and writing) to a block device.

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/blockdev.c,v $
    $Revision: 1.5 $
*/

#include "ntifs.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FsdBlockDeviceIoControl)
#pragma alloc_text(PAGE, Ext2ReadSync)
#pragma alloc_text(PAGE, FsdReadWriteBlockDevice)
#pragma alloc_text(PAGE, FsdReadWriteBlockDeviceCompletion)
#endif

NTSTATUS
FsdBlockDeviceIoControl (IN PDEVICE_OBJECT DeviceObject, IN ULONG IoctlCode,
                         IN PVOID InputBuffer, IN ULONG InputBufferSize,
                         IN OUT PVOID OutputBuffer,
                         IN OUT PULONG OutputBufferSize)
{
   ULONG OutputBufferSize2 = 0;
   KEVENT Event;
   PIRP Irp;
   IO_STATUS_BLOCK IoStatus;
   NTSTATUS Status;

   ASSERT (DeviceObject != NULL);

   if (OutputBufferSize)
   {
      OutputBufferSize2 = *OutputBufferSize;
   }

   KeInitializeEvent (&Event, NotificationEvent, FALSE);



   Irp =
      IoBuildDeviceIoControlRequest (IoctlCode, DeviceObject, InputBuffer,
                                     InputBufferSize, OutputBuffer,
                                     OutputBufferSize2, FALSE, &Event,
                                     &IoStatus);

   if (!Irp)
   {
      DbgPrint ("ERROR FsdReadWriteBlockDevice : !Irp \n");
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   Status = IoCallDriver (DeviceObject, Irp);

   if (Status == STATUS_PENDING)
   {
      KeWaitForSingleObject (&Event, Executive, KernelMode, FALSE, NULL);
      Status = IoStatus.Status;
   }

   if (OutputBufferSize)
   {
      *OutputBufferSize = (ULONG) IoStatus.Information;
   }

   return Status;
}



NTSTATUS
Ext2ReadSync (IN PDEVICE_OBJECT DeviceObject, IN LONGLONG Offset,
              IN ULONG Length, OUT PVOID Buffer, BOOLEAN bVerify)
{
   KEVENT Event;
   PIRP Irp;
   IO_STATUS_BLOCK IoStatus;
   NTSTATUS Status;


   ASSERT (DeviceObject != NULL);
   ASSERT (Buffer != NULL);

   KeInitializeEvent (&Event, NotificationEvent, FALSE);

   Irp =
      IoBuildSynchronousFsdRequest (IRP_MJ_READ, DeviceObject, Buffer, Length,
                                    (PLARGE_INTEGER) (&Offset), &Event,
                                    &IoStatus);


   if (!Irp)
   {
      DbgPrint ("ERROR Ext2ReadSync : !Irp");
      ExRaiseStatus (STATUS_INSUFFICIENT_RESOURCES);
   }

   if (bVerify)
      SetFlag (IoGetNextIrpStackLocation (Irp)->Flags,
               SL_OVERRIDE_VERIFY_VOLUME);

   Status = IoCallDriver (DeviceObject, Irp);

   if (Status == STATUS_PENDING)
   {
      KeWaitForSingleObject (&Event, Executive, KernelMode, FALSE, NULL);
      Status = IoStatus.Status;
   }

   return Status;
}


NTSTATUS
FsdReadWriteBlockDevice (IN ULONG Operation, IN PDEVICE_OBJECT DeviceObject,
                         IN PLARGE_INTEGER Offset, IN ULONG Length,
                         IN BOOLEAN OverrideVerify, IN OUT PVOID Buffer)
{
   PIRP Irp;
   IO_STATUS_BLOCK IoStatus;
   KEVENT Event;
   PIO_STACK_LOCATION IrpSp;

#ifndef FSD_RO
   ASSERT (Operation == IRP_MJ_READ || Operation == IRP_MJ_WRITE);
#else
   ASSERT (Operation == IRP_MJ_READ);
#endif
   ASSERT (DeviceObject != NULL);
   ASSERT (Offset != NULL);
   ASSERT (Buffer != NULL);

   Irp =
      IoBuildAsynchronousFsdRequest (Operation, DeviceObject, Buffer, Length,
                                     Offset, &IoStatus);

   if (!Irp)
   {
      DbgPrint ("ERROR FsdReadWriteBlockDevice : !Irp\n");
      ExRaiseStatus (STATUS_INSUFFICIENT_RESOURCES);
   }

   KeInitializeEvent (&Event, NotificationEvent, FALSE);

   Irp->UserEvent = &Event;

   IoSetCompletionRoutine (Irp, FsdReadWriteBlockDeviceCompletion, NULL, TRUE,
                           TRUE, TRUE);

   if (OverrideVerify)
   {
      IrpSp = IoGetNextIrpStackLocation (Irp);

      SetFlag (IrpSp->Flags, SL_OVERRIDE_VERIFY_VOLUME);
   }

   IoCallDriver (DeviceObject, Irp);

   KeWaitForSingleObject (&Event, Executive, KernelMode, FALSE, NULL);

   return IoStatus.Status;
}

NTSTATUS
FsdReadWriteBlockDeviceCompletion (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp,
                                   IN PVOID Context)
{
   PMDL Mdl;

   ASSERT (Irp != NULL);

   *Irp->UserIosb = Irp->IoStatus;

   KeSetEvent (Irp->UserEvent, 0, FALSE);

   while ((Mdl = Irp->MdlAddress))
   {
      Irp->MdlAddress = Mdl->Next;

      IoFreeMdl (Mdl);
   }

   IoFreeIrp (Irp);

   return STATUS_MORE_PROCESSING_REQUIRED;
}
