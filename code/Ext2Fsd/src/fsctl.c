/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_FILE_SYSTEM_CONTROL

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/fsctl.c,v $
    $Revision: 1.4 $
*/

#include "ntifs.h"
#include "fsd.h"
#include "ext2.h"
#include "io.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdFileSystemControl)
#pragma alloc_text(PAGE, Ext2FsdUserFsRequest)
#pragma alloc_text(PAGE, Ext2FsdLockVolume)
#pragma alloc_text(PAGE, Ext2FsdUnlockVolume)
#pragma alloc_text(PAGE, Ext2FsdDismountVolume)
#pragma alloc_text(PAGE, Ext2FsdIsVolumeMounted)
#pragma alloc_text(PAGE, Ext2FsdMountVolume)
#pragma alloc_text(PAGE, Ext2FsdVerifyVolume)
#pragma alloc_text(PAGE, FsdPurgeVolume)
#pragma alloc_text(PAGE, FsdPurgeFile)
#pragma alloc_text(PAGE, FsdSetVpbFlag)
#pragma alloc_text(PAGE, FsdClearVpbFlag)
#endif


NTSTATUS
Ext2FsdFileSystemControl (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   ASSERT (IrpContext);
   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   switch (IrpContext->MinorFunction)
   {
   case IRP_MN_USER_FS_REQUEST:
      Status = Ext2FsdUserFsRequest (IrpContext);
      break;

   case IRP_MN_MOUNT_VOLUME:
      Status = Ext2FsdMountVolume (IrpContext);
      break;

   case IRP_MN_VERIFY_VOLUME:
      Status = Ext2FsdVerifyVolume (IrpContext);
      break;

   default:
      Status = STATUS_INVALID_DEVICE_REQUEST;
      IrpContext->Irp->IoStatus.Status = Status;
      FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
      Ext2FsdFreeIrpContext (IrpContext);
   }

   return Status;
}

NTSTATUS
Ext2FsdUserFsRequest (IN PFSD_IRP_CONTEXT IrpContext)
{
   PIRP Irp;
   PIO_STACK_LOCATION IrpSp;
   ULONG FsControlCode;
   NTSTATUS Status;

   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   Irp = IrpContext->Irp;

   IrpSp = IoGetCurrentIrpStackLocation (Irp);

#ifndef _GNU_NTIFS_
   FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
#else
   FsControlCode =
      ((PEXTENDED_IO_STACK_LOCATION) IrpSp)->Parameters.FileSystemControl.
      FsControlCode;
#endif

   switch (FsControlCode)
   {
   case FSCTL_LOCK_VOLUME:
      Status = Ext2FsdLockVolume (IrpContext);
      break;

   case FSCTL_UNLOCK_VOLUME:
      Status = Ext2FsdUnlockVolume (IrpContext);
      break;

   case FSCTL_DISMOUNT_VOLUME:
      Status = Ext2FsdDismountVolume (IrpContext);
      break;

   case FSCTL_IS_VOLUME_MOUNTED:
      Status = Ext2FsdIsVolumeMounted (IrpContext);
      break;

   default:
      Status = STATUS_INVALID_DEVICE_REQUEST;
      IrpContext->Irp->IoStatus.Status = Status;
      FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
      Ext2FsdFreeIrpContext (IrpContext);
   }

   return Status;
}


NTSTATUS
Ext2FsdLockVolume (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb;
   BOOLEAN VcbResourceAcquired = FALSE;

   __try
   {
      ASSERT (IrpContext != NULL);
      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);
      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      ExAcquireResourceSharedLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

      if (FlagOn (Vcb->Flags, VCB_VOLUME_LOCKED))
      {
         KdPrint ((DRIVER_NAME ": *** Volume is already locked ***\n"));
         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      if (Vcb->OpenFileHandleCount)
      {
         KdPrint ((DRIVER_NAME ": *** Open files exists ***\n"));
         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                      ExGetCurrentResourceThread ());
      VcbResourceAcquired = FALSE;
      FsdPurgeVolume (Vcb, TRUE);
      ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

      if (Vcb->ReferenceCount > 1)
      {
         KdPrint ((DRIVER_NAME ": *** Could not purge cached files ***\n"));

         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      SetFlag (Vcb->Flags, VCB_VOLUME_LOCKED);
      FsdSetVpbFlag (Vcb->Vpb, VPB_LOCKED);
      KdPrint ((DRIVER_NAME ": Volume locked\n"));
      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

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

NTSTATUS
Ext2FsdUnlockVolume (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb;
   BOOLEAN VcbResourceAcquired = FALSE;

   __try
   {
      ASSERT (IrpContext != NULL);
      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

      if (!FlagOn (Vcb->Flags, VCB_VOLUME_LOCKED))
      {
         KdPrint ((DRIVER_NAME ": *** Volume is not locked ***\n"));
         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      ClearFlag (Vcb->Flags, VCB_VOLUME_LOCKED);

      FsdClearVpbFlag (Vcb->Vpb, VPB_LOCKED);

      KdPrint ((DRIVER_NAME ": Volume unlocked\n"));

      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

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

NTSTATUS
Ext2FsdDismountVolume (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb;
   BOOLEAN VcbResourceAcquired = FALSE;

   __try
   {
      ASSERT (IrpContext != NULL);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

      if (!FlagOn (Vcb->Flags, VCB_VOLUME_LOCKED))
      {
         KdPrint ((DRIVER_NAME ": *** Volume is not locked ***\n"));
         Status = STATUS_ACCESS_DENIED;

         __leave;
      }

      SetFlag (Vcb->Flags, VCB_DISMOUNT_PENDING);
      KdPrint ((DRIVER_NAME ": Volume dismount pending\n"));
      Status = STATUS_SUCCESS;
   }
   __finally
   {
      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

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


NTSTATUS
Ext2FsdIsVolumeMounted (IN PFSD_IRP_CONTEXT IrpContext)
{
   ASSERT (IrpContext);
   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   return Ext2FsdVerifyVolume (IrpContext);
}


NTSTATUS
Ext2FsdMountVolume (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT MainDeviceObject;
   BOOLEAN GlobalDataResourceAcquired = FALSE;
   PIO_STACK_LOCATION IrpSp;
   PDEVICE_OBJECT TargetDeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PDEVICE_OBJECT VolumeDeviceObject = NULL;
   PFSD_VCB Vcb;
   BOOLEAN VcbResourceInitialized = FALSE;
   BOOLEAN NotifySyncInitialized = FALSE;
   LARGE_INTEGER Offset;
   ULONG IoctlSize;
   ext2_t *ext2 = NULL;

   __try
   {
      ASSERT (IrpContext != NULL);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      MainDeviceObject = IrpContext->DeviceObject;

      if (MainDeviceObject != FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      //acquires the given resource for exclusive access by the calling thread.
      ExAcquireResourceExclusiveLite (&FsdGlobalData.Resource, TRUE);

      GlobalDataResourceAcquired = TRUE;

      if (FlagOn (FsdGlobalData.Flags, FSD_UNLOAD_PENDING))
      {
         Status = STATUS_UNRECOGNIZED_VOLUME;
         __leave;
      }

      IrpSp = IoGetCurrentIrpStackLocation (IrpContext->Irp);

      TargetDeviceObject = IrpSp->Parameters.MountVolume.DeviceObject;

      ext2 = ext2_create ();

      ext2->DeviceObject = TargetDeviceObject;

      if (ext2_init (ext2) == EXT2_STATUS_OK)
         Status = STATUS_SUCCESS;
      else
         Status = STATUS_UNSUCCESSFUL;


      if (!NT_SUCCESS (Status))
      {

         __leave;
      }

      Status =
         IoCreateDevice (MainDeviceObject->DriverObject, sizeof (FSD_VCB), NULL,
                         FILE_DEVICE_DISK_FILE_SYSTEM, 0, FALSE,
                         &VolumeDeviceObject);

      if (!NT_SUCCESS (Status))
      {
         __leave;
      }

      VolumeDeviceObject->StackSize = TargetDeviceObject->StackSize;

      (IrpSp->Parameters.MountVolume.Vpb)->DeviceObject = VolumeDeviceObject;

      Vcb = (PFSD_VCB) VolumeDeviceObject->DeviceExtension;

      RtlZeroMemory (Vcb, sizeof (FSD_VCB));

      Vcb->Identifier.Type = VCB;
      Vcb->Identifier.Size = sizeof (FSD_VCB);

      ExInitializeResourceLite (&Vcb->MainResource);
      ExInitializeResourceLite (&Vcb->PagingIoResource);

      VcbResourceInitialized = TRUE;

      Vcb->Vpb = IrpSp->Parameters.MountVolume.Vpb;

      InitializeListHead (&Vcb->FcbList);
      InitializeListHead (&Vcb->NotifyList);

      FsRtlNotifyInitializeSync (&Vcb->NotifySync);
      NotifySyncInitialized = TRUE;

      Vcb->DeviceObject = VolumeDeviceObject;
      Vcb->TargetDeviceObject = TargetDeviceObject;
      Vcb->OpenFileHandleCount = 0;
      Vcb->ReferenceCount = 0;
      Vcb->Flags = 0;

      Vcb->ext2 = ext2;


      Offset.QuadPart = 0;

      //Ext2 doesn't have a Volumename .....
      Vcb->Vpb->VolumeLabelLength = 4 * 2 + 2;

      FsdCharToWchar (Vcb->Vpb->VolumeLabel, "ext2", 4 + 1);

      Vcb->Vpb->SerialNumber = 0x1234;

      IoctlSize = sizeof (DISK_GEOMETRY);

      Status =
         FsdBlockDeviceIoControl (TargetDeviceObject,
                                  IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                                  &Vcb->DiskGeometry, &IoctlSize);

      if (!NT_SUCCESS (Status))
      {
         __leave;
      }

      IoctlSize = sizeof (PARTITION_INFORMATION);

      Status =
         FsdBlockDeviceIoControl (TargetDeviceObject,
                                  IOCTL_DISK_GET_PARTITION_INFO, NULL, 0,
                                  &Vcb->PartitionInformation, &IoctlSize);

      if (!NT_SUCCESS (Status))
      {
         Vcb->PartitionInformation.StartingOffset.QuadPart = 0;
         Vcb->PartitionInformation.PartitionLength.QuadPart =
            Vcb->DiskGeometry.Cylinders.QuadPart *
            Vcb->DiskGeometry.TracksPerCylinder *
            Vcb->DiskGeometry.SectorsPerTrack *
            Vcb->DiskGeometry.BytesPerSector;
         Status = STATUS_SUCCESS;
      }

      InsertTailList (&FsdGlobalData.VcbList, &Vcb->Next);

      DEBUG ("Ext2FsdMountVolume : Ext2 partition found.\n");
   }
   __finally
   {
      if (GlobalDataResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&FsdGlobalData.Resource,
                                         ExGetCurrentResourceThread ());
      }

      if (!NT_SUCCESS (Status))
      {
         DEBUG ("Ext2FsdMountVolume : It wasn't a ext2 partition - cleanup.\n");
         if (ext2)
         {
            ext2_dispose (ext2);
         }

         if (NotifySyncInitialized)
         {
            FsRtlNotifyUninitializeSync (&Vcb->NotifySync);
         }

         if (VcbResourceInitialized)
         {
            ExDeleteResourceLite (&Vcb->MainResource);
            ExDeleteResourceLite (&Vcb->PagingIoResource);
         }

         if (VolumeDeviceObject)
         {
            IoDeleteDevice (VolumeDeviceObject);
         }
      }

      if (!IrpContext->ExceptionInProgress)
      {
         if (NT_SUCCESS (Status))
         {
            ClearFlag (VolumeDeviceObject->Flags, DO_DEVICE_INITIALIZING);
         }

         IrpContext->Irp->IoStatus.Status = Status;
         FsdCompleteRequest (IrpContext->Irp,
                             (CCHAR) (NT_SUCCESS (Status) ? IO_DISK_INCREMENT :
                                      IO_NO_INCREMENT));
         Ext2FsdFreeIrpContext (IrpContext);
      }
   }

   return Status;
}

NTSTATUS
Ext2FsdVerifyVolume (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb;
   BOOLEAN VcbResourceAcquired = FALSE;
   PIRP Irp;

   __try
   {
      ASSERT (IrpContext != NULL);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      DeviceObject = IrpContext->DeviceObject;

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

      if (!FlagOn (Vcb->TargetDeviceObject->Flags, DO_VERIFY_VOLUME))
      {
         Status = STATUS_SUCCESS;
         __leave;
      }

      Irp = IrpContext->Irp;

/*
        Status = FsdIsDeviceSameRomfs(
            Vcb->TargetDeviceObject,
            be32_to_cpu(Vcb->romfs_super_block->checksum)
            );
*/
      if (NT_SUCCESS (Status))
      {
         ClearFlag (Vcb->TargetDeviceObject->Flags, DO_VERIFY_VOLUME);

         KdPrint ((DRIVER_NAME ": Volume verify succeeded\n"));

         __leave;
      }
      else
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());

         VcbResourceAcquired = FALSE;

         FsdPurgeVolume (Vcb, FALSE);

         ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

         VcbResourceAcquired = TRUE;

         SetFlag (Vcb->Flags, VCB_DISMOUNT_PENDING);

         ClearFlag (Vcb->TargetDeviceObject->Flags, DO_VERIFY_VOLUME);

         KdPrint ((DRIVER_NAME ": Volume verify failed\n"));

         __leave;
      }
   }
   __finally
   {
      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

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


typedef struct _FCB_LIST_ENTRY
{
   PFSD_FCB Fcb;
   LIST_ENTRY Next;
} FCB_LIST_ENTRY, *PFCB_LIST_ENTRY;

VOID
FsdPurgeVolume (IN PFSD_VCB Vcb, IN BOOLEAN FlushBeforePurge)
{
   BOOLEAN VcbResourceAcquired = FALSE;
   PFSD_FCB Fcb;
   LIST_ENTRY FcbList;
   PLIST_ENTRY ListEntry;
   PFCB_LIST_ENTRY FcbListEntry;

   __try
   {
      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

      ExAcquireResourceSharedLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

#ifndef FSD_RO
      if (FlagOn (Vcb->Flags, VCB_READ_ONLY))
      {
         FlushBeforePurge = FALSE;
      }
#endif

      InitializeListHead (&FcbList);

      for (ListEntry = Vcb->FcbList.Flink; ListEntry != &Vcb->FcbList;
           ListEntry = ListEntry->Flink)
      {
         Fcb = CONTAINING_RECORD (ListEntry, FSD_FCB, Next);

         ExAcquireResourceExclusiveLite (&Fcb->MainResource, TRUE);

         Fcb->ReferenceCount++;

         ExReleaseResourceForThreadLite (&Fcb->MainResource,
                                         ExGetCurrentResourceThread ());

         FcbListEntry =
            Ext2FsdAllocatePool (NonPagedPool, sizeof (FCB_LIST_ENTRY), '1mTR');

         FcbListEntry->Fcb = Fcb;

         InsertTailList (&FcbList, &FcbListEntry->Next);
      }

      ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                      ExGetCurrentResourceThread ());

      VcbResourceAcquired = FALSE;

      while (!IsListEmpty (&FcbList))
      {
         ListEntry = RemoveHeadList (&FcbList);

         FcbListEntry = CONTAINING_RECORD (ListEntry, FCB_LIST_ENTRY, Next);

         Fcb = FcbListEntry->Fcb;

         FsdPurgeFile (Fcb, FlushBeforePurge);

         Fcb->ReferenceCount--;

         if (!Fcb->ReferenceCount)
         {
            KdPrint (("FsdFreeFcb %s\n", Fcb->AnsiFileName.Buffer));
            Ext2FsdFreeFcb (Fcb);
         }

         Ext2FsdFreePool (FcbListEntry);
      }

      KdPrint ((DRIVER_NAME ": Volume flushed and purged\n"));
   }
   __finally
   {
      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }
   }
}

VOID
FsdPurgeFile (IN PFSD_FCB Fcb, IN BOOLEAN FlushBeforePurge)
{
#ifndef FSD_RO
   IO_STATUS_BLOCK IoStatus;
#endif

   ASSERT (Fcb != NULL);

   ASSERT ((Fcb->Identifier.Type == FCB)
           && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

#ifndef FSD_RO
   if (FlushBeforePurge)
   {
      KdPrint (("CcFlushCache on %s\n", Fcb->AnsiFileName.Buffer));

      CcFlushCache (&Fcb->SectionObject, NULL, 0, &IoStatus);
   }
#endif

   if (Fcb->SectionObject.ImageSectionObject)
   {
      KdPrint (("MmFlushImageSection on %s\n", Fcb->AnsiFileName.Buffer));

      MmFlushImageSection (&Fcb->SectionObject, MmFlushForWrite);
   }

   if (Fcb->SectionObject.DataSectionObject)
   {
      KdPrint (("CcPurgeCacheSection on %s\n", Fcb->AnsiFileName.Buffer));

      CcPurgeCacheSection (&Fcb->SectionObject, NULL, 0, FALSE);
   }
}


VOID
FsdSetVpbFlag (IN PVPB Vpb, IN USHORT Flag)
{
   KIRQL OldIrql;

   IoAcquireVpbSpinLock (&OldIrql);

   Vpb->Flags |= Flag;

   IoReleaseVpbSpinLock (OldIrql);
}

VOID
FsdClearVpbFlag (IN PVPB Vpb, IN USHORT Flag)
{
   KIRQL OldIrql;

   IoAcquireVpbSpinLock (&OldIrql);

   Vpb->Flags &= ~Flag;

   IoReleaseVpbSpinLock (OldIrql);
}
