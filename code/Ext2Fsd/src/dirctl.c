/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle IRP_MJ_DIRECTORY_CONTROL

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/06/30 19:53:40 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/dirctl.c,v $
    $Revision: 1.7 $
*/

#include "ntifs.h"
#include "fsd.h"
#include "ext2.h"
#include "io.h"

ULONG Ext2GetInfoLength (IN FILE_INFORMATION_CLASS FileInformationClass);
void Ext2FsdQueryDirectoryDebug (PUNICODE_STRING FileName, ULONG FileIndex,
                                 BOOLEAN RestartScan, BOOLEAN ReturnSingleEntry,
                                 BOOLEAN IndexSpecified,
                                 FILE_INFORMATION_CLASS FileInformationClass);
ULONG Ext2Align (ULONG address);
ULONG Ext2CountDirEntries (ext2_t * ext2, char *buffer, ULONG size);
BOOLEAN Ext2FillUpInformation (ext2_t * ext2,
                               FILE_INFORMATION_CLASS FileInformationClass,
                               PUCHAR UserBuffer, dir_iterator_t * dir_iterator,
                               ULONG index, PULONG used, PANSI_STRING dir,
                               PULONG parentInode);
BOOLEAN Ext2MatchFileName (PUNICODE_STRING Expression,
                           PUNICODE_STRING FileName);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Ext2FsdDirectoryControl)
#pragma alloc_text(PAGE, Ext2GetInfoLength)
#pragma alloc_text(PAGE, Ext2FsdQueryDirectoryDebug)
#pragma alloc_text(PAGE, Ext2Align)
#pragma alloc_text(PAGE, Ext2CountDirEntries)
#pragma alloc_text(PAGE, Ext2FillUpInformation)
#pragma alloc_text(PAGE, Ext2MatchFileName)
#pragma alloc_text(PAGE, Ext2FsdQueryDirectory)
#pragma alloc_text(PAGE, Ext2FsdNotifyChangeDirectory)

#endif


NTSTATUS
Ext2FsdDirectoryControl (IN PFSD_IRP_CONTEXT IrpContext)
{
   NTSTATUS Status;

   ASSERT (IrpContext);

   ASSERT ((IrpContext->Identifier.Type == ICX)
           && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

   switch (IrpContext->MinorFunction)
   {
   case IRP_MN_QUERY_DIRECTORY:
      Status = Ext2FsdQueryDirectory (IrpContext);
      break;

   case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
      Status = Ext2FsdNotifyChangeDirectory (IrpContext);
      break;

   default:
      Status = STATUS_INVALID_DEVICE_REQUEST;
      IrpContext->Irp->IoStatus.Status = Status;
      FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
      Ext2FsdFreeIrpContext (IrpContext);
   }

   return Status;
}

ULONG
Ext2GetInfoLength (IN FILE_INFORMATION_CLASS FileInformationClass)
{
   switch (FileInformationClass)
   {
   case FileDirectoryInformation:
      return sizeof (FILE_DIRECTORY_INFORMATION);
      break;

   case FileFullDirectoryInformation:
      return sizeof (FILE_FULL_DIR_INFORMATION);
      break;

   case FileBothDirectoryInformation:
      return sizeof (FILE_BOTH_DIR_INFORMATION);
      break;

   case FileNamesInformation:
      return sizeof (FILE_NAMES_INFORMATION);
      break;

   default:
      DbgPrint ("ERROR : Ext2GetInfoLength");
      ASSERT (0);
      break;
   }

   return sizeof (FILE_FULL_DIR_INFORMATION) * 2;
}

void
Ext2FsdQueryDirectoryDebug (PUNICODE_STRING FileName, ULONG FileIndex,
                            BOOLEAN RestartScan, BOOLEAN ReturnSingleEntry,
                            BOOLEAN IndexSpecified,
                            FILE_INFORMATION_CLASS FileInformationClass)
{

   if (FileName != NULL)
   {
      DbgPrint ("Ext2FsdQueryDirectoryDebug : FileName=%.*S, FileIndex=%i\n",
                FileName->Length / 2, FileName->Buffer, FileIndex);
   }

   DbgPrint
      ("Ext2FsdQueryDirectoryDebug : RestartScan=%s, ReturnSingleEntry=%s, IndexSpecified=%s\n",
       RestartScan ? "TRUE" : "FALSE", ReturnSingleEntry ? "TRUE" : "FALSE",
       IndexSpecified ? "TRUE" : "FALSE");

   switch (FileInformationClass)
   {
   case FileDirectoryInformation:
      DbgPrint ("QueryDir : FileInformationClass = FileDirectoryInformation\n");
      break;

   case FileFullDirectoryInformation:
      DbgPrint
         ("QueryDir : FileInformationClass = FileFullDirectoryInformation\n");
      break;

   case FileBothDirectoryInformation:
      DbgPrint
         ("QueryDir : FileInformationClass = FileBothDirectoryInformation\n");
      break;

   case FileNamesInformation:
      DbgPrint ("QueryDir : FileInformationClass = FileNamesInformation\n");
      break;

   default:
      DbgPrint
         ("ERROR : Ext2FsdQueryDirectoryDebug : FileInformationClass = ??\n");
   }

   DbgPrint ("Ext2FsdQueryDirectoryDebug : ");
}

ULONG
Ext2Align (ULONG address)
{
   if (address % 8 == 0)
   {
      return address;
   }
   else
   {
      return address + (8 - (address % 8));
   }
}

ULONG
Ext2CountDirEntries (ext2_t * ext2, char *buffer, ULONG size)
{
   ULONG i = 0;
   dir_iterator_t *it;

   it = dir_iterator_create ();

   while (ext2_list_dir (ext2, buffer, size, it) == EXT2_STATUS_OK)
   {
      i++;
   }

   dir_iterator_dispose (it);

   return i;
}

BOOLEAN
Ext2FillUpInformation (ext2_t * ext2,
                       FILE_INFORMATION_CLASS FileInformationClass,
                       PUCHAR UserBuffer, dir_iterator_t * dir_iterator,
                       ULONG index, PULONG used, PANSI_STRING dir,
                       PULONG parentInode)
{
   inode_t *inode = NULL;
   BOOLEAN state = TRUE;
   char *file = NULL;
   _u32 inode_num = EXT2_INVALID_INODE;

   //Load the inode

   inode = Ext2FsdAllocateInode ();

   ASSERT (inode);

   __try
   {
      if (dir_iterator->ext2_dir_entry.file_type == EXT2_FT_SYMLINK)
      {
         file =
            Ext2FsdAllocatePool (NonPagedPool,
                                 dir->Length +
                                 dir_iterator->ext2_dir_entry.name_len + 1,
                                 '5yDR');

         ASSERT (file);
         if (file == NULL)
         {
            state = FALSE;
            __leave;
         }

         strcpy (file, dir->Buffer);

         strcat (file, dir_iterator->ext2_dir_entry.name);

         inode_num = ext2_lookup (ext2, file, *parentInode);

         if (!ext2_inode_valid (ext2, inode_num))
         {
            state = TRUE;
            DbgPrint ("ERROR : Ext2FillUpInformation : inode_num invalid");
            __leave;
         }

         if (ext2_load_inode (ext2, inode, inode_num) != EXT2_STATUS_OK)
         {
            state = FALSE;
            DbgPrint ("ERROR : Ext2FillUpInformation : load inode failed");
            __leave;
         }

      }
      else
      {
         if (ext2_load_inode (ext2, inode, dir_iterator->ext2_dir_entry.inode)
             != EXT2_STATUS_OK)
         {
            state = FALSE;      //We ignore it....
            DbgPrint ("ERROR : Ext2FillUpInformation : load inode failed");
            __leave;
         }
      }
#ifdef TRACE
      DbgPrint ("Ext2FillUpInformation : i#=%i ft=%i na=%s",
                dir_iterator->ext2_dir_entry.inode,
                dir_iterator->ext2_dir_entry.file_type,
                dir_iterator->ext2_dir_entry.name);
#endif
      //Fill up the FileInformationClass Structs

      switch (FileInformationClass)
      {
      case FileDirectoryInformation:
         {
            PFILE_DIRECTORY_INFORMATION Buffer =
               (PFILE_DIRECTORY_INFORMATION) (UserBuffer);;

            ext2_getCreationTimeNT (inode, &Buffer->CreationTime);
            ext2_getAccessTimeNT (inode, &Buffer->LastAccessTime);
            ext2_getModificationTimeNT (inode, &Buffer->LastWriteTime);
            ext2_getModificationTimeNT (inode, &Buffer->ChangeTime);

            Buffer->EndOfFile.QuadPart = inode->i_size;
            Buffer->AllocationSize.QuadPart = inode->i_size;
            Buffer->FileIndex = index;

            Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

            SetFlag (Buffer->FileAttributes, FILE_ATTRIBUTE_READONLY);

            if (S_ISDIR (inode->i_mode))
               SetFlag (Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY);

            Buffer->FileNameLength = dir_iterator->ext2_dir_entry.name_len * 2;
            FsdCharToWchar (Buffer->FileName, dir_iterator->ext2_dir_entry.name,
                            dir_iterator->ext2_dir_entry.name_len + 1);

            Buffer->NextEntryOffset =
               sizeof (FILE_DIRECTORY_INFORMATION) + Buffer->FileNameLength -
               sizeof (WCHAR);
            Buffer->NextEntryOffset = Ext2Align (Buffer->NextEntryOffset);

            (*used) = (*used) + Buffer->NextEntryOffset;

            break;
         }
         break;

      case FileFullDirectoryInformation:
         {
            PFILE_FULL_DIR_INFORMATION Buffer =
               (PFILE_FULL_DIR_INFORMATION) (UserBuffer);;

            ext2_getCreationTimeNT (inode, &Buffer->CreationTime);
            ext2_getAccessTimeNT (inode, &Buffer->LastAccessTime);
            ext2_getModificationTimeNT (inode, &Buffer->LastWriteTime);
            ext2_getModificationTimeNT (inode, &Buffer->ChangeTime);

            Buffer->EndOfFile.QuadPart = inode->i_size;
            Buffer->AllocationSize.QuadPart = inode->i_size;
            Buffer->FileIndex = index;

            Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;
            SetFlag (Buffer->FileAttributes, FILE_ATTRIBUTE_READONLY);

            if (S_ISDIR (inode->i_mode))
               SetFlag (Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY);

            Buffer->FileNameLength = dir_iterator->ext2_dir_entry.name_len * 2;
            FsdCharToWchar (Buffer->FileName, dir_iterator->ext2_dir_entry.name,
                            dir_iterator->ext2_dir_entry.name_len + 1);

            Buffer->NextEntryOffset =
               sizeof (FILE_FULL_DIR_INFORMATION) + Buffer->FileNameLength -
               sizeof (WCHAR);
            Buffer->NextEntryOffset = Ext2Align (Buffer->NextEntryOffset);

            (*used) = (*used) + Buffer->NextEntryOffset;

            break;
         }
         break;


      case FileNamesInformation:
         {
            PFILE_NAMES_INFORMATION Buffer =
               (PFILE_NAMES_INFORMATION) (UserBuffer);;

            Buffer->FileIndex = index;

            Buffer->FileNameLength = dir_iterator->ext2_dir_entry.name_len * 2;
            FsdCharToWchar (Buffer->FileName, dir_iterator->ext2_dir_entry.name,
                            dir_iterator->ext2_dir_entry.name_len * 2);

            Buffer->NextEntryOffset =
               sizeof (FILE_NAMES_INFORMATION) + Buffer->FileNameLength -
               sizeof (WCHAR);
            Buffer->NextEntryOffset = Ext2Align (Buffer->NextEntryOffset);

            (*used) = (*used) + Buffer->NextEntryOffset;

            break;
         }

      case FileBothDirectoryInformation:
         {
            PFILE_BOTH_DIR_INFORMATION Buffer =
               (PFILE_BOTH_DIR_INFORMATION) (UserBuffer);;

            ext2_getCreationTimeNT (inode, &Buffer->CreationTime);
            ext2_getAccessTimeNT (inode, &Buffer->LastAccessTime);
            ext2_getModificationTimeNT (inode, &Buffer->LastWriteTime);
            ext2_getModificationTimeNT (inode, &Buffer->ChangeTime);

            Buffer->EndOfFile.QuadPart = inode->i_size;
            Buffer->AllocationSize.QuadPart = inode->i_size;
            Buffer->FileIndex = index;

            Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

            SetFlag (Buffer->FileAttributes, FILE_ATTRIBUTE_READONLY);

            if (S_ISDIR (inode->i_mode))
               SetFlag (Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY);

            Buffer->FileNameLength = dir_iterator->ext2_dir_entry.name_len * 2;
            FsdCharToWchar (Buffer->FileName, dir_iterator->ext2_dir_entry.name,
                            dir_iterator->ext2_dir_entry.name_len + 1);

            Buffer->NextEntryOffset =
               sizeof (FILE_BOTH_DIR_INFORMATION) + Buffer->FileNameLength -
               sizeof (WCHAR);
            Buffer->NextEntryOffset = Ext2Align (Buffer->NextEntryOffset);

            (*used) = (*used) + Buffer->NextEntryOffset;

            break;
         }

      default:
         {
            DbgPrint ("ERROR : Ext2FillUpInformation : ???");
         }
      }
   }
   __finally
   {
      if (inode)
      {
         Ext2FsdFreeInode (inode);
      }

      if (file)
      {
         Ext2FsdFreePool (file);
      }
   }

   return state;
}

BOOLEAN
Ext2MatchFileName (PUNICODE_STRING Expression, PUNICODE_STRING FileName)
{
   if (Expression == NULL)
      return TRUE;

   if (FsRtlDoesNameContainWildCards (Expression))
   {
      return FsRtlIsNameInExpression (Expression, FileName, FALSE, NULL);
   }
   else
   {
      if (RtlCompareUnicodeString (Expression, FileName, FALSE) == 0)
      {
         return TRUE;
      }
      else
      {
         return FALSE;
      }

   }
}

NTSTATUS
Ext2FsdQueryDirectory (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb = NULL;
   PFILE_OBJECT FileObject = NULL;
   PFSD_FCB Fcb = NULL;
   PFSD_CCB Ccb = NULL;
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   FILE_INFORMATION_CLASS FileInformationClass;
   ULONG Length;
   PUNICODE_STRING FileName = NULL;
   ULONG FileIndex;
   BOOLEAN RestartScan;
   BOOLEAN ReturnSingleEntry;
   BOOLEAN IndexSpecified;
   PUCHAR UserBuffer = NULL;
   inode_t *Inode = NULL;
   BOOLEAN FcbResourceAcquired = FALSE;
   ULONG UsedLength = 0;
   ULONG EntryCount;
   PUCHAR buffer = NULL;
   dir_iterator_t *dir_iterator = NULL;
   ULONG index = 0;
   ULONG offset = 0;
   ULONG MinLength = 0;
   UNICODE_STRING CurrentInodeName;
   PUCHAR p = NULL;
   ULONG count = 0;

   __try
   {
      ASSERT (IrpContext);

      ASSERT ((IrpContext->Identifier.Type == ICX)
              && (IrpContext->Identifier.Size == sizeof (FSD_IRP_CONTEXT)));

      CurrentInodeName.Buffer = NULL;
      CurrentInodeName.MaximumLength = (EXT2_NAME_LEN + 1) * 2;
      CurrentInodeName.Buffer =
         Ext2FsdAllocatePool (NonPagedPool, CurrentInodeName.MaximumLength,
                              '5xDR');

      if (CurrentInodeName.Buffer == NULL)
      {
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : CurrentInodeName.Buffer == NULL");
         Status = STATUS_INSUFFICIENT_RESOURCES;
         __leave;
      }

      DeviceObject = IrpContext->DeviceObject;
      ASSERT (DeviceObject);

      if (DeviceObject == FsdGlobalData.DeviceObject)
      {
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : DeviceObject == FsdGlobalData.DeviceObject");
         Status = STATUS_INVALID_DEVICE_REQUEST;
         __leave;
      }

      Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

      ASSERT (Vcb != NULL);

      ASSERT ((Vcb->Identifier.Type == VCB)
              && (Vcb->Identifier.Size == sizeof (FSD_VCB)));


      FileObject = IrpContext->FileObject;

      ASSERT (FileObject);


      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb);

      if (Fcb->Identifier.Type == VCB)
      {
         Status = STATUS_INVALID_PARAMETER;
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : Fcb->Identifier.Type == VCB");
         __leave;
      }

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

      if (!FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
      {
         Status = STATUS_INVALID_PARAMETER;
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : !FlagOn(Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY)");
         __leave;
      }

      Ccb = (PFSD_CCB) FileObject->FsContext2;

      ASSERT (Ccb);

      ASSERT ((Ccb->Identifier.Type == CCB)
              && (Ccb->Identifier.Size == sizeof (FSD_CCB)));

      Irp = IrpContext->Irp;
      ASSERT (Irp);

      IrpSp = IoGetCurrentIrpStackLocation (Irp);
      ASSERT (IrpSp);

      FileInformationClass =
         IrpSp->Parameters.QueryDirectory.FileInformationClass;

      Length = IrpSp->Parameters.QueryDirectory.Length;
      FileName = (PUNICODE_STRING) IrpSp->Parameters.QueryDirectory.FileName;
      FileIndex = IrpSp->Parameters.QueryDirectory.FileIndex;

      RestartScan = FlagOn (IrpSp->Flags, SL_RESTART_SCAN);
      ReturnSingleEntry = FlagOn (IrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
      IndexSpecified = FlagOn (IrpSp->Flags, SL_INDEX_SPECIFIED);


#ifdef TRACE
      Ext2FsdQueryDirectoryDebug (FileName, FileIndex, RestartScan,
                                  ReturnSingleEntry, IndexSpecified,
                                  FileInformationClass);
#endif

      if (Irp->RequestorMode != KernelMode && !Irp->MdlAddress
          && Irp->UserBuffer)
      {
         ProbeForWrite (Irp->UserBuffer,
                        IrpSp->Parameters.QueryDirectory.Length, 1);
      }

      UserBuffer = FsdGetUserBuffer (Irp);

      if (UserBuffer == NULL)
      {
         Status = STATUS_INVALID_USER_BUFFER;
         DbgPrint ("ERROR : Ext2FsdQueryDirectory : UserBuffer == NULL");
         __leave;
      }

      if (!IrpContext->IsSynchronous)
      {
         Status = STATUS_PENDING;
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : !IrpContext->IsSynchronous");
         __leave;
      }

      if (!ExAcquireResourceSharedLite
          (&Fcb->MainResource, IrpContext->IsSynchronous))
      {
         Status = STATUS_PENDING;
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : !ExAcquireResourceSharedLite(&Fcb->MainResource, IrpContext->IsSynchronous)");
         __leave;
      }

      RtlZeroMemory (UserBuffer, Length);

      FcbResourceAcquired = TRUE;

      MinLength =
         Ext2GetInfoLength (FileInformationClass) + 2 * (EXT2_NAME_LEN + 1);


      //------------------------------------------------------------------------
      //Ok, new iterate through the directory
      //------------------------------------------------------------------------

      if (RestartScan)
      {
         Ccb->FileIndex = 0;
      }
      else if (IndexSpecified)
      {
         Ccb->FileIndex = FileIndex;
      }

      if (FileName != NULL)
      {
         Ccb->DirectorySearchPattern.Length =
            Ccb->DirectorySearchPattern.MaximumLength = FileName->Length;

         Ccb->DirectorySearchPattern.Buffer =
            Ext2FsdAllocatePool (NonPagedPool, FileName->Length, '2iDR');

         if (Ccb->DirectorySearchPattern.Buffer == NULL)
         {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
         }

         RtlCopyMemory (Ccb->DirectorySearchPattern.Buffer, FileName->Buffer,
                        FileName->Length);

      }

      buffer = Ext2FsdAllocatePool (NonPagedPool, Fcb->inode->i_size, '5xDX');
      ASSERT (buffer);

      if (buffer == NULL)
      {
         Status = STATUS_UNSUCCESSFUL;
         DbgPrint ("ERROR : Ext2FsdQueryDirectory : buffer == null");
         __leave;
      }

      if (ext2_read_inode
          (Vcb->ext2, Fcb->inode, buffer, 0, Fcb->inode->i_size,
           0) != EXT2_STATUS_OK)
      {
         Status = STATUS_UNSUCCESSFUL;
         DbgPrint
            ("ERROR : Ext2FsdQueryDirectory : !ExAcquireResourceSharedLite(&Fcb->MainResource, IrpContext->IsSynchronous");
         __leave;
      }

      EntryCount = Ext2CountDirEntries (Vcb->ext2, buffer, Fcb->inode->i_size);


      if (Ccb->FileIndex >= EntryCount)
      {
         Status = STATUS_NO_MORE_FILES;
      }
      else
      {
         p = UserBuffer;

         dir_iterator = dir_iterator_create ();

         if (dir_iterator == NULL)
         {
            Status = STATUS_UNSUCCESSFUL;
            DbgPrint ("ERROR : Ext2FsdQueryDirectory : dir_iterator == NULL!");
            __leave;
         }

         ASSERT (dir_iterator);

         while (ext2_list_dir
                (Vcb->ext2, buffer, Fcb->inode->i_size,
                 dir_iterator) == EXT2_STATUS_OK)
         {
            CurrentInodeName.Length =
               (dir_iterator->ext2_dir_entry.name_len + 1) * 2 - sizeof (WCHAR);

            if (CurrentInodeName.Length >= CurrentInodeName.MaximumLength)
            {
               DbgPrint
                  ("ERROR : Ext2FsdQueryDirectory : CurrentInodeName.Length > CurrentInodeName.MaximumLength!");
               Status = STATUS_UNSUCCESSFUL;
               __leave;
            }

            FsdCharToWchar (CurrentInodeName.Buffer,
                            dir_iterator->ext2_dir_entry.name,
                            dir_iterator->ext2_dir_entry.name_len);

            if (index >= Ccb->FileIndex)
            {
               if (offset + MinLength > Length)
               {
                  //UserBuffer too small
#ifdef TRACE
                  DbgPrint ("Ext2FsdQueryDirectory : Buffer too small");
#endif

                  break;
               }

               p = UserBuffer + offset;

               if (Ext2MatchFileName
                   (&Ccb->DirectorySearchPattern, &CurrentInodeName))
               {
                  count++;

                  if (Ext2FillUpInformation
                      (Vcb->ext2, FileInformationClass, UserBuffer + offset,
                       dir_iterator, index, &offset, &Fcb->AnsiFileName,
                       &Fcb->IndexNumber))
                  {

                  }
                  else
                  {
                     DbgPrint
                        ("ERROR : Ext2FsdQueryDirectory : Ext2FillUpInformation failed!");
                     Status = STATUS_UNSUCCESSFUL;
                     __leave;
                  }
               }
               Ccb->FileIndex++;
            }
            else
            {
            }

            index++;

            if (ReturnSingleEntry && (offset > 0))
            {
               break;
            }
         }

         if (offset)
         {
            Status = STATUS_SUCCESS;
         }
         else
         {
            Status = STATUS_NO_SUCH_FILE;
         }
      }
   }
   __finally
   {
      if (p)
      {
         ((PFILE_BOTH_DIR_INFORMATION) p)->NextEntryOffset = 0;
      }

      if (buffer)
      {
         Ext2FsdFreePool (buffer);
      }

      if (dir_iterator)
      {
         dir_iterator_dispose (dir_iterator);
      }

      if (FcbResourceAcquired)
      {
         ExReleaseResourceForThreadLite (&Fcb->MainResource,
                                         ExGetCurrentResourceThread ());
      }

      if (Inode != NULL)
      {
         Ext2FsdFreePool (Inode);
      }

      if (CurrentInodeName.Buffer != NULL)
      {
         Ext2FsdFreePool (CurrentInodeName.Buffer);
      }

      if (!IrpContext->ExceptionInProgress)
      {
         if (Status == STATUS_PENDING)
         {
            Status =
               FsdLockUserBuffer (IrpContext->Irp,
                                  IrpSp->Parameters.QueryDirectory.Length,
                                  IoWriteAccess);

            if (NT_SUCCESS (Status))
            {
               Status = FsdQueueRequest (IrpContext);
            }
            else
            {
               IrpContext->Irp->IoStatus.Status = Status;
               FsdCompleteRequest (IrpContext->Irp, IO_NO_INCREMENT);
               Ext2FsdFreeIrpContext (IrpContext);
            }
         }
         else
         {
            IrpContext->Irp->IoStatus.Information = UsedLength;
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
Ext2FsdNotifyChangeDirectory (IN PFSD_IRP_CONTEXT IrpContext)
{
   PDEVICE_OBJECT DeviceObject = NULL;
   BOOLEAN CompleteRequest;
   NTSTATUS Status = STATUS_UNSUCCESSFUL;
   PFSD_VCB Vcb = NULL;
   PFILE_OBJECT FileObject = NULL;
   PFSD_FCB Fcb = NULL;
   PIRP Irp = NULL;
   PIO_STACK_LOCATION IrpSp = NULL;
   ULONG CompletionFilter;
   BOOLEAN WatchTree;

   __try
   {
      ASSERT (IrpContext);

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


      FileObject = IrpContext->FileObject;
      ASSERT (FileObject);

      Fcb = (PFSD_FCB) FileObject->FsContext;

      ASSERT (Fcb);

      if (Fcb->Identifier.Type == VCB)
      {
         CompleteRequest = TRUE;
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      ASSERT ((Fcb->Identifier.Type == FCB)
              && (Fcb->Identifier.Size == sizeof (FSD_FCB)));

      if (!FlagOn (Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
      {
         CompleteRequest = TRUE;
         Status = STATUS_INVALID_PARAMETER;
         __leave;
      }

      Irp = IrpContext->Irp;

      ASSERT (Irp);

      IrpSp = IoGetCurrentIrpStackLocation (Irp);

      ASSERT (IrpSp);

      CompletionFilter = IrpSp->Parameters.NotifyDirectory.CompletionFilter;

      WatchTree = FlagOn (IrpSp->Flags, SL_WATCH_TREE);

      CompleteRequest = FALSE;

      Status = STATUS_PENDING;

      FsRtlNotifyChangeDirectory (Vcb->NotifySync, FileObject->FsContext2,
                                  &Fcb->AnsiFileName, &Vcb->NotifyList,
                                  WatchTree, CompletionFilter, Irp);

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
