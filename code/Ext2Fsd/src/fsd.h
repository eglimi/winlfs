/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Headerfile for the ext2 file system driver

  CVS Information:
    $Date: 2003/07/02 08:34:48 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/fsd.h,v $
    $Revision: 1.14 $
*/

#ifndef _FSD_
#define _FSD_

#include <ntdddisk.h>
#include "ext2.h"

#undef ASSERT
#define ASSERT(m) if(!(m)) {DbgPrint("ASSERT : %s(%d): %s\n", __FILE__, __LINE__, #m); for(;;);}

//
// Name for the driver and it's main device
//
#define DRIVER_NAME     "Ext2Fsd"
#define DEVICE_NAME     L"\\Ext2Fsd"
#if DBG
#define DOS_DEVICE_NAME L"\\DosDevices\\Ext2Fsd"
#endif

//
// Define FSD_RO to compile a read-only driver
//
// Currently this is an implementation of a read-only driver and romfs was
// designed for read-only use but some code needed for write support is
// included anyway for use as documentation or a base for implementing a
// writeable driver
//
//#define FSD_RO

//
// Comment out these to make all driver code nonpaged
//
#define FSD_INIT_CODE   "INIT"
#define FSD_PAGED_CODE  "PAGE"

//
// Private IOCTL to make the driver ready to unload
//
#define IOCTL_PREPARE_TO_UNLOAD \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 2048, METHOD_NEITHER, FILE_WRITE_ACCESS)

#ifndef SetFlag
#define SetFlag(x,f)    ((x) |= (f))
#endif

#ifndef ClearFlag
#define ClearFlag(x,f)  ((x) &= ~(f))
#endif

#define IsFlagOn(a,b) ((BOOLEAN)(FlagOn(a,b) ? TRUE : FALSE))

typedef PVOID PBCB;

//
// FSD_IDENTIFIER_TYPE
//
// Identifiers used to mark the structures
//
typedef enum _FSD_IDENTIFIER_TYPE
{
   FGD = ':DGF',
   VCB = ':BCV',
   FCB = ':BCF',
   CCB = ':BCC',
   ICX = ':XCI',
   FSD = ':DSF'
} FSD_IDENTIFIER_TYPE;

//
// FSD_IDENTIFIER
//
// Header used to mark the structures
//
typedef struct _FSD_IDENTIFIER
{
   FSD_IDENTIFIER_TYPE Type;
   ULONG Size;
} FSD_IDENTIFIER, *PFSD_IDENTIFIER;

//
// FSD_GLOBAL_DATA
//
// Data that is not specific to a mounted volume
//
typedef struct _FSD_GLOBAL_DATA
{

   // Identifier for this structure
   FSD_IDENTIFIER Identifier;

   // Syncronization primitive for this structure
   ERESOURCE Resource;

   // Pointer to the driver object
   PDRIVER_OBJECT DriverObject;

   // Pointer to the main device object
   PDEVICE_OBJECT DeviceObject;

   // List of mounted volumes
   LIST_ENTRY VcbList;

   // Global flags for the driver
   ULONG Flags;

   ULONG Irps;

} FSD_GLOBAL_DATA, *PFSD_GLOBAL_DATA;

//
// Flags for FSD_GLOBAL_DATA
//
#define FSD_UNLOAD_PENDING      0x00000001

//
// The global data is declared in init.c
//
extern FSD_GLOBAL_DATA FsdGlobalData;

//
// FSD_VCB Volume Control Block
//
// Data that represents a mounted logical volume
// It is allocated as the device extension of the volume device object
//
typedef struct _FSD_VCB
{

   // FCB header required by NT
   // The VCB is also used as an FCB for file objects
   // that represents the volume itself
   FSRTL_COMMON_FCB_HEADER CommonFCBHeader;
   SECTION_OBJECT_POINTERS SectionObject;
   ERESOURCE MainResource;
   ERESOURCE PagingIoResource;
   // end FCB header required by NT

   // Identifier for this structure
   FSD_IDENTIFIER Identifier;

   // List of VCBs
   LIST_ENTRY Next;

   // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
   // for files on this volume.
   ULONG OpenFileHandleCount;

   // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
   // for both files on this volume and open instances of the
   // volume itself.
   ULONG ReferenceCount;

   // Pointer to the VPB in the target device object
   PVPB Vpb;

   // List of FCBs for open files on this volume
   LIST_ENTRY FcbList;

   // List of IRPs pending on directory change notify requests
   LIST_ENTRY NotifyList;

   // Pointer to syncronization primitive for this list
   PNOTIFY_SYNC NotifySync;

   // This volumes device object
   PDEVICE_OBJECT DeviceObject;

   // The physical device object (the disk)
   PDEVICE_OBJECT TargetDeviceObject;

   // Information about the physical device object
   DISK_GEOMETRY DiskGeometry;
   PARTITION_INFORMATION PartitionInformation;

   // Pointer to the file system superblock
   ext2_t *ext2;

   // Flags for the volume
   ULONG Flags;

} FSD_VCB, *PFSD_VCB;

//
// Flags for FSD_VCB
//
#define VCB_VOLUME_LOCKED       0x00000001
#define VCB_DISMOUNT_PENDING    0x00000002
#define VCB_READ_ONLY           0x00000004

//
// FSD_FCB File Control Block
//
// Data that represents an open file
// There is a single instance of the FCB for every open file
//
typedef struct _FSD_FCB
{

   // FCB header required by NT
   FSRTL_COMMON_FCB_HEADER CommonFCBHeader;
   SECTION_OBJECT_POINTERS SectionObject;
   ERESOURCE MainResource;
   ERESOURCE PagingIoResource;
   // end FCB header required by NT

   // Identifier for this structure
   FSD_IDENTIFIER Identifier;

   // List of FCBs for this volume
   LIST_ENTRY Next;

#ifndef FSD_RO
   // Share Access for the file object
   SHARE_ACCESS ShareAccess;
#endif

   // List of byte-range locks for this file
   FILE_LOCK FileLock;

   // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
   ULONG OpenHandleCount;

   // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
   ULONG ReferenceCount;

   // The filename
   UNICODE_STRING FileName;
   ANSI_STRING AnsiFileName;

   // The file attributes
   ULONG FileAttributes;

   // The inode number
   ULONG IndexNumber;

   // Flags for the FCB
   ULONG Flags;

   // Pointer to the inode
   inode_t *inode;

   // List of CCBs for this FCB
   LIST_ENTRY FirstCCB;

} FSD_FCB, *PFSD_FCB;

//
// Flags for FSD_FCB
//
#define FCB_PAGE_FILE               0x00000001
#define FCB_DELETE_PENDING          0x00000002

//
// FSD_CCB Context Control Block
//
// Data that represents one instance of an open file
// There is one instance of the CCB for every instance of an open file
//
typedef struct _FSD_CCB
{

   // Identifier for this structure
   FSD_IDENTIFIER Identifier;

   // State that may need to be maintained
   ULONG FileIndex;


   UNICODE_STRING DirectorySearchPattern;

   // Pointer to the next CCB
   LIST_ENTRY NextCCB;

   // Related FCB
   PFSD_FCB PtrFCB;

} FSD_CCB, *PFSD_CCB;

/*!
 * \brief Irp context struct
 *
 * Used to pass information about a request between the drivers functions.
 */
typedef struct _FSD_IRP_CONTEXT
{

   // Identifier for this structure
   FSD_IDENTIFIER Identifier;

   /*! \brief Pointer to the IRP this request describes
    */
   PIRP Irp;

   // The major and minor function code for the request
   UCHAR MajorFunction;          //!< \brief Major request
   UCHAR MinorFunction;          //!< \brief Minor request

   // The device object
   PDEVICE_OBJECT DeviceObject;  //!< \brief Pointer to the target device object

   // The file object
   PFILE_OBJECT FileObject;

   // If the request is synchronous (we are allowed to block)
   BOOLEAN IsSynchronous;

   // If the request is top level
   BOOLEAN IsTopLevel;

   // Used if the request needs to be queued for later processing
   WORK_QUEUE_ITEM WorkQueueItem;

   // If an exception is currently in progress
   BOOLEAN ExceptionInProgress;

   // The exception code when an exception is in progress
   NTSTATUS ExceptionCode;

} FSD_IRP_CONTEXT, *PFSD_IRP_CONTEXT;

//
// FSD_ALLOC_HEADER
//
// In the checked version of the driver this header is put in the beginning of
// every memory allocation
//
typedef struct _FSD_ALLOC_HEADER
{
   FSD_IDENTIFIER Identifier;
} FSD_ALLOC_HEADER, *PFSD_ALLOC_HEADER;

//
// Function prototypes from alloc.c
//

#if DBG

PVOID Ext2FsdAllocatePool (IN POOL_TYPE PoolType, IN ULONG NumberOfBytes,
                           IN ULONG Tag);

VOID Ext2FsdFreePool (IN PVOID p);

#else // !DBG

#define Ext2FsdAllocatePool(PoolType, NumberOfBytes, Tag) \
        ExAllocatePool(PoolType, NumberOfBytes)

#define Ext2FsdFreePool(p) \
        ExFreePool(p)

#endif // !DBG

PFSD_IRP_CONTEXT Ext2FsdAllocateIrpContext (IN PDEVICE_OBJECT DeviceObject,
                                            IN PIRP Irp);

VOID Ext2FsdFreeIrpContext (IN PFSD_IRP_CONTEXT IrpContext);

PFSD_FCB Ext2FsdAllocateFcb (IN PFSD_VCB Vcb, IN PUNICODE_STRING FileName,
                             IN ULONG IndexNumber, IN inode_t * inode);

VOID Ext2FsdFreeFcb (IN PFSD_FCB Fcb);

PFSD_CCB Ext2FsdAllocateCcb (VOID);

VOID Ext2FsdFreeCcb (IN PFSD_CCB Ccb);

VOID Ext2FsdFreeVcb (IN PFSD_VCB Vcb);

//
// Function prototypes from blockdev.c
//

NTSTATUS Ext2ReadSync (IN PDEVICE_OBJECT DeviceObject, IN LONGLONG Offset,
                       IN ULONG Length, OUT PVOID Buffer, BOOLEAN bVerify);

NTSTATUS FsdBlockDeviceIoControl (IN PDEVICE_OBJECT DeviceObject,
                                  IN ULONG IoctlCode, IN PVOID InputBuffer,
                                  IN ULONG InputBufferSize,
                                  IN OUT PVOID OutputBuffer,
                                  IN OUT PULONG OutputBufferSize);

NTSTATUS FsdReadWriteBlockDevice (IN ULONG Operation,
                                  IN PDEVICE_OBJECT DeviceObject,
                                  IN PLARGE_INTEGER Offset, IN ULONG Length,
                                  IN BOOLEAN OverrideVerify,
                                  IN OUT PVOID Buffer);

NTSTATUS FsdReadWriteBlockDeviceCompletion (IN PDEVICE_OBJECT DeviceObject,
                                            IN PIRP Irp, IN PVOID Context);

//
// Function prototypes from char.c
//

VOID FsdCharToWchar (IN OUT PWCHAR Destination, IN PCHAR Source,
                     IN ULONG Length);

NTSTATUS FsdWcharToChar (IN OUT PCHAR Destination, IN PWCHAR Source,
                         IN ULONG Length);

//
// Function prototypes from cleanup.c
//

NTSTATUS Ext2FsdCleanup (IN PFSD_IRP_CONTEXT IrpContext);

//
// Function prototypes from close.c
//

NTSTATUS Ext2FsdClose (IN PFSD_IRP_CONTEXT IrpContext);

VOID Ext2FsdQueueCloseRequest (IN PFSD_IRP_CONTEXT IrpContext);

VOID Ext2FsdDeQueueCloseRequest (IN PVOID Context);


//
// Function prototypes from create.c
//

NTSTATUS Ext2FsdCreate (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdCreateFs (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdCreateVolume (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdCreateFile (IN PFSD_IRP_CONTEXT IrpContext);

PFSD_FCB FsdLookupFcbByFileName (IN PFSD_VCB Vcb,
                                 IN PUNICODE_STRING FullFileName);

//
// Function prototypes from debug.c
//

#if DBG

extern ULONG ProcessNameOffset;

#define FsdGetCurrentProcessName() ( \
    (PUCHAR) PsGetCurrentProcess() + ProcessNameOffset \
)

ULONG FsdGetProcessNameOffset (VOID);

VOID Ext2FsdDbgPrintCall (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

VOID FsdDbgPrintComplete (IN PIRP Irp);

PUCHAR FsdNtStatusToString (IN NTSTATUS Status);

#define FsdCompleteRequest(Irp, PriorityBoost) \
        FsdDbgPrintComplete(Irp); \
        IoCompleteRequest(Irp, PriorityBoost)

#else // !DBG

#define FsdDbgPrintCall(DeviceObject, Irp)

#define FsdCompleteRequest(Irp, PriorityBoost) \
        IoCompleteRequest(Irp, PriorityBoost)

#endif // !DBG

//
// Function prototypes from devctl.c
//

NTSTATUS Ext2FsdDeviceControl (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdPrepareToUnload (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdDeviceControlNormal (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdDeviceControlCompletion (IN PDEVICE_OBJECT DeviceObject,
                                         IN PIRP Irp, IN PVOID Context);

//
// Function prototypes from dirctl.c
//

NTSTATUS Ext2FsdDirectoryControl (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdQueryDirectory (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdNotifyChangeDirectory (IN PFSD_IRP_CONTEXT IrpContext);

//
// Function prototypes from fileinfo.c
//

NTSTATUS Ext2FsdQueryInformation (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdSetInformation (IN PFSD_IRP_CONTEXT IrpContext);

//
// Function prototypes from fsctl.c
//

NTSTATUS Ext2FsdFileSystemControl (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdUserFsRequest (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdLockVolume (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdUnlockVolume (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdDismountVolume (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdIsVolumeMounted (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdMountVolume (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdVerifyVolume (IN PFSD_IRP_CONTEXT IrpContext);

VOID FsdPurgeVolume (IN PFSD_VCB Vcb, IN BOOLEAN FlushBeforePurge);

VOID FsdPurgeFile (IN PFSD_FCB Fcb, IN BOOLEAN FlushBeforePurge);

VOID FsdSetVpbFlag (IN PVPB Vpb, IN USHORT Flag);

VOID FsdClearVpbFlag (IN PVPB Vpb, IN USHORT Flag);

//
// Function prototypes from fsd.c
//

NTSTATUS Ext2FsdBuildRequest (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

NTSTATUS FsdQueueRequest (IN PFSD_IRP_CONTEXT IrpContext);

VOID FsdDeQueueRequest (IN PVOID Context);

NTSTATUS FsdDispatchRequest (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS FsdExceptionFilter (IN PFSD_IRP_CONTEXT IrpContext,
                             IN NTSTATUS ExceptionCode);

NTSTATUS FsdExceptionHandler (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS FsdLockUserBuffer (IN PIRP Irp, IN ULONG Length,
                            IN LOCK_OPERATION Operation);

PVOID FsdGetUserBuffer (IN PIRP Irp);

//
// Function prototypes from init.c
//

NTSTATUS DriverEntry (IN PDRIVER_OBJECT DriverObject,
                      IN PUNICODE_STRING RegistryPath);

VOID DriverUnload (IN PDRIVER_OBJECT DriverObject);

//
// Function prototypes from inode.c
//
NTSTATUS Ext2FsdCreateInode (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                             IN OUT PFSD_FCB pParentFcb, IN ULONG found_index,
                             IN ULONG Type, IN ULONG FileAttr,
                             IN PUNICODE_STRING FileName);

BOOLEAN Ext2FsdNewInode (PFSD_IRP_CONTEXT IrpContext, PFSD_VCB Vcb,
                         ULONG GroupHint, ULONG Type, PULONG Inode);

BOOLEAN Ext2FsdDeleteInode (PFSD_IRP_CONTEXT IrpContext, PFSD_VCB Vcb,
                            ULONG Inode, ULONG Type);

NTSTATUS Ext2FsdAddEntry (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                          IN PFSD_FCB Dcb, IN ULONG FileType, IN ULONG Inode,
                          IN PUNICODE_STRING FileName);

NTSTATUS Ext2FsdRemoveEntry (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                             IN PFSD_FCB Dcb, IN ULONG Inode);

BOOLEAN Ext2FsdSaveGroup (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb);

BOOLEAN Ext2FsdSaveInode (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                          IN ULONG Inode, IN inode_t * inode);

BOOLEAN Ext2FsdSaveSuper (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb);

BOOLEAN Ext2FsdGetInodeLba (IN PFSD_VCB vcb, IN ULONG inode,
                            OUT PLONGLONG offset);

//
// Function prototypes from lockctl.c
//

NTSTATUS Ext2FsdLockControl (IN PFSD_IRP_CONTEXT IrpContext);

//
// Function prototypes from read.c
//

NTSTATUS Ext2FsdRead (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdReadNormal (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS Ext2FsdReadComplete (IN PFSD_IRP_CONTEXT IrpContext);

//
// Function prototypes from volinfo.c
//

NTSTATUS Ext2FsdQueryVolumeInformation (IN PFSD_IRP_CONTEXT IrpContext);

// 
// Function prototypes from write.c
// 
NTSTATUS FsdWrite (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS FsdWriteNormal (IN PFSD_IRP_CONTEXT IrpContext);

NTSTATUS FsdWriteComplete (IN PFSD_IRP_CONTEXT IrpContext);


//
// These declarations is missing in some versions of the DDK and ntifs.h
//

#ifndef IoCopyCurrentIrpStackLocationToNext
#define IoCopyCurrentIrpStackLocationToNext( Irp ) { \
    PIO_STACK_LOCATION irpSp; \
    PIO_STACK_LOCATION nextIrpSp; \
    irpSp = IoGetCurrentIrpStackLocation( (Irp) ); \
    nextIrpSp = IoGetNextIrpStackLocation( (Irp) ); \
    RtlCopyMemory( \
        nextIrpSp, \
        irpSp, \
        FIELD_OFFSET(IO_STACK_LOCATION, CompletionRoutine) \
        ); \
    nextIrpSp->Control = 0; }
#endif

#ifndef IoSkipCurrentIrpStackLocation
#define IoSkipCurrentIrpStackLocation( Irp ) \
    (Irp)->CurrentLocation++; \
    (Irp)->Tail.Overlay.CurrentStackLocation++;
#endif

NTKERNELAPI VOID FsRtlNotifyChangeDirectory (IN PNOTIFY_SYNC NotifySync,
                                             IN PVOID FsContext,
                                             IN PSTRING FullDirectoryName,
                                             IN PLIST_ENTRY NotifyList,
                                             IN BOOLEAN WatchTree,
                                             IN ULONG CompletionFilter,
                                             IN PIRP NotifyIrp);

#ifndef RtlUshortByteSwap
USHORT FASTCALL RtlUshortByteSwap (IN USHORT Source);
#endif

#ifndef RtlUlongByteSwap
ULONG FASTCALL RtlUlongByteSwap (IN ULONG Source);
#endif

#ifndef RtlUlonglongByteSwap
ULONGLONG FASTCALL RtlUlonglongByteSwap (IN ULONGLONG Source);
#endif

#endif
