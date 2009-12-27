/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/07/02 10:48:51 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/init.c,v $
    $Revision: 1.11 $
*/

/*!\file init.c
 * \brief Entry and Unload functions.
 * 
 * The Entry and Unload functions are needed to initialize and unload the
 * driver.
 */

/*!\mainpage WinLFS Code Documentation
 *
 * \section intro Introduction
 *
 * This chapter contains the documentation of the code written or modified
 * during this thesis. The missing parts can be found in the documentation of
 * the continuing studies diploma thesis.
 */

#include "ntifs.h"
#include "fsd.h"

//
// The FSDs global data
//
FSD_GLOBAL_DATA FsdGlobalData;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#endif

/*!
 * \brief Entry point of the driver.
 *
 * This function initializes the driver after loading.
 *
 * @param DriverObject DRIVER_OBJECT structure of the lower driver.
 * @param RegistryPath Unicode string to the driver's registry key.
 * @return If the driver initialization was successful.
 */
NTSTATUS
DriverEntry (IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
   UNICODE_STRING DeviceName;
   PCACHE_MANAGER_CALLBACKS CacheManagerCallbacks;

#if DBG
   UNICODE_STRING DosDeviceName;
#endif
   NTSTATUS Status;

   //
   // Print some info about the driver
   //
   DbgPrint (DRIVER_NAME ": " __DATE__ " " __TIME__
#if DBG
             ", checked"
#endif
             ", build , Copyright DL + DZ + JK\n");

   //
   // Initialize the global data
   //
   RtlZeroMemory (&FsdGlobalData, sizeof (FSD_GLOBAL_DATA));
   FsdGlobalData.Identifier.Type = FGD;
   FsdGlobalData.Identifier.Size = sizeof (FSD_GLOBAL_DATA);
   FsdGlobalData.DriverObject = DriverObject;
   FsdGlobalData.Irps = 0;

   InitializeListHead (&FsdGlobalData.VcbList);

   //
   // Initialize the dispatch entry points
   //
   DriverObject->MajorFunction[IRP_MJ_CREATE] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_CLOSE] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_READ] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_CLEANUP] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;
   DriverObject->MajorFunction[IRP_MJ_WRITE] =
      (PDRIVER_DISPATCH) Ext2FsdBuildRequest;

   //
   // Create the main device object
   //
   RtlInitUnicodeString (&DeviceName, DEVICE_NAME);
   Status =
      IoCreateDevice (DriverObject, 0, &DeviceName,
                      FILE_DEVICE_DISK_FILE_SYSTEM, 0, FALSE,
                      &FsdGlobalData.DeviceObject);
   if (NT_SUCCESS (Status))
   {
      ExInitializeResourceLite (&FsdGlobalData.Resource);

#if DBG
      RtlInitUnicodeString (&DosDeviceName, DOS_DEVICE_NAME);
      IoCreateSymbolicLink (&DosDeviceName, &DeviceName);
      ProcessNameOffset = FsdGetProcessNameOffset ();
#endif
      IoRegisterFileSystem (FsdGlobalData.DeviceObject);
   }
   return Status;
}

/*!\fn VOID DriverUnload (IN PDRIVER_OBJECT DriverObject)
 * \brief Function to free ressources.
 *
 * This function will be called by ZwUnloadDriver(). An application must call
 * IOCTL_PREPARE_TO_UNLOAD before the driver can be unloaded.
 *
 * @param DriverObject Caller-supplied pointer to a DRIVER_OBJECT structure.
 *                     This is the driver's driver object.
 */
#if DBG
#pragma code_seg(FSD_PAGED_CODE)

VOID
DriverUnload (IN PDRIVER_OBJECT DriverObject)
{
   UNICODE_STRING DosDeviceName;

   KdPrint ((DRIVER_NAME ": Unloading driver\n"));

   RtlInitUnicodeString (&DosDeviceName, DOS_DEVICE_NAME);

   IoDeleteSymbolicLink (&DosDeviceName);

   ExDeleteResourceLite (&FsdGlobalData.Resource);

#if (VER_PRODUCTBUILD < 2600)
   // moved to Ext2FsdPrepareToUnload()
   // IoDeleteDevice(FsdGlobalData.DeviceObject);
#endif
}

#pragma code_seg()            // end FSD_PAGED_CODE
#endif                        // DBG

