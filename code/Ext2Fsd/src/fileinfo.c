/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_QUERY_INFORMATION

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/fileinfo.c,v $
    $Revision: 1.5 $
*/

#include "ntifs.h"
#include "fsd.h"
#include "ext2.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdQueryInformation)
#pragma alloc_text(PAGE, Ext2FsdSetInformation)
#endif


NTSTATUS
Ext2FsdQueryInformation (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFILE_OBJECT FileObject = NULL;
   PFSD_FCB Fcb = NULL;
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   FILE_INFORMATION_CLASS FileInformationClass;
   ULONG Length;
   PVOID SystemBuffer = NULL;
   BOOLEAN FcbResourceAcquired = FALSE;

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

      FileObject = IrpContext->FileObject;

      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb != NULL);

      if (Fcb->Identifier.Type == VCB)
      {
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

#ifndef FSD_RO
      if (!FlagOn (Fcb->Flags, FCB_PAGE_FILE))
#endif
      {
         if (!ExAcquireResourceSharedLite
             (&Fcb->MainResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         FcbResourceAcquired = TRUE;
      }

      Irp = IrpContext->Irp;

      IrpSp = IoGetCurrentIrpStackLocation (Irp);

      FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;

      Length = IrpSp->Parameters.QueryFile.Length;

      SystemBuffer = Irp->AssociatedIrp.SystemBuffer;

      RtlZeroMemory (SystemBuffer, Length);

      switch (FileInformationClass)
      {
      case FileBasicInformation:
         {
            PFILE_BASIC_INFORMATION Buffer;

            if (Length < sizeof (FILE_BASIC_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_BASIC_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_BASIC_INFORMATION {
                    LARGE_INTEGER   CreationTime;
                    LARGE_INTEGER   LastAccessTime;
                    LARGE_INTEGER   LastWriteTime;
                    LARGE_INTEGER   ChangeTime;
                    ULONG           FileAttributes;
                } FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;
*/


            ext2_getCreationTimeNT (Fcb->inode, &Buffer->CreationTime);
            ext2_getAccessTimeNT (Fcb->inode, &Buffer->LastAccessTime);
            ext2_getModificationTimeNT (Fcb->inode, &Buffer->LastWriteTime);
            ext2_getModificationTimeNT (Fcb->inode, &Buffer->ChangeTime);

            Buffer->FileAttributes = Fcb->FileAttributes;

            Irp->IoStatus.Information = sizeof (FILE_BASIC_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileStandardInformation:
         {
            PFILE_STANDARD_INFORMATION Buffer;

            if (Length < sizeof (FILE_STANDARD_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_STANDARD_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_STANDARD_INFORMATION {
                    LARGE_INTEGER   AllocationSize;
                    LARGE_INTEGER   EndOfFile;
                    ULONG           NumberOfLinks;
                    BOOLEAN         DeletePending;
                    BOOLEAN         Directory;
                } FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;
*/

            /*
               Buffer->AllocationSize.QuadPart =
               be32_to_cpu(Fcb->inode->i_size);
               // */

            Buffer->AllocationSize.QuadPart = Fcb->inode->i_size;

            /*
               Buffer->EndOfFile.QuadPart =
               be32_to_cpu(Fcb->inode->i_size);
               // */

            Buffer->EndOfFile.QuadPart = Fcb->inode->i_size;


            Buffer->NumberOfLinks = 1;

#ifndef FSD_RO
            Buffer->DeletePending =
               (BOOLEAN) FlagOn (Fcb->Flags, FCB_DELETE_PENDING);
#else
            Buffer->DeletePending = FALSE;
#endif
            Buffer->Directory =
               (BOOLEAN) FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY);

            Irp->IoStatus.Information = sizeof (FILE_STANDARD_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileInternalInformation:
         {
            PFILE_INTERNAL_INFORMATION Buffer;

            if (Length < sizeof (FILE_INTERNAL_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_INTERNAL_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_INTERNAL_INFORMATION {
                    LARGE_INTEGER IndexNumber;
                } FILE_INTERNAL_INFORMATION, *PFILE_INTERNAL_INFORMATION;
*/

            // The "inode number"
            Buffer->IndexNumber.QuadPart = Fcb->IndexNumber;

            Irp->IoStatus.Information = sizeof (FILE_INTERNAL_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileEaInformation:
         {
            PFILE_EA_INFORMATION Buffer;

            if (Length < sizeof (FILE_EA_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_EA_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_EA_INFORMATION {
                    ULONG EaSize;
                } FILE_EA_INFORMATION, *PFILE_EA_INFORMATION;
*/

            // Romfs doesn't have any extended attributes
            Buffer->EaSize = 0;

            Irp->IoStatus.Information = sizeof (FILE_EA_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileNameInformation:
         {
            PFILE_NAME_INFORMATION Buffer;

            if (Length <
                sizeof (FILE_NAME_INFORMATION) + Fcb->FileName.Length -
                sizeof (WCHAR))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_NAME_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_NAME_INFORMATION {
                    ULONG FileNameLength;
                    WCHAR FileName[1];
                } FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;
*/

            Buffer->FileNameLength = Fcb->FileName.Length;

            RtlCopyMemory (Buffer->FileName, Fcb->FileName.Buffer,
                           Fcb->FileName.Length);

            Irp->IoStatus.Information =
               sizeof (FILE_NAME_INFORMATION) + Fcb->FileName.Length -
               sizeof (WCHAR);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FilePositionInformation:
         {
            PFILE_POSITION_INFORMATION Buffer;

            if (Length < sizeof (FILE_POSITION_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_POSITION_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_POSITION_INFORMATION {
                    LARGE_INTEGER CurrentByteOffset;
                } FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;
*/

            Buffer->CurrentByteOffset = FileObject->CurrentByteOffset;

            Irp->IoStatus.Information = sizeof (FILE_POSITION_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileAllInformation:
         {
            PFILE_ALL_INFORMATION FileAllInformation;
            PFILE_BASIC_INFORMATION FileBasicInformation;
            PFILE_STANDARD_INFORMATION FileStandardInformation;
            PFILE_INTERNAL_INFORMATION FileInternalInformation;
            PFILE_EA_INFORMATION FileEaInformation;
            PFILE_POSITION_INFORMATION FilePositionInformation;
            PFILE_NAME_INFORMATION FileNameInformation;

            if (Length < sizeof (FILE_ALL_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            FileAllInformation = (PFILE_ALL_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_ALL_INFORMATION {
                    FILE_BASIC_INFORMATION      BasicInformation;
                    FILE_STANDARD_INFORMATION   StandardInformation;
                    FILE_INTERNAL_INFORMATION   InternalInformation;
                    FILE_EA_INFORMATION         EaInformation;
                    FILE_ACCESS_INFORMATION     AccessInformation;
                    FILE_POSITION_INFORMATION   PositionInformation;
                    FILE_MODE_INFORMATION       ModeInformation;
                    FILE_ALIGNMENT_INFORMATION  AlignmentInformation;
                    FILE_NAME_INFORMATION       NameInformation;
                } FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
*/

            FileBasicInformation = &FileAllInformation->BasicInformation;
            FileStandardInformation = &FileAllInformation->StandardInformation;
            FileInternalInformation = &FileAllInformation->InternalInformation;
            FileEaInformation = &FileAllInformation->EaInformation;
            FilePositionInformation = &FileAllInformation->PositionInformation;
            FileNameInformation = &FileAllInformation->NameInformation;

            ext2_getCreationTimeNT (Fcb->inode,
                                    &FileBasicInformation->CreationTime);
            ext2_getAccessTimeNT (Fcb->inode,
                                  &FileBasicInformation->LastAccessTime);
            ext2_getModificationTimeNT (Fcb->inode,
                                        &FileBasicInformation->LastWriteTime);
            ext2_getModificationTimeNT (Fcb->inode,
                                        &FileBasicInformation->ChangeTime);

            FileBasicInformation->FileAttributes = Fcb->FileAttributes;

            /*
               FileStandardInformation->AllocationSize.QuadPart =
               be32_to_cpu(Fcb->inode->i_size);

               FileStandardInformation->EndOfFile.QuadPart =
               be32_to_cpu(Fcb->inode->i_size);

               FileStandardInformation->NumberOfLinks = 1;
               // */

            FileStandardInformation->AllocationSize.QuadPart =
               Fcb->inode->i_size;
            FileStandardInformation->EndOfFile.QuadPart = Fcb->inode->i_size;
            FileStandardInformation->NumberOfLinks = 1;


#ifndef FSD_RO
            FileStandardInformation->DeletePending =
               (BOOLEAN) FlagOn (Fcb->Flags, FCB_DELETE_PENDING);
#else
            FileStandardInformation->DeletePending = FALSE;
#endif

            FileStandardInformation->Directory =
               (BOOLEAN) FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY);

            // The "inode number"
            FileInternalInformation->IndexNumber.QuadPart = Fcb->IndexNumber;

            // Ext2 doesn't have any extended attributes
            FileEaInformation->EaSize = 0;

            FilePositionInformation->CurrentByteOffset =
               FileObject->CurrentByteOffset;

            if (Length <
                sizeof (FILE_ALL_INFORMATION) + Fcb->FileName.Length -
                sizeof (WCHAR))
            {
               Irp->IoStatus.Information = sizeof (FILE_ALL_INFORMATION);
               FileNameInformation->FileNameLength = 0;
               DbgPrint ("Ext2FsdQueryInformation %i %i", Length,
                         sizeof (FILE_ALL_INFORMATION) + Fcb->FileName.Length -
                         sizeof (WCHAR));
//                    Status = STATUS_BUFFER_OVERFLOW;
//                    __leave;
            }
            else
            {
               FileNameInformation->FileNameLength = Fcb->FileName.Length;
               RtlCopyMemory (FileNameInformation->FileName,
                              Fcb->FileName.Buffer, Fcb->FileName.Length);
               Irp->IoStatus.Information =
                  sizeof (FILE_ALL_INFORMATION) + Fcb->FileName.Length -
                  sizeof (WCHAR);
            }


            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileNetworkOpenInformation:
         {
            PFILE_NETWORK_OPEN_INFORMATION Buffer;

            if (Length < sizeof (FILE_NETWORK_OPEN_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_NETWORK_OPEN_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_NETWORK_OPEN_INFORMATION {
                    LARGE_INTEGER   CreationTime;
                    LARGE_INTEGER   LastAccessTime;
                    LARGE_INTEGER   LastWriteTime;
                    LARGE_INTEGER   ChangeTime;
                    LARGE_INTEGER   AllocationSize;
                    LARGE_INTEGER   EndOfFile;
                    ULONG           FileAttributes;
                } FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;
*/


            Buffer->AllocationSize.QuadPart = Fcb->inode->i_size;
            Buffer->EndOfFile.QuadPart = Fcb->inode->i_size;

            Buffer->FileAttributes = Fcb->FileAttributes;

            ext2_getCreationTimeNT (Fcb->inode, &Buffer->CreationTime);
            ext2_getAccessTimeNT (Fcb->inode, &Buffer->LastAccessTime);
            ext2_getModificationTimeNT (Fcb->inode, &Buffer->LastWriteTime);
            ext2_getModificationTimeNT (Fcb->inode, &Buffer->ChangeTime);

            Buffer->AllocationSize.QuadPart = Fcb->inode->i_size;

            Buffer->EndOfFile.QuadPart = Fcb->inode->i_size;

            Irp->IoStatus.Information = sizeof (FILE_NETWORK_OPEN_INFORMATION);

            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileAttributeTagInformation:
         {
            PFILE_ATTRIBUTE_TAG_INFORMATION Buffer;

            if (Length < sizeof (FILE_ATTRIBUTE_TAG_INFORMATION))
            {
               Status = STATUS_INFO_LENGTH_MISMATCH;
               __leave;
            }

            Buffer = (PFILE_ATTRIBUTE_TAG_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_ATTRIBUTE_TAG_INFORMATION {
                    ULONG FileAttributes;
                    ULONG ReparseTag;
                } FILE_ATTRIBUTE_TAG_INFORMATION, *PFILE_ATTRIBUTE_TAG_INFORMATION;
*/
            Buffer->FileAttributes = Fcb->FileAttributes;
            Buffer->ReparseTag = 0;

            Irp->IoStatus.Information = sizeof (FILE_ATTRIBUTE_TAG_INFORMATION);
            Status = STATUS_SUCCESS;
            __leave;
         }

      case FileStreamInformation:
         {
            Status = STATUS_NOT_IMPLEMENTED;
            __leave;
         }

      default:
         DbgPrint
            ("ERROR : Ext2FsdQueryInformation : STATUS_INVALID_INFO_CLASS");
         Status = STATUS_INVALID_INFO_CLASS;
      }
   }
   __finally
   {
      if (FcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Fcb->MainResource,
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

NTSTATUS
Ext2FsdSetInformation (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb;
   PFILE_OBJECT FileObject;
   PFSD_FCB Fcb;
   PIRP Irp;
   PIO_STACK_LOCATION IrpSp;
   FILE_INFORMATION_CLASS FileInformationClass;
   ULONG Length;
   PVOID SystemBuffer;

#ifndef FSD_RO
   BOOLEAN VcbResourceAcquired = FALSE;
#endif
   BOOLEAN FcbMainResourceAcquired = FALSE;

#ifndef FSD_RO
   BOOLEAN FcbPagingIoResourceAcquired = FALSE;
#endif

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

      FileObject = IrpContext->FileObject;

      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb != NULL);

      if (Fcb->Identifier.Type == VCB)
      {
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

      Irp = IrpContext->Irp;

      IrpSp = IoGetCurrentIrpStackLocation (Irp);

      FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;

      Length = IrpSp->Parameters.SetFile.Length;

      SystemBuffer = Irp->AssociatedIrp.SystemBuffer;

#ifndef FSD_RO

      if (FileInformationClass == FileDispositionInformation
          || FileInformationClass == FileRenameInformation
          || FileInformationClass == FileLinkInformation)
      {
         if (!ExAcquireResourceExclusiveLite
             (&Vcb->MainResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         VcbResourceAcquired = TRUE;
      }

#endif // !FSD_RO

#ifndef FSD_RO
      if (!FlagOn (Fcb->Flags, FCB_PAGE_FILE))
#endif
      {
         if (!ExAcquireResourceExclusiveLite
             (&Fcb->MainResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         FcbMainResourceAcquired = TRUE;
      }

#ifndef FSD_RO

      if (FileInformationClass == FileDispositionInformation
          || FileInformationClass == FileRenameInformation
          || FileInformationClass == FileLinkInformation
          || FileInformationClass == FileAllocationInformation
          || FileInformationClass == FileEndOfFileInformation)
      {
         if (!ExAcquireResourceExclusiveLite
             (&Fcb->PagingIoResource, IrpContext->IsSynchronous))
         {
            Status = STATUS_PENDING;
            __leave;
         }

         FcbPagingIoResourceAcquired = TRUE;
      }

#endif // !FSD_RO

#ifndef FSD_RO
      if (FlagOn (Vcb->Flags, VCB_READ_ONLY))
#endif
      {
         if (FileInformationClass != FilePositionInformation)
         {
            Status = STATUS_MEDIA_WRITE_PROTECTED;
            __leave;
         }
      }

      switch (FileInformationClass)
      {
         //
         // This is the only set file information request supported on read
         // only file systems
         //
      case FilePositionInformation:
         {
            PFILE_POSITION_INFORMATION Buffer;

            if (Length < sizeof (FILE_POSITION_INFORMATION))
            {
               DbgPrint
                  ("ERROR : Ext2FsdSetInformation [1]: STATUS_INVALID_PARAMETER");
               Status = STATUS_INVALID_PARAMETER;
               __leave;
            }

            Buffer = (PFILE_POSITION_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_POSITION_INFORMATION {
                    LARGE_INTEGER CurrentByteOffset;
                } FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;
*/

            if (FlagOn (FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING)
                && (Buffer->CurrentByteOffset.LowPart & DeviceObject->
                    AlignmentRequirement))
            {
               DbgPrint
                  ("ERROT : Ext2FsdSetInformation [2] : STATUS_INVALID_PARAMETER");
               Status = STATUS_INVALID_PARAMETER;
               __leave;
            }

            FileObject->CurrentByteOffset = Buffer->CurrentByteOffset;

            Status = STATUS_SUCCESS;
            __leave;
         }

      default:
         Status = STATUS_INVALID_INFO_CLASS;
      }
   }
   __finally
   {

#ifndef FSD_RO

      if (FcbPagingIoResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Fcb->PagingIoResource,
                                         ExGetCurrentResourceThread ());
      }

#endif // !FSD_RO

      if (FcbMainResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Fcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

#ifndef FSD_RO

      if (VcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Vcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

#endif // !FSD_RO

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
