/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/07/02 14:51:52 $
    $Source: /cvsroot/winlfs/code/ExtFsr/ExtFsr.c,v $
    $Revision: 1.11 $
*/

/*!\file ExtFsr.c
 * \brief Complete file system recognizer.
 * 
 * This file contains all functions needed by file system recognizer.
 */

#include <ifs/ntifs.h>
#include "ExtFsr.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ReadDisk)
#pragma alloc_text(PAGE, FileSystemControl)
#pragma alloc_text(PAGE, LoadSuperBlock)
#pragma alloc_text(PAGE, ExtFsrUnload)
#endif

/*!
 * \brief Reads data from physical disk into memory.
 *
 * This function builds an IRP for a FSD request, calls the lower
 * driver  with  this  IRP  and wait  for the completion  of this
 * operation. The read data will be written to the Buffer.
 *
 * @param DeviceObject  Points  to  the  next-lower  driver's  device
 *                      object representing the target device for the
 *                      read operation.
 * @param Offset        Points to the offset on the disk to read from.
 * @param Length        Specifies  the  length,  in bytes,  of Buffer.
 * @param Buffer        Points to a buffer to receive data.
 * @return Status       Status of the Operation
 */
NTSTATUS
ReadDisk (IN PDEVICE_OBJECT DeviceObject, IN LONGLONG Offset, IN ULONG Length,
          OUT PVOID Buffer)
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
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   Status = IoCallDriver (DeviceObject, Irp);

   if (Status == STATUS_PENDING)
   {
      KeWaitForSingleObject (&Event, Suspended, KernelMode, FALSE, NULL);
      Status = IoStatus.Status;
   }

   return Status;
}

/*!
 * \brief Loads super block from disk.
 *
 * This function  allocates memory  from nonpaged pool for the super
 * block.  The superblock is  1024 bytes and  the sector size is 512
 * bytes. So we need two times the SECTOR_SIZE. As an offset we also
 * take  two times SECTOR_SIZE because the super block is the second
 * block on a partition.
 *
 * If the call of ReadDisk fails, the allocated memory will be freed
 * and a null pointer will be returned.  If a pointer  to the buffer
 * is  returned, the  allocated memory  must be  freed from  calling
 * function.
 *
 * @param    DeviceObject Device object of the mounted volume
 * @return   PEXT3_SUPER_BLOCK
 */
PEXT3_SUPER_BLOCK
LoadSuperBlock (IN PDEVICE_OBJECT DeviceObject)
{
   PVOID Buffer;
   NTSTATUS Status;

   Buffer = ExAllocatePool (NonPagedPool, 2 * SECTOR_SIZE);
   if (!Buffer)
   {
      KdPrint ((DRIVER_NAME ": Ext2LoadSuper(): no enough memory.\n"));
      return NULL;
   }

   Status = ReadDisk (DeviceObject, 2 * SECTOR_SIZE, 2 * SECTOR_SIZE, Buffer);
   if (!NT_SUCCESS (Status))
   {
      KdPrint ((DRIVER_NAME ": Read Block Device error.\n"));
      ExFreePool (Buffer);
      return NULL;
   }

   return Buffer;
}

/*!
 * \brief Manage mount requests and loads file system drivers.
 *
 * This function manages IRP_MN_MOUNT_VOLUME and IRP_MN_LOAD_FILE_SYSTEM.
 * If a ext3 file system is regocnized, it will try to load the ext3 driver.
 * If that was not successfully, the next request of the higher level driver
 * will try to load the ext2 file system.
 *
 * If ext2 and ext3 both either are loaded successfully or have failed to load, the
 * file system recognizer unloads itself.
 *
 * @param  DeviceObject  Points to the next-lower driver's device object representing
 *                       the target device for the read operation.
 * @param  Irp           I/O request packet.
 * @return If the routine succeeds, it must return STATUS_SUCCESS. 
 *         Otherwise, it must return one of the error status values 
 *         defined in ntstatus.h.
 */
NTSTATUS
FileSystemControl (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
   PIO_STACK_LOCATION IoStackLocation;
   NTSTATUS Status = STATUS_UNRECOGNIZED_VOLUME;
   UNICODE_STRING RegistryPath;
   PEXT3_SUPER_BLOCK ExtSuperBlock = NULL;
   DEVICE_EXTENSION Extension = NULL;
   PLOAD_STATUS LoadStatus = NULL;

   PAGED_CODE ();

   IoStackLocation = IoGetCurrentIrpStackLocation (Irp);
   ASSERT (IoStackLocation);

   Extension = (DEVICE_EXTENSION) (ExtFsrDeviceObject->DeviceExtension);

   switch (IoStackLocation->MinorFunction)
   {
   case IRP_MN_MOUNT_VOLUME:

      KdPrint ((DRIVER_NAME ": FileSystemControl(): IRP_MN_MOUNT_VOLUME\n"));

      ExtSuperBlock =
         (PEXT3_SUPER_BLOCK) LoadSuperBlock (IoStackLocation->Parameters.
                                             MountVolume.DeviceObject);

      Extension->journalling = FALSE;

      if (ExtSuperBlock)
      {
         if (ExtSuperBlock->s_magic == EXT_SUPER_MAGIC)
         {
            DbgPrint (DRIVER_NAME ": Ext file system has been found.\n");
            Status = STATUS_FS_DRIVER_REQUIRED;
            if (EXT3_HAS_COMPAT_FEATURE
                (ExtSuperBlock, EXT3_FEATURE_COMPAT_HAS_JOURNAL)
                && !Extension->ext3.failed && !Extension->ext3.loaded)
            {
               DbgPrint (DRIVER_NAME ": Ext3 file system has been found.\n");
               Extension->journalling = TRUE;
            }
            else if (Extension->ext2.failed || Extension->ext2.loaded)
            {
               Status = STATUS_UNRECOGNIZED_VOLUME;
            }
         }
         // ExFreePool(Ext2Sb);
         ExFreePool (ExtSuperBlock);
      }

      Irp->IoStatus.Status = Status;

      break;

   case IRP_MN_LOAD_FILE_SYSTEM:

      KdPrint ((DRIVER_NAME
                ": FileSystemControl(): IRP_MN_LOAD_FILE_SYSTEM\n"));

      if (Extension->journalling)
      {
         DbgPrint (DRIVER_NAME ": Load Ext3 file system driver.\n");
         LoadStatus = &(Extension->ext3);
         RtlInitUnicodeString (&RegistryPath, EXT3FSD_REGISTRY_PATH);
      }
      else if (!Extension->journalling)
      {
         DbgPrint (DRIVER_NAME ": Load Ext2 file system driver.\n");
         LoadStatus = &(Extension->ext2);
         RtlInitUnicodeString (&RegistryPath, EXT2FSD_REGISTRY_PATH);
      }
      else
      {
         DbgPrint (DRIVER_NAME
                   ": If you see this, there's something wrong ;)\n");
         Status = STATUS_INVALID_DEVICE_REQUEST;
         Irp->IoStatus.Status = Status;
         break;
      }

      // If the 
      if ((!LoadStatus->failed) && (!LoadStatus->loaded))
      {
         Status = ZwLoadDriver (&RegistryPath);

         if (!NT_SUCCESS (Status))
         {
            LoadStatus->failed = TRUE;
            LoadStatus->loaded = FALSE;
            DbgPrint (DRIVER_NAME ": ZwLoadDriver failed with error %xh.\n",
                      Status);
         }
         else
         {
            LoadStatus->failed = FALSE;
            LoadStatus->loaded = TRUE;
            DbgPrint (DRIVER_NAME ": ZwLoadDriver successful.\n");
         }
      }

      Irp->IoStatus.Status = Status;

      break;

   default:

      DbgPrint (DRIVER_NAME ": FILE_SYSTEM_CONTROL: "
                "Unknown minor function %xh\n", IoStackLocation->MinorFunction);

      Status = STATUS_INVALID_DEVICE_REQUEST;

      Irp->IoStatus.Status = Status;
   }

   IoCompleteRequest (Irp, IO_NO_INCREMENT);

   if ((Extension->ext2.loaded || Extension->ext2.failed)
       && (Extension->ext3.loaded || Extension->ext3.failed))
   {
      IoUnregisterFileSystem (ExtFsrDeviceObject);
      IoDeleteDevice (ExtFsrDeviceObject);
      RtlInitUnicodeString (&RegistryPath, EXTFSR_REGISTRY_PATH);
      ZwUnloadDriver (&RegistryPath);
      DbgPrint (DRIVER_NAME ": Unregistred.\n");
   }

   return Status;

}

/*!
 * \brief Unload function.
 *
 * This function cleans up reserved resources if this driver is
 * no longer in use.
 *
 * @param DriverObject  Points to the next-lower driver's device object representing
 *                      the target device for the read operation.
 */
VOID
ExtFsrUnload (IN PDRIVER_OBJECT DriverObject)
{
   DbgPrint (DRIVER_NAME ": Unload function called.\n");
   return;
};

/*!
 * \brief Entry point of this driver.
 *
 * It initializes global variables and registers the file system.
 *
 * @param DriverObject  Points to the next-lower driver's device object representing
 *                      the target device for the read operation.
 * @param RegistryPath  Pointer to a counted Unicode string specifying the path to
 *                      the driver's registry key.
 * @return If the routine succeeds, it must return STATUS_SUCCESS. 
 *         Otherwise, it must return one of the error status values 
 *         defined in ntstatus.h.
 */
NTSTATUS
DriverEntry (PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
   NTSTATUS Status = STATUS_SUCCESS;
   UNICODE_STRING driverName;
   UNICODE_STRING fileSystemName;
   DEVICE_EXTENSION Extension = NULL;

   DbgPrint ("ExtFsr DriverEntry...\n");

   DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FileSystemControl;
   DriverObject->DriverUnload = ExtFsrUnload;

   RtlInitUnicodeString (&driverName, FSD_RECOGNIZER_NAME);

   Status =
      IoCreateDevice (DriverObject, sizeof (ExtFsrDeviceExtension), &driverName,
                      FILE_DEVICE_DISK_FILE_SYSTEM, 0, FALSE,
                      &ExtFsrDeviceObject);

   if (!NT_SUCCESS (Status))
   {
      DbgPrint ("Error when creating DeviceObject.\n");
      return Status;
   }

   Extension = (DEVICE_EXTENSION) (ExtFsrDeviceObject->DeviceExtension);
   Extension->journalling = FALSE;

   IoRegisterFileSystem (ExtFsrDeviceObject);

   return Status;
}
