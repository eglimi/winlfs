/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/07/02 15:19:07 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/create.c,v $
    $Revision: 1.15 $
*/

/*!\file create.c
 * \brief Functions to handle IRP_MJ_CREATE
 * 
 * The I/O Manager sends this request when a new file or directory is being created,
 * or when an existing file, device, directory, or volume is being opened. Normally 
 * this IRP is sent on behalf of a user-mode application that has called a function
 * such as CreateFile or a kernel-mode component that has called IoCreateFile,
 * ZwCreateFile, or ZwOpenFile.
 */

#include "ntifs.h"
#include "ext2.h"
#include "fsd.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdCreate)
#pragma alloc_text(PAGE, Ext2FsdCreateFs)
#pragma alloc_text(PAGE, Ext2FsdCreateVolume)
#pragma alloc_text(PAGE, Ext2FsdCreateFile)
#pragma alloc_text(PAGE, FsdLookupFcbByFileName)
#endif

NTSTATUS
Ext2FsdCreate (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   PIRP Irp;
   PIO_STACK_LOCATION IrpSp;

   DeviceObject = IrpContext->DeviceObject;

   Irp = IrpContext->Irp;

   IrpSp = IoGetCurrentIrpStackLocation (Irp);

#ifdef TRACE
   DbgPrint ("ExtFsd: IRP_MJ_CREATE -> Minor: %S\n", IrpContext->MinorFunction);
#endif


   if (DeviceObject == FsdGlobalData.DeviceObject)
   {
      return Ext2FsdCreateFs (IrpContext);
   }
   else if (IrpSp->FileObject->FileName.Length == 0)
   {
      return Ext2FsdCreateVolume (IrpContext);
   }
   else
   {
      return Ext2FsdCreateFile (IrpContext);
   }
}

NTSTATUS
Ext2FsdCreateFs (IN PFSD_IRP_CONTEXT IrpContext)
{
   IrpContext->Irp->IoStatus.Status = STATUS_SUCCESS;

   IrpContext->Irp->IoStatus.Information = FILE_OPENED;

   FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);

   Ext2FsdFreeIrpContext (IrpContext);

   return STATUS_SUCCESS;
}

NTSTATUS
Ext2FsdCreateVolume (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   PFILE_OBJECT FileObject;
   PFSD_VCB Vcb;

   DeviceObject = IrpContext->DeviceObject;

   Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

   ASSERT (Vcb != NULL);

   ASSERT ((Vcb->Identifier.Type == VCB)
           && (Vcb->Identifier.Size == sizeof (FSD_VCB)));

   FileObject = IrpContext->FileObject;

   FileObject->FsContext = Vcb;

   ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

   Vcb->ReferenceCount++;

   ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                   ExGetCurrentResourceThread ());

   IrpContext->Irp->IoStatus.Status = STATUS_SUCCESS;

   IrpContext->Irp->IoStatus.Information = FILE_OPENED;

   FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);

   Ext2FsdFreeIrpContext (IrpContext);

   return STATUS_SUCCESS;
}

/*!\brief Creates the structures in the memory representing a physical file.
 *
 * If a file already exists on the disk, this function creates all needed structure
 * to represent the file in memory.
 *
 * If the file does not yet exists, it looks for free inodes on disk and creates
 * the directory entries and structures for the new allocated inodes.
 *
 * @param  IrpContext Pointer to the context of the IRP
 * @return Status if all allocations were successful.
 */
NTSTATUS
Ext2FsdCreateFile (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb = NULL;
   PFSD_FCB Fcb = NULL;
   PFSD_CCB Ccb = NULL;
   _u32 found_index = EXT2_INVALID_INODE;
   inode_t *Inode = NULL;
   inode_t *pTmpInode = NULL;
   BOOLEAN VcbResourceAcquired = FALSE;
   PUCHAR buffer = NULL;
   PFILE_OBJECT PtrNewFileObject = NULL;
   PFILE_OBJECT PtrRelatedFileObject = NULL;
   PFSD_FCB pParentFcb = NULL;
   UNICODE_STRING TargetObjectName;
   UNICODE_STRING RelatedObjectName;
   UNICODE_STRING AbsolutePathName;
   PFSD_CCB PtrRelatedCCB = NULL;       //, PtrNewCCB = NULL;
   PFSD_FCB PtrRelatedFCB = NULL;       //, PtrNewFCB = NULL;
   UNICODE_STRING *PtrFileName = NULL;

   ULONG Options;
   ULONG CreateDisposition;

   BOOLEAN OpenDirectory;
   BOOLEAN OpenTargetDirectory;
   BOOLEAN CreateDirectory;
   BOOLEAN SequentialOnly;
   BOOLEAN NoIntermediateBuffering;
   BOOLEAN IsPagingFile;
   BOOLEAN DirectoryFile;
   BOOLEAN NonDirectoryFile;
   BOOLEAN NoEaKnowledge;
   BOOLEAN DeleteOnClose;
   BOOLEAN TemporaryFile;
   BOOLEAN CaseSensitive;


   AbsolutePathName.Buffer = NULL;
   AbsolutePathName.Length = AbsolutePathName.MaximumLength = 0;

   DeviceObject = IrpContext->DeviceObject;

   Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

   Irp = IrpContext->Irp;

   IrpSp = IoGetCurrentIrpStackLocation (Irp);

   ASSERT (DeviceObject);
   ASSERT (Vcb);
   ASSERT (Irp);
   ASSERT (IrpSp);

   Options = IrpSp->Parameters.Create.Options;
   DirectoryFile = IsFlagOn (Options, FILE_DIRECTORY_FILE);
   OpenTargetDirectory = IsFlagOn (IrpSp->Flags, SL_OPEN_TARGET_DIRECTORY);

   NonDirectoryFile = IsFlagOn (Options, FILE_NON_DIRECTORY_FILE);
   SequentialOnly = IsFlagOn (Options, FILE_SEQUENTIAL_ONLY);
   NoIntermediateBuffering = IsFlagOn (Options, FILE_NO_INTERMEDIATE_BUFFERING);
   NoEaKnowledge = IsFlagOn (Options, FILE_NO_EA_KNOWLEDGE);
   DeleteOnClose = IsFlagOn (Options, FILE_DELETE_ON_CLOSE);

   CaseSensitive = IsFlagOn (IrpSp->Flags, SL_CASE_SENSITIVE);

   TemporaryFile =
      IsFlagOn (IrpSp->Parameters.Create.FileAttributes,
                FILE_ATTRIBUTE_TEMPORARY);

   CreateDisposition = (Options >> 24) & 0x000000ff;

   IsPagingFile = IsFlagOn (IrpSp->Flags, SL_OPEN_PAGING_FILE);

   CreateDirectory = (BOOLEAN) (DirectoryFile
                                && ((CreateDisposition == FILE_CREATE)
                                    || (CreateDisposition == FILE_OPEN_IF)));

   OpenDirectory = (BOOLEAN) (DirectoryFile
                              && ((CreateDisposition == FILE_OPEN)
                                  || (CreateDisposition == FILE_OPEN_IF)));

   //KdPrint(("CreateDirectory: %s", (CreateDirectory ? "TRUE" : "FALSE") ));

   __try
   {

      PtrNewFileObject = IrpSp->FileObject;
      TargetObjectName = PtrNewFileObject->FileName;
      PtrRelatedFileObject = PtrNewFileObject->RelatedFileObject;
      if (PtrRelatedFileObject)
      {
         pParentFcb = (PFSD_FCB) (PtrRelatedFileObject->FsContext);
      }


      // If a related file object is present, get the pointers
      //      to the CCB and the FCB for the related file object
      if (PtrRelatedFileObject)
      {
#ifdef TRACE
         DbgPrint ("RelatedFileObject detected (%s,%d)", __FILE__, __LINE__);
#endif
         PtrRelatedCCB = (PFSD_CCB) (PtrRelatedFileObject->FsContext2);
         ASSERT (PtrRelatedCCB);
         ASSERT (PtrRelatedCCB->Identifier.Type == CCB);
         // each CCB in turn points to a FCB
         PtrRelatedFCB = PtrRelatedCCB->PtrFCB;
         ASSERT (PtrRelatedFCB);
         ASSERT ((PtrRelatedFCB->Identifier.Type == FCB)
                 || (PtrRelatedFCB->Identifier.Type == VCB));
         RelatedObjectName = PtrRelatedFileObject->FileName;

         // Now determine the starting point from which to begin the parsing
         // We have a user supplied related file object.
         //      This implies a "relative" open i.e. relative to the directory
         //      represented by the related file object ...

         //      Note: The only purpose FSD implementations ever have for
         //      the related file object is to determine whether this
         //      is a relative open or not. At all other times (including
         //      during I/O operations), this field is meaningless from
         //      the FSD's perspective.                  
         if (!(S_ISDIR (PtrRelatedFCB->inode->i_mode)))
         {
            // we must have a directory as the "related" object
            Status = STATUS_INVALID_PARAMETER;
            __leave;
         }

         // So we have a directory, ensure that the name begins with
         //      a "\" i.e. begins at the root and does *not* begin with a "\\"
         //      NOTE: This is just an example of the kind of path-name string
         //      validation that a FSD must do. Although the remainder of
         //      the code may not include such checks, any commercial
         //      FSD *must* include such checking (no one else, including
         //      the I/O Manager will perform checks on your FSD's behalf)
         if ((RelatedObjectName.Length == 0)
             || (RelatedObjectName.Buffer[0] != L'\\'))
         {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
         }

         // similarly, if the target file name starts with a "\", it
         //      is wrong since the target file name can no longer be absolute
         if ((TargetObjectName.Length != 0)
             && (TargetObjectName.Buffer[0] == L'\\'))
         {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
         }

         // Create an absolute path-name. You could potentially use
         //      the absolute path-name if you cache previously opened
         //      file/directory object names.
         {
            AbsolutePathName.MaximumLength =
               TargetObjectName.Length + RelatedObjectName.Length +
               sizeof (WCHAR);
            if (!
                (AbsolutePathName.Buffer =
                 ExAllocatePool (PagedPool, AbsolutePathName.MaximumLength)))
            {
               Status = STATUS_INSUFFICIENT_RESOURCES;
               __leave;
            }

            RtlZeroMemory (AbsolutePathName.Buffer,
                           AbsolutePathName.MaximumLength);

            RtlCopyMemory ((void *) (AbsolutePathName.Buffer),
                           (void *) (RelatedObjectName.Buffer),
                           RelatedObjectName.Length);
            AbsolutePathName.Length = RelatedObjectName.Length;
            RtlAppendUnicodeToString (&AbsolutePathName, L"\\");
            RtlAppendUnicodeToString (&AbsolutePathName,
                                      TargetObjectName.Buffer);
            /*
               buffer = (PUCHAR)Ext2FsdAllocatePool(NonPagedPool,AbsolutePathName.Length / sizeof(WCHAR) + 1,'2cFR');

               if (buffer == NULL)
               {
               DbgPrint("ERROR CREATE : buffer == NULL");
               Status = STATUS_INSUFFICIENT_RESOURCES;
               __leave;
               }
               FsdWcharToChar(buffer, AbsolutePathName.Buffer, AbsolutePathName.Length / sizeof(WCHAR));
               buffer[AbsolutePathName.Length / sizeof(WCHAR)] = 0;
               DbgPrint("Absolute path with related name = %s", buffer);

               Ext2FsdFreePool(buffer);

               // */
            PtrFileName = &AbsolutePathName;
         }
      }
      else
      {
         PtrFileName = &IrpSp->FileObject->FileName;
      }


      ExAcquireResourceExclusiveLite (&Vcb->MainResource, TRUE);

      VcbResourceAcquired = TRUE;

      Fcb = FsdLookupFcbByFileName (Vcb, PtrFileName);

      if (!Fcb)
      {
         Inode = Ext2FsdAllocatePool (NonPagedPool, sizeof (inode_t), '3cFR');

         if (Inode == NULL)
         {
            DbgPrint ("ERROR CREATE : Inode == NULL");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
         }

         DbgPrint ("Ext2FsdCreateFile : length=%i file=%.*S\n",
                   PtrFileName->Length, PtrFileName->Buffer,
                   PtrFileName->Length / sizeof (WCHAR));

         buffer =
            (PUCHAR) Ext2FsdAllocatePool (NonPagedPool,
                                          PtrFileName->Length / sizeof (WCHAR) +
                                          1, '2cFR');

         if (buffer == NULL)
         {
            DbgPrint ("ERROR CREATE : buffer == NULL");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
         }

         if (FsRtlDoesNameContainWildCards (PtrFileName))
         {
            Status = STATUS_OBJECT_NAME_INVALID;
            __leave;
         }


         FsdWcharToChar (buffer, PtrFileName->Buffer,
                         PtrFileName->Length / sizeof (WCHAR));

         buffer[PtrFileName->Length / sizeof (WCHAR)] = 0;

         found_index = ext2_lookup (Vcb->ext2, buffer, EXT2_ROOT_INODE);

         if (ext2_inode_valid (Vcb->ext2, found_index))
         {
            if (ext2_load_inode (Vcb->ext2, Inode, found_index) ==
                EXT2_STATUS_OK)
            {
#ifdef TRACE
               DbgPrint ("Ext2FsdCreateFile : Inode found index=%i\n",
                         found_index);
#endif
               Status = STATUS_SUCCESS;
            }
         }

         if (!NT_SUCCESS (Status))
         {
            UNICODE_STRING NewName;

            NewName = *PtrFileName;

#ifdef TRACE
            KdPrint ((DRIVER_NAME ": eusä Teil :)"));
#endif

            // We need to create a new one ?
            if ((CreateDisposition == FILE_CREATE)
                || (CreateDisposition == FILE_OPEN_IF)
                || (CreateDisposition == FILE_OVERWRITE_IF))
            {
               if (IsFlagOn (Vcb->Flags, VCB_READ_ONLY))
               {
                  Status = STATUS_MEDIA_WRITE_PROTECTED;
                  __leave;
               }

               if (DirectoryFile)
               {
                  if (TemporaryFile)
                  {
                     Status = STATUS_INVALID_PARAMETER;
                     __leave;
                  }
               }

               if (!pParentFcb)
               {
                  if (DirectoryFile)
                  {
                     while (NewName.Length > 0
                            && NewName.Buffer[NewName.Length / 2 - 1] == L'\\')
                     {
                        NewName.Buffer[NewName.Length / 2 - 1] = 0;
                        NewName.Length -= 2;
                     }
                  }
                  else
                  {
                     if (NewName.Buffer[NewName.Length / 2 - 1] == L'\\')
                     {
                        Status = STATUS_INVALID_PARAMETER;
                        __leave;
                     }
                  }

                  while (NewName.Length > 0
                         && NewName.Buffer[NewName.Length / 2 - 1] != L'\\')
                  {
                     NewName.Length -= 2;
                  }

                  pParentFcb = FsdLookupFcbByFileName (Vcb, &NewName);

                  NewName.Buffer =
                     (USHORT *) ((UCHAR *) NewName.Buffer + NewName.Length);
                  NewName.Length = PtrFileName->Length - NewName.Length;

                  //Here we should create the fcb.
                  if (!pParentFcb)
                  {
                     if (NewName.Length == 0)
                     {
                        Status = STATUS_OBJECT_NAME_NOT_FOUND;
                        __leave;
                     }

                     //inode_t* tmpInode;
                     pTmpInode = ExAllocatePool (PagedPool, sizeof (inode_t));
                     if (!pTmpInode)
                     {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        __leave;
                     }
                     RtlCopyMemory (pTmpInode, Inode, sizeof (inode_t));
                     pParentFcb =
                        Ext2FsdAllocateFcb (Vcb, PtrFileName, found_index,
                                            pTmpInode);

                     if (!pParentFcb)
                     {
                        ExFreePool (pTmpInode);
                        Status = STATUS_INVALID_PARAMETER;
                        __leave;
                     }

                     ExAcquireResourceExclusiveLite (&pParentFcb->MainResource,
                                                     TRUE);
                     pParentFcb->ReferenceCount++;
                     ExReleaseResourceForThreadLite (&pParentFcb->MainResource,
                                                     ExGetCurrentResourceThread
                                                     ());

                  }
               }

               // Here we get a valid pParentFcb

               if (DirectoryFile)
               {
                  Status =
                     Ext2FsdCreateInode (IrpContext, Vcb, pParentFcb,
                                         pParentFcb->IndexNumber, EXT2_FT_DIR,
                                         IrpSp->Parameters.Create.
                                         FileAttributes, &NewName);
               }
               else
               {
                  Status =
                     Ext2FsdCreateInode (IrpContext, Vcb, pParentFcb,
                                         pParentFcb->IndexNumber,
                                         EXT2_FT_REG_FILE,
                                         IrpSp->Parameters.Create.
                                         FileAttributes, &NewName);
               }

               if (NT_SUCCESS (Status))
               {
                  // FIXME: what is it used for??
				  // bCreated = TRUE;

                  Irp->IoStatus.Information = FILE_CREATED;
                  pParentFcb = FsdLookupFcbByFileName (Vcb, &NewName);

                  /* Status = Ext2FsdLookupFileName (
                     Vcb,
                     &NewName,
                     pParentFcb,
                     &Ext2Mcb,
                     Inode      );
                   */

                  // if (!NT_SUCCESS(Status))
                  if (pParentFcb == NULL)
                  {
                     Ext2FsdBreakPoint ();
                  }
               }
               else
               {
                  Ext2FsdBreakPoint ();
               }
#ifdef TRACE
               KdPrint ((DRIVER_NAME ": reached the end!\n"));
#endif
            }

            Status = STATUS_OBJECT_NAME_INVALID;
            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
            __leave;
         }

         Fcb =
            Ext2FsdAllocateFcb (((PFSD_VCB) DeviceObject->DeviceExtension),
                                PtrFileName, found_index, Inode);

         if (Fcb == NULL)
         {
            DbgPrint ("ERROR CREATE : FCB == NULL");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
         }

#ifdef TRACE
         KdPrint ((DRIVER_NAME ": Allocated a new FCB for %s",
                   Fcb->AnsiFileName.Buffer));
#endif
      }

      Ccb = Ext2FsdAllocateCcb ();

      if (Ccb == NULL)
      {
         DbgPrint ("ERROR CREATE : CCB == NULL");
         Status = STATUS_INSUFFICIENT_RESOURCES;
         __leave;
      }

      Ccb->PtrFCB = Fcb;
      Fcb->OpenHandleCount++;
      Vcb->OpenFileHandleCount++;
      Fcb->ReferenceCount++;
      Vcb->ReferenceCount++;
      Fcb->CommonFCBHeader.IsFastIoPossible = FastIoIsNotPossible;

      IrpSp->FileObject->FsContext = (void *) Fcb;
      IrpSp->FileObject->FsContext2 = (void *) Ccb;
      IrpSp->FileObject->PrivateCacheMap = NULL;
      IrpSp->FileObject->SectionObjectPointer = &(Fcb->SectionObject);
      IrpSp->FileObject->Vpb = Vcb->Vpb;

      Irp->IoStatus.Information = FILE_OPENED;
      Status = STATUS_SUCCESS;

#ifdef TRACE
      KdPrint ((DRIVER_NAME ": %s OpenHandleCount: %u ReferenceCount: %u\n",
                Fcb->AnsiFileName.Buffer, Fcb->OpenHandleCount,
                Fcb->ReferenceCount));
#endif
   }
   __finally
   {
      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

      if (buffer)
      {
         Ext2FsdFreePool (buffer);
      }

      if (AbsolutePathName.Buffer != NULL)
      {
         ExFreePool (AbsolutePathName.Buffer);
      }

      if (!NT_SUCCESS (Status) && Inode)
      {
         Ext2FsdFreePool (Inode);
      }

      if (!IrpContext->ExceptionInProgress)
      {
         IrpContext->Irp->IoStatus.Status = Status;

         FsdCompleteRequest (IrpContext->Irp,
                             (CCHAR) (NT_SUCCESS (Status) ? IO_DISK_INCREMENT :
                                      IO_NO_INCREMENT));

         Ext2FsdFreeIrpContext (IrpContext);

         if (Vcb && FlagOn (Vcb->Flags, VCB_DISMOUNT_PENDING)
             && !Vcb->ReferenceCount)
         {
            Ext2FsdFreeVcb (Vcb);
         }
      }
   }

   return Status;
}

/*!\brief Checks if FCB for a file is in the memory already.
 *
 * Returns FCB for FullFileName if already exists in FcbList of Vcb or NULL if FCB not yet exists.
 *
 * @param Vcb           VolumeControlBlock of mounted Partition
 * @param FullFileName  Full Path plus Filename
 * @return              Fcb of requested File
 */
PFSD_FCB
FsdLookupFcbByFileName (IN PFSD_VCB Vcb, IN PUNICODE_STRING FullFileName)
{
   PLIST_ENTRY ListEntry = NULL;
   PFSD_FCB Fcb = NULL;

   ListEntry = Vcb->FcbList.Flink;

   ASSERT (ListEntry);

   while (ListEntry != &Vcb->FcbList)
   {
      Fcb = CONTAINING_RECORD (ListEntry, FSD_FCB, Next);

      if (!RtlCompareUnicodeString (&Fcb->FileName, FullFileName, FALSE))
      {
#ifdef TRACE
         KdPrint ((DRIVER_NAME ": Found an allocated FCB for %s",
                   Fcb->AnsiFileName.Buffer));
#endif
         return Fcb;
      }

      ListEntry = ListEntry->Flink;
   }
   return NULL;
}
