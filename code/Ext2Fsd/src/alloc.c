/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/07/02 10:48:51 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/alloc.c,v $
    $Revision: 1.8 $
*/

/*!\file alloc.c
 * \brief Allocates and frees memory for structures.
 * 
 * This file contains all allocate and free functions to manage the memory
 * used by structures.
 */

#include "ext2.h"
#include "io.h"
#include "ntifs.h"
#include "fsd.h"

#if DBG

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdAllocatePool)
#pragma alloc_text(PAGE, Ext2FsdFreePool)
#pragma alloc_text(PAGE, Ext2FsdAllocateIrpContext)
#pragma alloc_text(PAGE, Ext2FsdFreeIrpContext)
#pragma alloc_text(PAGE, Ext2FsdAllocateFcb)
#pragma alloc_text(PAGE, Ext2FsdFreeFcb)
#pragma alloc_text(PAGE, Ext2FsdAllocateCcb)
#pragma alloc_text(PAGE, Ext2FsdFreeCcb)
#pragma alloc_text(PAGE, Ext2FsdFreeVcb)
#pragma alloc_text(PAGE, Ext2FsdAllocateInode)
#pragma alloc_text(PAGE, Ext2FsdFreeInode)
#endif

PVOID
Ext2FsdAllocatePool (IN POOL_TYPE PoolType, IN ULONG NumberOfBytes,
                     IN ULONG Tag)
{
   PVOID p;
   PFSD_ALLOC_HEADER AllocHeader;

   NumberOfBytes += sizeof (FSD_ALLOC_HEADER);

   p = ExAllocatePoolWithTag (PoolType, NumberOfBytes, Tag);

   ASSERT (p);

   if (p)
   {
      RtlFillMemory (p, NumberOfBytes, '0');

      AllocHeader = (PFSD_ALLOC_HEADER) p;

      AllocHeader->Identifier.Type = FSD;
      AllocHeader->Identifier.Size = NumberOfBytes;

      return (PVOID) ((PUCHAR) p + sizeof (FSD_ALLOC_HEADER));
   }
   else
   {
      return NULL;
   }
}

VOID
Ext2FsdFreePool (IN PVOID p)
{
   PFSD_ALLOC_HEADER AllocHeader;

   ASSERT (p != NULL);

   p = (PVOID) ((PUCHAR) p - sizeof (FSD_ALLOC_HEADER));

   AllocHeader = (PFSD_ALLOC_HEADER) p;

   ASSERT (AllocHeader->Identifier.Type == FSD);

   RtlFillMemory (p, AllocHeader->Identifier.Size, 'X');

   ExFreePool (p);
}

#endif // DBG

PFSD_IRP_CONTEXT
Ext2FsdAllocateIrpContext (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
   PIO_STACK_LOCATION IrpSp;
   PFSD_IRP_CONTEXT IrpContext;

   ASSERT (DeviceObject != NULL);
   ASSERT (Irp != NULL);

   IrpSp = IoGetCurrentIrpStackLocation (Irp);

   IrpContext =
      Ext2FsdAllocatePool (NonPagedPool, sizeof (FSD_IRP_CONTEXT), 'xcIR');

   if (!IrpContext)
   {
      return NULL;
   }

   IrpContext->Identifier.Type = ICX;
   IrpContext->Identifier.Size = sizeof (FSD_IRP_CONTEXT);

   IrpContext->Irp = Irp;

   IrpContext->MajorFunction = IrpSp->MajorFunction;
   IrpContext->MinorFunction = IrpSp->MinorFunction;

   IrpContext->DeviceObject = DeviceObject;

   IrpContext->FileObject = IrpSp->FileObject;

   if (IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL
       || IrpContext->MajorFunction == IRP_MJ_DEVICE_CONTROL
       || IrpContext->MajorFunction == IRP_MJ_SHUTDOWN)
   {
      IrpContext->IsSynchronous = TRUE;
   }
   else if (IrpContext->MajorFunction == IRP_MJ_CLEANUP
            || IrpContext->MajorFunction == IRP_MJ_CLOSE)
   {
      IrpContext->IsSynchronous = FALSE;
   }
   else
   {
      IrpContext->IsSynchronous = IoIsOperationSynchronous (Irp);
   }


   //
   // Temporary workaround for a bug in close that makes it reference a
   // fileobject when it is no longer valid.
   //
   if (IrpContext->MajorFunction == IRP_MJ_CLOSE)
   {
      IrpContext->IsSynchronous = TRUE;
   }

   IrpContext->IsTopLevel = (IoGetTopLevelIrp () == Irp);

   IrpContext->ExceptionInProgress = FALSE;

   return IrpContext;
}

VOID
Ext2FsdFreeIrpContext (IN PFSD_IRP_CONTEXT IrpContext)
{
   ASSERT (IrpContext != NULL);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   Ext2FsdFreePool (IrpContext);
}

PFSD_FCB
Ext2FsdAllocateFcb (IN PFSD_VCB Vcb, IN PUNICODE_STRING FileName,
                    IN ULONG IndexNumber, IN inode_t * inode)
{
   PFSD_FCB Fcb;

   Fcb = Ext2FsdAllocatePool (NonPagedPool, sizeof (FSD_FCB), 'bcFR');

   if (!Fcb)
   {
      return NULL;
   }

   Fcb->Identifier.Type = FCB;
   Fcb->Identifier.Size = sizeof (FSD_FCB);

#ifndef FSD_RO
   RtlZeroMemory (&Fcb->ShareAccess, sizeof (SHARE_ACCESS));
#endif

   FsRtlInitializeFileLock (&Fcb->FileLock, NULL, NULL);

   Fcb->OpenHandleCount = 0;
   Fcb->ReferenceCount = 0;

   Fcb->FileName.Length = 0;

   Fcb->FileName.MaximumLength = FileName->Length;

   Fcb->FileName.Buffer =
      (PWSTR) Ext2FsdAllocatePool (NonPagedPool, Fcb->FileName.MaximumLength,
                                   '1cFR');

   if (!Fcb->FileName.Buffer)
   {
      Ext2FsdFreePool (Fcb);
      return NULL;
   }

   RtlCopyUnicodeString (&Fcb->FileName, FileName);

   Fcb->AnsiFileName.Length = Fcb->FileName.Length / sizeof (WCHAR);

   Fcb->AnsiFileName.MaximumLength = Fcb->FileName.Length / sizeof (WCHAR) + 1;

   Fcb->AnsiFileName.Buffer =
      (PUCHAR) Ext2FsdAllocatePool (NonPagedPool,
                                    Fcb->FileName.Length / sizeof (WCHAR) + 1,
                                    '2cFR');

   if (!Fcb->AnsiFileName.Buffer)
   {
      Ext2FsdFreePool (Fcb->FileName.Buffer);
      Ext2FsdFreePool (Fcb);
      return NULL;
   }

   FsdWcharToChar (Fcb->AnsiFileName.Buffer, Fcb->FileName.Buffer,
                   Fcb->FileName.Length / sizeof (WCHAR));

   Fcb->AnsiFileName.Buffer[Fcb->FileName.Length / sizeof (WCHAR)] = 0;

   Fcb->FileAttributes = FILE_ATTRIBUTE_NORMAL;

   if (S_ISDIR (inode->i_mode))
   {
      SetFlag (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY);
   }

   SetFlag (Fcb->FileAttributes, FILE_ATTRIBUTE_READONLY);

   Fcb->IndexNumber = IndexNumber;

   Fcb->Flags = 0;

   Fcb->inode = inode;

   RtlZeroMemory (&Fcb->CommonFCBHeader, sizeof (FSRTL_COMMON_FCB_HEADER));

   Fcb->CommonFCBHeader.NodeTypeCode = (USHORT) FCB;
   Fcb->CommonFCBHeader.NodeByteSize = sizeof (FSD_FCB);
   Fcb->CommonFCBHeader.IsFastIoPossible = FastIoIsNotPossible;
   Fcb->CommonFCBHeader.Resource = &(Fcb->MainResource);
   Fcb->CommonFCBHeader.PagingIoResource = &(Fcb->PagingIoResource);
   Fcb->CommonFCBHeader.AllocationSize.QuadPart = inode->i_size;
   Fcb->CommonFCBHeader.FileSize.QuadPart = inode->i_size;
   Fcb->CommonFCBHeader.ValidDataLength.QuadPart = inode->i_size;

   Fcb->SectionObject.DataSectionObject = NULL;
   Fcb->SectionObject.SharedCacheMap = NULL;
   Fcb->SectionObject.ImageSectionObject = NULL;

   ExInitializeResourceLite (&(Fcb->MainResource));
   ExInitializeResourceLite (&(Fcb->PagingIoResource));

   InsertTailList (&Vcb->FcbList, &Fcb->Next);

   return Fcb;
}

VOID
Ext2FsdFreeFcb (IN PFSD_FCB Fcb)
{
   ASSERT (Fcb != NULL);

   ASSERT ((Fcb->Identifier.Type == FCB)
           && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

   FsRtlUninitializeFileLock (&Fcb->FileLock);

   ExDeleteResourceLite (&Fcb->MainResource);

   ExDeleteResourceLite (&Fcb->PagingIoResource);

   RemoveEntryList (&Fcb->Next);

   Ext2FsdFreePool (Fcb->FileName.Buffer);

   Ext2FsdFreePool (Fcb->AnsiFileName.Buffer);

   Ext2FsdFreePool (Fcb->inode);

   Ext2FsdFreePool (Fcb);
}

PFSD_CCB
Ext2FsdAllocateCcb (VOID)
{
   PFSD_CCB Ccb;

   Ccb = Ext2FsdAllocatePool (NonPagedPool, sizeof (FSD_CCB), 'bcCR');

   if (!Ccb)
   {
      return NULL;
   }

   Ccb->Identifier.Type = CCB;
   Ccb->Identifier.Size = sizeof (FSD_CCB);
   Ccb->FileIndex = 0;

   Ccb->DirectorySearchPattern.Length = 0;
   Ccb->DirectorySearchPattern.MaximumLength = 0;
   Ccb->DirectorySearchPattern.Buffer = 0;

   Ccb->PtrFCB = 0;

   return Ccb;
}

VOID
Ext2FsdFreeCcb (IN PFSD_CCB Ccb)
{
   ASSERT (Ccb != NULL);

   ASSERT ((Ccb->Identifier.Type == CCB)
           && (Ccb->Identifier.Size == sizeof (FSD_CCB)));


   if (Ccb->DirectorySearchPattern.Buffer != NULL)
   {
      Ext2FsdFreePool (Ccb->DirectorySearchPattern.Buffer);
   }

   Ext2FsdFreePool (Ccb);
}

VOID
Ext2FsdFreeVcb (IN PFSD_VCB Vcb)
{
   ASSERT (Vcb != NULL);

   ASSERT ((Vcb->Identifier.Type == VCB)
           && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

   FsdClearVpbFlag (Vcb->Vpb, VPB_MOUNTED);

   FsRtlNotifyUninitializeSync (&Vcb->NotifySync);

   ExAcquireResourceExclusiveLite (&FsdGlobalData.Resource, TRUE);

   RemoveEntryList (&Vcb->Next);

   ExReleaseResourceForThreadLite (&FsdGlobalData.Resource,
                                   ExGetCurrentResourceThread ());

   ExDeleteResourceLite (&Vcb->MainResource);

   ExDeleteResourceLite (&Vcb->PagingIoResource);

   Ext2FsdFreePool (Vcb->ext2);

   IoDeleteDevice (Vcb->DeviceObject);

#ifdef TRACE
   KdPrint ((DRIVER_NAME ": Vcb deallocated\n"));
#endif
}


/*!
 * \brief Allocates memory for inode_t structure from paged pool.
 */
inode_t *
Ext2FsdAllocateInode ()
{
   inode_t *inode = (inode_t *) MALLOC (sizeof (inode_t));

   ASSERT (inode);
   return inode;
}

/*!
 * \brief Frees memory for inode_t structure.
 */
void
Ext2FsdFreeInode (inode_t * inode)
{
   FREE (inode);
}
