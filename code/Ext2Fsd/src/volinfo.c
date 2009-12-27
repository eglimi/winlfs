/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_QUERY_VOLUME_INFORMATION

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/volinfo.c,v $
    $Revision: 1.5 $
*/

#include "ntifs.h"
#include "fsd.h"
#include "ext2.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdQueryVolumeInformation)
#endif

NTSTATUS
Ext2FsdQueryVolumeInformation (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb = NULL;
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   FS_INFORMATION_CLASS FsInformationClass;
   ULONG Length;
   PVOID SystemBuffer = NULL;
   BOOLEAN VcbResourceAcquired = FALSE;

   __try
   {
      ASSERT (IrpContext != NULL);

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

      if (!ExAcquireResourceSharedLite
          (&Vcb->MainResource, IrpContext->IsSynchronous))
      {
         Status = STATUS_PENDING;
         __leave;
      }

      VcbResourceAcquired = TRUE;

      Irp = IrpContext->Irp;
      ASSERT (Irp);

      IrpSp = IoGetCurrentIrpStackLocation (Irp);
      ASSERT (IrpSp);

      FsInformationClass = IrpSp->Parameters.QueryVolume.FsInformationClass;

      Length = IrpSp->Parameters.QueryVolume.Length;

      SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
      ASSERT (SystemBuffer);

      RtlZeroMemory (SystemBuffer, Length);

      switch (FsInformationClass)
      {
      case FileFsVolumeInformation:
         {
            PFILE_FS_VOLUME_INFORMATION Buffer;
            ULONG VolumeLabelLength;
            ULONG RequiredLength;

            if (Length < sizeof (FILE_FS_VOLUME_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }
#ifdef TRACE
            DbgPrint
               ("Ext2FsdQueryVolumeInformation : FileFsVolumeInformation\n");
#endif

            Buffer = (PFILE_FS_VOLUME_INFORMATION) SystemBuffer;

            Buffer->VolumeCreationTime.QuadPart = 0;
            Buffer->VolumeSerialNumber = 0xAFFE;        //It's a joke!

            VolumeLabelLength = 4 + 1;  //Ext2 doesn't have a volume label, so we take 'ext2'
            Buffer->VolumeLabelLength = VolumeLabelLength * 2;

            // I don't know what this means.
            Buffer->SupportsObjects = FALSE;

            RequiredLength =
               sizeof (FILE_FS_VOLUME_INFORMATION) + VolumeLabelLength * 2 -
               sizeof (WCHAR);

            if (Length < RequiredLength)
            {
               DbgPrint ("Ext2FsdQueryVolumeInformation[1] : L=%i RL=%i\n",
                         Length, RequiredLength);
               Irp->IoStatus.Information = sizeof (FILE_FS_VOLUME_INFORMATION);
               Status = STATUS_BUFFER_OVERFLOW;
               __leave;
            }

            FsdCharToWchar (Buffer->VolumeLabel, "ext2", VolumeLabelLength);

            Irp->IoStatus.Information = RequiredLength;
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileFsSizeInformation:
         {
            PFILE_FS_SIZE_INFORMATION Buffer;

#ifdef TRACE
            DbgPrint
               ("Ext2FsdQueryVolumeInformation : FileFsSizeInformation\n");
#endif

            if (Length < sizeof (FILE_FS_SIZE_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_FS_SIZE_INFORMATION) SystemBuffer;

            // On a readonly filesystem total size is the size of the
            // contents and available size is zero

            Buffer->TotalAllocationUnits.QuadPart = 0;  //Vcb->ext2->superblock.s_blocks_count;
            Buffer->AvailableAllocationUnits.QuadPart = 0;      //Vcb->ext2->block_size;

            Buffer->SectorsPerAllocationUnit =
               Vcb->ext2->block_size / Vcb->DiskGeometry.BytesPerSector;

            Buffer->BytesPerSector = Vcb->DiskGeometry.BytesPerSector;

            Irp->IoStatus.Information = sizeof (FILE_FS_SIZE_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileFsDeviceInformation:
         {
            PFILE_FS_DEVICE_INFORMATION Buffer;

#ifdef TRACE
            DbgPrint
               ("Ext2FsdQueryVolumeInformation : FileFsDeviceInformation\n");
#endif

            if (Length < sizeof (FILE_FS_DEVICE_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_FS_DEVICE_INFORMATION) SystemBuffer;

            Buffer->DeviceType = Vcb->TargetDeviceObject->DeviceType;

            Buffer->Characteristics = Vcb->TargetDeviceObject->Characteristics;


            SetFlag (Buffer->Characteristics, FILE_READ_ONLY_DEVICE);

            Irp->IoStatus.Information = sizeof (FILE_FS_DEVICE_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileFsAttributeInformation:
         {
            PFILE_FS_ATTRIBUTE_INFORMATION Buffer;
            ULONG RequiredLength;

#ifdef TRACE
            DbgPrint
               ("Ext2FsdQueryVolumeInformation : FileFsAttributeInformation\n");
#endif

            if (Length < sizeof (FILE_FS_ATTRIBUTE_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_FS_ATTRIBUTE_INFORMATION) SystemBuffer;

            Buffer->FileSystemAttributes =
               FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES |
               FILE_READ_ONLY_VOLUME;

            Buffer->MaximumComponentNameLength = EXT2_NAME_LEN;

            RequiredLength =
               sizeof (FILE_FS_ATTRIBUTE_INFORMATION) +
               sizeof (DRIVER_NAME) * 2 - sizeof (WCHAR);

            if (Length < RequiredLength)
            {
               Buffer->FileSystemNameLength = 0;
               Irp->IoStatus.Information =
                  sizeof (FILE_FS_ATTRIBUTE_INFORMATION);
               DbgPrint ("Ext2FsdQueryVolumeInformation[1] : L=%i RL=%i\n",
                         Length, RequiredLength);
               //Status = STATUS_BUFFER_OVERFLOW;
               //__leave;
            }
            else
            {
               Buffer->FileSystemNameLength = sizeof (DRIVER_NAME) * 2;
               FsdCharToWchar (Buffer->FileSystemName, DRIVER_NAME,
                               sizeof (DRIVER_NAME));

               Irp->IoStatus.Information = RequiredLength;
            }

            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileFsFullSizeInformation:
         {
            PFILE_FS_FULL_SIZE_INFORMATION Buffer;

#ifdef TRACE
            DbgPrint
               ("Ext2FsdQueryVolumeInformation : FileFsFullSizeInformation\n");
#endif

            if (Length < sizeof (FILE_FS_FULL_SIZE_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_FS_FULL_SIZE_INFORMATION) SystemBuffer;

            // On a readonly filesystem total size is the size of the
            // contents and available size is zero

            Buffer->TotalAllocationUnits.QuadPart = 0;  //Vcb->ext2->superblock.s_blocks_count;

            Buffer->CallerAvailableAllocationUnits.QuadPart = 0;
            Buffer->ActualAvailableAllocationUnits.QuadPart = 0;

            Buffer->SectorsPerAllocationUnit =
               Vcb->ext2->block_size / Vcb->DiskGeometry.BytesPerSector;

            Buffer->BytesPerSector = Vcb->DiskGeometry.BytesPerSector;

            Irp->IoStatus.Information = sizeof (FILE_FS_FULL_SIZE_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }
      default:
         Status = STATUS_INVALID_INFO_CLASS;
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
