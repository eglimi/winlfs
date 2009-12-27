/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/07/02 15:19:08 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/inode.c,v $
    $Revision: 1.12 $
*/

/*!
 * \file inode.c 
 * \brief Functions for inode operations.
 *
 */

#include "ext2.h"
#include "io.h"

#ifndef DRIVER
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#else
#include "fsd.h"
#endif

#ifdef DRIVER

_u32 ext2_lookup_recursiv (ext2_t * this, _u32 startInode, char *file);
_u32 ext2_read_single_indirect (ext2_t * this, inode_t * inode, _u32 block_num);
_u32 ext2_read_double_indirect (ext2_t * this, inode_t * inode, _u32 block_num);
_u32 ext2_read_tripple_indirect (ext2_t * this, inode_t * inode,
                                 _u32 block_num);
_u32 ext2_read (ext2_t * this, inode_t * inode, _u32 offset);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ext2_load_inode)
#pragma alloc_text(PAGE, ext2_print_inode)
#pragma alloc_text(PAGE, ext2_read_single_indirect)
#pragma alloc_text(PAGE, ext2_read_double_indirect)
#pragma alloc_text(PAGE, ext2_read_tripple_indirect)
#pragma alloc_text(PAGE, ext2_read)
#pragma alloc_text(PAGE, ext2_read_inode)
#pragma alloc_text(PAGE, dir_iterator_create)
#pragma alloc_text(PAGE, dir_iterator_dispose)
#pragma alloc_text(PAGE, ext2_list_dir)
#pragma alloc_text(PAGE, ext2_lookup_recursiv)
#pragma alloc_text(PAGE, ext2_lookup)
#pragma alloc_text(PAGE, ext2_resolve_softlink)
#pragma alloc_text(PAGE, ext2_getAccessTimeNT)
#pragma alloc_text(PAGE, ext2_getCreationTimeNT)
#pragma alloc_text(PAGE, ext2_getDeletionTimeNT)
#pragma alloc_text(PAGE, ext2_getModificationTimeNT)
#pragma alloc_text(PAGE, Ext2FsdNewInode)
#pragma alloc_text(PAGE, Ext2FsdCreateInode)
#pragma alloc_text(PAGE, Ext2FsdAddEntry)
#pragma alloc_text(PAGE, Ext2FsdRemoveEntry)
#pragma alloc_text(PAGE, Ext2FsdDeleteInode)
#pragma alloc_text(PAGE, Ext2FsdSaveGroup)
#pragma alloc_text(PAGE, Ext2FsdSaveInode)
#pragma alloc_text(PAGE, Ext2FsdSaveSuper)
#pragma alloc_text(PAGE, Ext2FsdGetInodeLba)
#endif

#endif

BOOLEAN
ext2_inode_valid (ext2_t * this, _u32 inode_no)
{
   return !(inode_no < 1 || inode_no > this->superblock.s_inodes_count);
}


EXT2_STATUS
ext2_load_inode (ext2_t * this, inode_t * inode, _u32 inode_no)
{
   _u64 offset;
   _u64 bg_inode;
   _u64 blocksize;
   _u64 offsetInode;
   inode_t *cache;

   if (!ext2_inode_valid (this, inode_no))
   {
      DEBUG
         ("ERROR : Inode value %lu was out of range in load_inode. Limit is %ld\n",
          inode_no, this->superblock.s_inodes_count);
      return EXT2_STATUS_ERROR; /* Inode out of range */
   }

   if ((inode_no - 1) / this->superblock.s_inodes_per_group >= this->num_groups)
   {
      DEBUG ("ERROR : ext2_load_inode : invalid group_desc[] offset");
   }

   cache = (inode_t *) cache_find (this->inode_chache, inode_no, TRUE);

   if (cache)
   {
      memcpy (inode, cache, sizeof (inode_t));
   }
   else
   {
      bg_inode.QuadPart =
         this->group_desc[(inode_no - 1) /
                          this->superblock.s_inodes_per_group].bg_inode_table;
      blocksize.QuadPart = this->block_size;
      offsetInode.QuadPart =
         ((inode_no -
           1) % this->superblock.s_inodes_per_group) * sizeof (inode_t);
      offset.QuadPart =
         bg_inode.QuadPart * blocksize.QuadPart + offsetInode.QuadPart;


      if (offset.QuadPart == 0)
      {
         DEBUG ("ERROR : ext2_load_inode : offset == 0\n");
         return EXT2_STATUS_ERROR;
      }

#ifdef TRACE
      DEBUG
         ("ext2_load_inode [1]: inode=%i address=%.8x%.8x bs=%i ipg=%i , bgi=%.8x%.8x",
          inode_no, offset.HighPart, offset.LowPart, this->block_size,
          this->superblock.s_inodes_per_group, bg_inode.HighPart,
          bg_inode.LowPart);
#endif


      if (READDISK (this, (byte *) inode, offset, sizeof (inode_t)) !=
          sizeof (inode_t))
      {
         DEBUG ("ERROR : Disk problem in load_inode.\n");
         return EXT2_STATUS_ERROR;
      }
      cache_add (this->inode_chache, inode_no, inode);
   }


#ifdef TRACE
   DEBUG ("ext2_load_inode [2]: inode=%i size=%i mode=%i ", inode_no,
          inode->i_size, inode->i_mode);
#endif

   return EXT2_STATUS_OK;
}

void
ext2_print_inode (inode_t * i, _u32 inode_num)
{
   _u32 a_time, c_time, m_time, d_time;

   if (i == NULL)
      return;                   /* Early Bailout */

   a_time = i->i_atime + TIMEZONE;
   c_time = i->i_ctime + TIMEZONE;
   m_time = i->i_mtime + TIMEZONE;
   d_time = i->i_dtime;

   DEBUG ("Print Inode");

   if (inode_num)
      DEBUG ("#: %li\n", inode_num);
   else
      DEBUG ("\n");

   DEBUG ("File Mode: %o\n", i->i_mode);
   DEBUG ("Owner UID: %u\n", i->i_uid);
   DEBUG ("Size (bytes): %lu\n", i->i_size);
#ifndef DRIVER
   DEBUG ("Access Time: %s", ctime ((time_t *) & a_time));
   DEBUG ("Creation Time: %s", ctime ((time_t *) & c_time));
   DEBUG ("Modification Time: %s", ctime ((time_t *) & m_time));
   DEBUG ("Deletion Time: %s", ctime ((time_t *) & d_time));
#endif
   DEBUG ("Owner GID: %u\n", i->i_gid);
   DEBUG ("Links Count: %u\n", i->i_links_count);
   DEBUG ("(512 byte)Blocks Count: %lu\n", i->i_blocks);
   DEBUG ("File Flags: 0x%lX\n", i->i_flags);
   DEBUG ("File Version: %lu\n", i->i_version);
   DEBUG ("File ACL: %lu\n", i->i_file_acl);
   DEBUG ("Directory ACL: %lu\n", i->i_dir_acl);
   DEBUG ("Fragment Address: %lu\n", i->i_faddr);
   DEBUG ("Fragment Number: %u\n", i->i_frag);
   DEBUG ("Fragment Size: %u\n", i->i_fsize);
}

_u32
ext2_read_single_indirect (ext2_t * this, inode_t * inode, _u32 block_num)
{
   _u32 b = this->block_size;
   _u32 *block = NULL;
   _u32 result = EXT2_INVALID_BLOCK;
   _u64 offset;

   block = (_u32 *) MALLOC (b);
   ASSERT (block);
   block_num = block_num - 12;

   offset.QuadPart = (ULONGLONG) inode->i_block[12] * (ULONGLONG) b;

   if (READDISK (this, (byte *) block, offset, b) != b)
      DEBUG ("ERROR : Couldn't load block\n");
   else
      result = *(block + block_num);

   FREE (block);

   return result;
}

_u32
ext2_read_double_indirect (ext2_t * this, inode_t * inode, _u32 block_num)
{
   _u32 b = this->block_size;
   _u32 *block = NULL;
   _u32 result = EXT2_INVALID_BLOCK;
   _u32 ind1;
   _u32 ind2;

   _u32 first_inode = b / 4 + 12;
   _u32 last_inode = (b / 4) * (b / 4) + (b / 4) + 12;
   _u64 offset;

   block_num = block_num - first_inode;

   block = MALLOC (b);
   ASSERT (block);

   offset.QuadPart = (ULONGLONG) inode->i_block[13] * (ULONGLONG) b;
   //First indirection
   if (READDISK (this, (byte *) block, offset, b) != b)
   {
      DEBUG ("ERROR : ext2_read_double_indirect [1] : Couldn't load block\n");
      FREE (block);
      return EXT2_INVALID_BLOCK;
   }

   ind1 = block_num / (b / 4);
   ind2 = block_num % (b / 4);

   result = *(block + ind1);

   offset.QuadPart = (ULONGLONG) result *(ULONGLONG) b;

   //Second indirection
   if (READDISK (this, (byte *) block, offset, b) != b)
   {
      DEBUG ("ERROR : ext2_read_double_indirect [2] : Couldn't load block\n");
      FREE (block);
      return EXT2_INVALID_BLOCK;
   }

   result = *(block + ind2);

   FREE (block);

   return result;
}

_u32
ext2_read_tripple_indirect (ext2_t * this, inode_t * inode, _u32 block_num)
{
   _u32 b = this->block_size;
   _u32 *block = NULL;
   _u32 result = EXT2_INVALID_BLOCK;
   _u32 ind1;
   _u32 ind2;
   _u32 ind3;
   _u32 temp;
   _u64 offset;
   _u32 first_inode = (b / 4) * (b / 4) + (b / 4) + 12;

   block_num = block_num - first_inode;

   block = MALLOC (b);
   ASSERT (block);

   offset.QuadPart = (ULONGLONG) inode->i_block[13] * (ULONGLONG) b;
   //First indirection
   if (READDISK (this, (byte *) block, offset, b) != b)
   {
      DEBUG ("ERROR : ext2_read_tripple_indirect [1] : Couldn't load block\n");
      FREE (block);
      return EXT2_INVALID_BLOCK;
   }

   ind1 = block_num / ((b / 4) * (b / 4));
   temp = block_num % ((b / 4) * (b / 4));
   block_num = block_num - temp * ((b / 4) * (b / 4));
   ind2 = (block_num) / (b / 4);
   ind3 = block_num % (b / 4);

   result = *(block + ind1);

   offset.QuadPart = (ULONGLONG) result *(ULONGLONG) b;

   //Second indirection
   if (READDISK (this, (byte *) block, offset, b) != b)
   {
      DEBUG ("ERROR : ext2_read_tripple_indirect [2] : Couldn't load block\n");
      return EXT2_INVALID_BLOCK;
   }

   result = *(block + ind2);

   //Third indirection
   offset.QuadPart = (ULONGLONG) result *(ULONGLONG) b;

   if (READDISK (this, (byte *) block, offset, b) != b)
   {
      DEBUG ("ERROR : ext2_read_tripple_indirect [3] : Couldn't load block\n");
      FREE (block);
      return EXT2_INVALID_BLOCK;
   }

   result = *(block + ind3);

   FREE (block);

   return result;
}


_u32
ext2_read (ext2_t * this, inode_t * inode, _u32 offset)
{
   _u32 b;
   _u32 block_num;

#ifdef TRACE
   DEBUG ("ext2_read [0]: @this=%.8X", this);
#endif

   b = this->block_size;
   block_num = offset / this->block_size;

   if (block_num <= 11)         //0..11
   {
#ifdef TRACE
      DEBUG ("ext2_read [1]: @inode->i_block=%.8X block_num=%i offset=%i",
             inode->i_block, block_num, offset);
#endif
      return inode->i_block[block_num];
   }
   else if (block_num <= (b / 4 + 11))  //12..267 Single Indirection
   {
#ifdef TRACE
      DEBUG ("ext2_read [2]: @inode->i_block=%.8X block_num=%i offset=%i",
             inode->i_block, block_num, offset);
#endif
      return ext2_read_single_indirect (this, inode, block_num);
   }
   else if (block_num <= ((b / 4) * (b / 4) + (b / 4) + 11))    //268..65803 Double Indirection
   {
#ifdef TRACE
      DEBUG ("ext2_read [3]: @inode->i_block=%.8X block_num=%i offset=%i",
             inode->i_block, block_num, offset);
#endif
      return ext2_read_double_indirect (this, inode, block_num);
   }
   else                         //65803..  Tripple Indirection
   {
#ifdef TRACE
      DEBUG ("ext2_read [4]: @inode->i_block=%.8X block_num=%i offset=%i",
             inode->i_block, block_num, offset);
#endif
      return ext2_read_tripple_indirect (this, inode, block_num);
   }

   return 0;
}

EXT2_STATUS
ext2_read_inode (ext2_t * this, inode_t * inode, char *buffer, _u32 offset,
                 _u32 size, _u32 * valid_size)
{
   byte *block = NULL;
   _u32 block_index;
   _u32 offset_in_block;
   _u32 done = 0;
   _u32 size_in_block;
   _u64 offset_in_disk;
   char *cache;
   EXT2_STATUS status = EXT2_STATUS_OK;
   _u32 _offset_at_start;

   if (offset + size > inode->i_size)
   {

#ifdef TRACE
      DEBUG ("ext2_read_inode : offset=%i size=%i inode->i_size=%i", offset,
             size, inode->i_size);
#endif
      if (offset > inode->i_size)
      {
         size = 0;
      }
      else
      {
         size = inode->i_size - offset;
      }

#ifdef TRACE
      DEBUG ("ext2_read_inode : size is now %i", size);
#endif
   }

   block = MALLOC (this->block_size);

   if (block == NULL)
   {
      DEBUG ("ERROR : ext2_read_inode : block == null");
      return EXT2_STATUS_ERROR;
   }

   _offset_at_start = offset;


   while (done < size)
   {
#ifdef TRACE
      DEBUG ("ext2_read_inode : begin of while");
#endif

      block_index = ext2_read (this, inode, offset);

#ifdef TRACE
      DEBUG ("ext2_read_inode : block index=%i \n", block_index);
#endif

      offset_in_block = offset % this->block_size;
      size_in_block = this->block_size - offset_in_block;

      if (size_in_block > size - done)
      {
         size_in_block = size - done;
      }

      cache = (char *) cache_find (this->block_chache, block_index, TRUE);

      if (cache)
      {
         memcpy (block, cache, this->block_size);
      }
      else
      {
         offset_in_disk.QuadPart =
            (ULONGLONG) block_index *(ULONGLONG) this->block_size;

         if (READDISK (this, block, offset_in_disk, this->block_size) !=
             this->block_size)
         {
            DEBUG ("ERROR : ext2_read_inode : couldn't read\n");
            status = EXT2_STATUS_ERROR;
            break;
         }

         cache_add (this->block_chache, block_index, block);
      }


#ifdef TRACE
      DEBUG
         ("ext2_read_inode : memcopy buffer=%.8X done=%X block=%.8X offset_in_block=%i size_in_block=%i",
          buffer, done, block, offset_in_block, size_in_block);
#endif

      memcpy (buffer + done, block + offset_in_block, size_in_block);

#ifdef TRACE
      DEBUG ("ext2_read_inode : memcopy done");
#endif
      offset += size_in_block;
      done += size_in_block;
#ifdef TRACE
      DEBUG ("ext2_read_inode : end of while");
#endif
   }

#ifdef TRACE
   DEBUG ("ext2_read_inode : after while");
#endif

   if (valid_size)
   {
      *valid_size =
         (_offset_at_start + done) >
         inode->i_size ? inode->i_size - _offset_at_start : done;
   }


#ifdef TRACE
   DEBUG ("ext2_read_inode : free block");
#endif

   FREE (block);

#ifdef TRACE
   DEBUG ("ext2_read_inode : return");
#endif

   return status;
}

dir_iterator_t *
dir_iterator_create ()
{
   dir_iterator_t *res = MALLOC (sizeof (dir_iterator_t));

   ASSERT (res);
   memset (res, 0, sizeof (dir_iterator_t));
   return res;
}

void
dir_iterator_dispose (dir_iterator_t * this)
{
   FREE (this);
}

EXT2_STATUS
ext2_list_dir (ext2_t * this, char *buffer, _u32 size,
               dir_iterator_t * dir_iterator)
{
#ifdef TRACE
   DEBUG ("ext2_list_dir : @buffer=%.8X size=%i @dir_iterator=%.8X pos=%i",
          buffer, size, dir_iterator, dir_iterator->position);
#endif

   if (dir_iterator->position >= size
       || dir_iterator->ext2_dir_entry.rec_len > DISK_BLOCK_SIZE)
   {
      return EXT2_STATUS_ERROR;
   }

#ifdef TRACE
   DEBUG ("ext2_list_dir : memcpy with len=8");
#endif

   //First Step : Copy everything except the name (inode (4Byte), rec_len (2Byte). name_len(1Byte), file_type (1Byte))
   memcpy (&dir_iterator->ext2_dir_entry, buffer + dir_iterator->position, 8);

#ifdef TRACE
   DEBUG ("ext2_list_dir : memcpy with len=%i",
          dir_iterator->ext2_dir_entry.name_len);
#endif

   //Second Step : Copy everything
   memcpy (&dir_iterator->ext2_dir_entry, buffer + dir_iterator->position,
           8 + dir_iterator->ext2_dir_entry.name_len);

#ifdef TRACE
   DEBUG ("ext2_list_dir : terminate name");
#endif

   dir_iterator->ext2_dir_entry.name[dir_iterator->ext2_dir_entry.name_len] = 0;


   if (dir_iterator->ext2_dir_entry.rec_len == 0)
   {
      DEBUG ("ERROR : ext2_list_dir : rec_len==0, name=%s inode=%i",
             dir_iterator->ext2_dir_entry.name,
             dir_iterator->ext2_dir_entry.inode);
      return EXT2_STATUS_ERROR;
   }
   else
   {
      dir_iterator->position += dir_iterator->ext2_dir_entry.rec_len;
   }

#ifdef TRACE
   DEBUG ("ext2_list_dir : name = %s return",
          dir_iterator->ext2_dir_entry.name);
#endif

   return EXT2_STATUS_OK;
}

_u32
ext2_lookup_recursiv (ext2_t * this, _u32 startInode, char *file)
{
   inode_t *inode = NULL;
   inode_t *isolve = NULL;
   byte *buffer = NULL;
   dir_iterator_t *iterator = NULL;
   _u32 inode_num = EXT2_INVALID_INODE;
   EXT2_STATUS status;

   inode = Ext2FsdAllocateInode ();
   isolve = Ext2FsdAllocateInode ();

   ASSERT (inode);
   ASSERT (isolve);

   if (ext2_load_inode (this, inode, startInode) == EXT2_STATUS_ERROR)
   {
      DEBUG ("ERROR : ext2_lookup_ : couldn't load inode %i\n", startInode);
      goto end;
   }



   buffer = (byte *) MALLOC (inode->i_size);
   ASSERT (buffer);

#ifdef TRACE
   DEBUG ("ext2_lookup_recursiv : startInode=%i inode->size=%i @buffer=%.8X",
          startInode, inode->i_size, buffer);
#endif

   if (ext2_read_inode (this, inode, buffer, 0, inode->i_size, 0) ==
       EXT2_STATUS_ERROR)
   {
      DEBUG ("ERROR : ext2_lookup_ : couldn't read inode %i\n", startInode);
      goto end;
   }

#ifdef TRACE
   DEBUG ("ext2_lookup_recursiv : read done");
#endif

   iterator = dir_iterator_create ();

   if (iterator == NULL)
   {
      DEBUG ("ERROR : ext2_lookup_recursiv : iterator == null");
      ASSERT (iterator);
   }

   status = ext2_list_dir (this, buffer, inode->i_size, iterator);

   while (status == EXT2_STATUS_OK)
   {
#ifdef TRACE
      DEBUG ("lookup_rekursiv : @iterator=%.8X inode=%i name=%s", iterator,
             iterator->ext2_dir_entry.inode, iterator->ext2_dir_entry.name);
#endif

      if (strcmp (iterator->ext2_dir_entry.name, file) == 0)
      {
         inode_num = iterator->ext2_dir_entry.inode;

         if (iterator->ext2_dir_entry.file_type == EXT2_FT_SYMLINK)
         {
#ifdef TRACE
            DEBUG ("lookup_rekursiv : is link");
#endif
            ext2_load_inode (this, isolve, inode_num);
            inode_num = ext2_resolve_softlink (this, isolve, startInode);
         }

         //Found, leave
#ifdef TRACE
         DEBUG ("lookup_rekursiv : break");
#endif
         break;
      }

      status = ext2_list_dir (this, buffer, inode->i_size, iterator);
   }

#ifdef TRACE
   DEBUG ("lookup_rekursiv : free iterator");
#endif

   dir_iterator_dispose (iterator);

 end:
   FREE (buffer);
   FREE (inode);
   FREE (isolve);

#ifdef TRACE
   DEBUG ("lookup_rekursiv : return");
#endif
   return inode_num;
}

_u32
ext2_lookup (ext2_t * this, char *file, _u32 inode_num)
{
   char *start = NULL;
   char *end = file;
   _u32 inode_found = EXT2_INVALID_INODE;
   char *buffer = NULL;

   buffer = MALLOC (EXT2_NAME_LEN + 1);
   ASSERT (buffer);

   if (file[0] != 0)
   {
      if ((file[0] == '\\' || file[0] == '/') && file[1] == 0)
      {
         inode_found = 2;
         goto end;
      }
   }

   if (file[0] == '\\' || file[0] == '/')
   {
      inode_num = EXT2_ROOT_INODE;
   }

   if (*end == '\\' || *end == '/')
      end++;

   start = end;

#ifdef TRACE
   DEBUG ("lookup [1] : look for %s", file);
#endif

   for (;;)
   {
      if (*end == '\\' || *end == '/' || *end == 0)
      {
         //Parse Path
         if (start != end)
         {
            memcpy (buffer, start, end - start);
            buffer[end - start] = 0;
            start = end + 1;
#ifdef TRACE
            DEBUG ("lookup : @buffer=%.8X buffer=%s", buffer);
#endif
            inode_found = ext2_lookup_recursiv (this, inode_num, buffer);

            if (!ext2_inode_valid (this, inode_found))
            {
#ifdef TRACE
               DEBUG ("lookup [2] : file %s not found", file);
#endif
               goto end;
            }

            inode_num = inode_found;
         }
      }

      if (*end == 0)
      {
         break;
      }
      else
      {
         end++;
      }
   }

 end:
   if (buffer)
   {
      FREE (buffer);
   }

   if (inode_found != EXT2_INVALID_INODE)
   {
#ifdef TRACE
      DEBUG ("lookup [2] : file %s found, inode=%i", file, inode_found);
#endif
   }

   return inode_found;
}


_u32
ext2_resolve_softlink (ext2_t * this, inode_t * inode, _u32 parentInode)
{
   char *buffer = NULL;
   _u32 valid_size = 0;
   _u32 inode_found = EXT2_INVALID_INODE;

   //Read the link
   if (inode->i_blocks == 0)
   {
      buffer = MALLOC (15 * sizeof (_u32) + 1);
      ASSERT (buffer);
      memcpy (buffer, inode->i_block, 15 * sizeof (_u32));
      buffer[15 * sizeof (_u32)] = 0;
   }
   else
   {
      buffer = MALLOC (inode->i_size + 1);
      ASSERT (buffer);

      if (ext2_read_inode (this, inode, buffer, 0, inode->i_size, &valid_size)
          != EXT2_STATUS_OK)
      {
         FREE (buffer);
         return inode_found;
      }

      buffer[inode->i_size] = 0;
   }

#ifdef TRACE
   DEBUG ("ext2_resolve_softlink : looking for %s in starting at inode# %i",
          buffer, parentInode);
#endif

   if (buffer[0] == '/')
      inode_found = ext2_lookup (this, buffer, EXT2_ROOT_INODE);
   else
      inode_found = ext2_lookup (this, buffer, parentInode);

   FREE (buffer);

   return inode_found;
}

/*!\brief Creates new Inode on disk and makes Directory entry.
 *
 * @param IrpContext         Pointer to the context of the IRP
 * @param Vcb                Volume control block
 * @param pParentFcb         FCB of the parent directory
 * @param ParentInodeNumber  Inode number of the parent directory
 * @param Type               Type of entry (file or directory)
 * @param FileName
 *
 * @return If the routine succeeds, it must return STATUS_SUCCESS. 
 *         Otherwise, it must return one of the error status values 
 *         defined in ntstatus.h.
 */
NTSTATUS
Ext2FsdCreateInode (PFSD_IRP_CONTEXT IrpContext, PFSD_VCB Vcb,
                    PFSD_FCB pParentFcb, ULONG ParentInodeNumber, ULONG Type,
                    ULONG FileAttr, PUNICODE_STRING FileName)
{
   ULONG inodeNumber;
   ULONG BlockGroup;

   inode_t inode;

   RtlZeroMemory (&inode, sizeof (inode_t));


   BlockGroup =
      (ParentInodeNumber - 1) / Vcb->ext2->superblock.s_blocks_per_group;

   KdPrint (("Ext2FsdCreateInode: %S in %S(ParentInodeNumber=%xh)\n",
             pParentFcb->FileName.Buffer, FileName->Buffer, ParentInodeNumber));

   if (Ext2FsdNewInode (IrpContext, Vcb, BlockGroup, Type, &inodeNumber))
   {
      if (NT_SUCCESS
          (Ext2FsdAddEntry
           (IrpContext, Vcb, pParentFcb, Type, inodeNumber, FileName)))
      {
         Ext2FsdSaveInode (IrpContext, Vcb, ParentInodeNumber,
                           pParentFcb->inode);

         inode.i_ctime = pParentFcb->inode->i_mtime;
         inode.i_mode = 0x1FF;
         inode.i_links_count = 1;

         if (FlagOn (FileAttr, FILE_ATTRIBUTE_READONLY))
         {
            Ext2FsdSetReadOnly (inode.i_mode);
         }

         if (Type == EXT2_FT_DIR)
         {
            SetFlag (inode.i_mode, S_IFDIR);
            if ((ParentInodeNumber == EXT2_ROOT_INODE)
                && (FileName->Length == 0x10))
            {
               if (memcmp (FileName->Buffer, L"Recycled\0", 0x10) == 0)
               {
                  Ext2FsdSetReadOnly (inode.i_mode);
               }
            }
         }
         else
         {
            SetFlag (inode.i_mode, S_IFFIL);
         }

         Ext2FsdSaveInode (IrpContext, Vcb, inodeNumber, &inode);

         KdPrint (("Ext2CreateInode: New inodeNumber = %xh (Type=%xh)\n",
                   inodeNumber, Type));

         return STATUS_SUCCESS;
      }
      else
      {
         Ext2FsdBreakPoint ();
         Ext2FsdDeleteInode (IrpContext, Vcb, inodeNumber, Type);
      }
   }

   return STATUS_UNSUCCESSFUL;
}

/*!\brief Allocate a new Inode on the partition.
 * 
 * The algoritm of this functions tries first to allocate an inode in the same
 * block group. If there are no more inodes available it searches inodes in 
 * other block groups.
 *
 * @param IrpContext  Pointer to the context of the IRP
 * @param Vcb         Volume control block
 * @param GroupHint   Number of block group where search starts
 * @param Type        Type of entry (file or directory)
 * @param InodeNumber Number of inode
 *
 * @return If the operation finished successfully.
 */
BOOLEAN
Ext2FsdNewInode (PFSD_IRP_CONTEXT IrpContext, PFSD_VCB Vcb, ULONG GroupHint,
                 ULONG Type, PULONG InodeNumber)
{
   RTL_BITMAP InodeBitmap;
   PULONG BitmapCache = NULL;
   PBCB BitmapBcb = NULL;

   ULONG BlockGroup, i, j;
   ULONG Average, Length;
   LARGE_INTEGER Offset;

   ULONG dwInode;
   ULONG maxGroup;
   PFILE_OBJECT StreamObj = NULL;

   StreamObj = IoCreateStreamFileObjectLite (NULL, Vcb->Vpb->RealDevice);

   if (StreamObj)
   {
      StreamObj->SectionObjectPointer = &(Vcb->SectionObject);
      StreamObj->Vpb = Vcb->Vpb;
      StreamObj->ReadAccess = TRUE;
      if (IsFlagOn (Vcb->Flags, VCB_READ_ONLY))
      {
         StreamObj->WriteAccess = TRUE;
         StreamObj->DeleteAccess = TRUE;
      }
      else
      {
         StreamObj->WriteAccess = TRUE;
         StreamObj->DeleteAccess = TRUE;
      }
      StreamObj->FsContext = (PVOID) Vcb;
      StreamObj->FsContext2 = NULL;
      StreamObj->Vpb = Vcb->Vpb;

      SetFlag (StreamObj->Flags, FO_NO_INTERMEDIATE_BUFFERING);
   }

   *InodeNumber = dwInode = 0XFFFFFFFF;

 repeat:

   BlockGroup = 0;
   i = 0;

   if (Type == EXT2_FT_DIR)
   {
      Average =
         Vcb->ext2->superblock.s_free_inodes_count / Vcb->ext2->num_groups;

      maxGroup = Vcb->ext2->num_groups + GroupHint;
      for (j = GroupHint; j < maxGroup; j++)
      {
         i = j % (Vcb->ext2->num_groups);

         if ((Vcb->ext2->group_desc[i].bg_used_dirs_count << 8) <
             Vcb->ext2->group_desc[i].bg_free_inodes_count)
         {
            BlockGroup = i + 1;
            break;
         }
      }

      if (!BlockGroup)
      {
         for (j = 0; j < Vcb->ext2->num_groups; j++)
         {
            if (Vcb->ext2->group_desc[j].bg_free_inodes_count >= Average)
            {
               if (!BlockGroup
                   || (Vcb->ext2->group_desc[j].bg_free_blocks_count >
                       Vcb->ext2->group_desc[BlockGroup].bg_free_blocks_count))
                  BlockGroup = j + 1;
            }
         }
      }
   }
   else
   {
      /*
       * Try to place the inode in its parent directory (GroupHint)
       */
      if (Vcb->ext2->group_desc[GroupHint].bg_free_inodes_count)
      {
         BlockGroup = GroupHint + 1;
      }
      else
      {
         i = GroupHint;

         /*
          * Use a quadratic hash to find a group with a
          * free inode
          */
         for (j = 1; j < Vcb->ext2->num_groups; j <<= 1)
         {

            i += j;
            if (i > Vcb->ext2->num_groups)
               i -= Vcb->ext2->num_groups;

            if (Vcb->ext2->group_desc[i].bg_free_inodes_count)
            {
               BlockGroup = i + 1;
               break;
            }
         }
      }

      if (!BlockGroup)
      {
         /*
          * That failed: try linear search for a free inode
          */
         i = GroupHint + 1;
         for (j = 2; j < Vcb->ext2->num_groups; j++)
         {
            if (++i >= Vcb->ext2->num_groups)
               i = 0;

            if (Vcb->ext2->group_desc[i].bg_free_inodes_count)
            {
               BlockGroup = i + 1;
               break;
            }
         }
      }
   }

   // Could not find a proper group.
   if (!BlockGroup)
   {
      return FALSE;
   }
   else
   {
      BlockGroup--;

      Offset.QuadPart = (LONGLONG) Vcb->ext2->block_chache;
      Offset.QuadPart =
         Offset.QuadPart * Vcb->ext2->group_desc[BlockGroup].bg_inode_bitmap;

      if (Vcb->ext2->num_groups == 1)
      {
         Length = Vcb->ext2->superblock.s_inodes_count;
      }
      else
      {
         if (BlockGroup == Vcb->ext2->num_groups - 1)
            Length =
               Vcb->ext2->superblock.s_inodes_count %
               Vcb->ext2->superblock.s_inodes_per_group;
         else
            Length = Vcb->ext2->superblock.s_inodes_per_group;
      }

      /*if (!CcPinRead( StreamObj,
         &Offset,
         Vcb->ext2->block_size,
         TRUE,
         &BitmapBcb,
         &BitmapCache ) )
         {
         KdPrint(("Ext2NewInode: PinReading error ...\n"));
         return FALSE;
         }
       */

      RtlInitializeBitMap (&InodeBitmap, BitmapCache, Length);

      dwInode = RtlFindClearBits (&InodeBitmap, 1, 0);

      if (dwInode == 0xFFFFFFFF)
      {
         CcUnpinData (BitmapBcb);
         BitmapBcb = NULL;
         BitmapCache = NULL;

         RtlZeroMemory (&InodeBitmap, sizeof (RTL_BITMAP));
      }
   }

   if (dwInode == 0xFFFFFFFF || dwInode >= Length)
   {
      if (Vcb->ext2->group_desc[BlockGroup].bg_free_inodes_count != 0)
      {
         Vcb->ext2->group_desc[BlockGroup].bg_free_inodes_count = 0;

         Ext2FsdSaveGroup (IrpContext, Vcb);
      }

      goto repeat;
   }
   else
   {
      RtlSetBits (&InodeBitmap, dwInode, 1);

      /* FIXME:
         CcSetDirtyPinnedData(BitmapBcb, NULL );
       */

      /* FIXME: What does it?
         Ext2RepinBcb(IrpContext, BitmapBcb);
       */

      CcUnpinData (BitmapBcb);

      *InodeNumber =
         dwInode + 1 + (BlockGroup * Vcb->ext2->superblock.s_inodes_per_group);

      //Updating BlockGroup Desc / Superblock
      Vcb->ext2->group_desc[BlockGroup].bg_free_inodes_count--;
      if (Type == EXT2_FT_DIR)
         Vcb->ext2->group_desc[BlockGroup].bg_used_dirs_count++;

      Ext2FsdSaveGroup (IrpContext, Vcb);

      Vcb->ext2->superblock.s_free_inodes_count--;
      Ext2FsdSaveSuper (IrpContext, Vcb);

      return TRUE;
   }

   return FALSE;
}

/*!\brief Add a new directory entry.
 * 
 * @param IrpContext  Pointer to the context of the IRP
 * @param Vcb         Volume control block
 * @param Dcb         Directory control block
 * @param FileType    Type of entry (file or directory)
 * @param InodeNumber Number of inode
 * @param FileName    String with name of the new entry
 *
 * @return If the routine succeeds, it must return STATUS_SUCCESS. 
 *         Otherwise, it must return one of the error status values 
 *         defined in ntstatus.h.
 */
NTSTATUS
Ext2FsdAddEntry (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                 IN PFSD_FCB Dcb, IN ULONG FileType, IN ULONG InodeNumber,
                 IN PUNICODE_STRING FileName)
{
   NTSTATUS Status = STATUS_UNSUCCESSFUL;

   pext2_dir_entry_t pDir = NULL;
   pext2_dir_entry_t pNewDir = NULL;
   pext2_dir_entry_t pTarget = NULL;

   ULONG Length = 0;
   ULONG dwBytes = 0;
   BOOLEAN bFound = FALSE;
   BOOLEAN bAdding = FALSE;

   BOOLEAN MainResourceAcquired = FALSE;

   ULONG dwRet;

   if (!IsFlagOn (Dcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
   {
      Status = STATUS_INVALID_PARAMETER;
      return Status;
   }

   MainResourceAcquired =
      ExAcquireResourceExclusiveLite (&Dcb->MainResource, TRUE);

   __try
   {
      ExAcquireResourceExclusiveLite (&Dcb->MainResource, TRUE);
      Dcb->ReferenceCount++;
      ExReleaseResourceForThreadLite (&Dcb->MainResource,
                                      ExGetCurrentResourceThread ());

      pDir =
         (pext2_dir_entry_t) ExAllocatePool (PagedPool,
                                             EXT2_DIR_REC_LEN (EXT2_NAME_LEN));
      if (!pDir)
      {
         Status = STATUS_INSUFFICIENT_RESOURCES;
         __leave;
      }

      pTarget =
         (pext2_dir_entry_t) ExAllocatePool (PagedPool,
                                             2 *
                                             EXT2_DIR_REC_LEN (EXT2_NAME_LEN));
      if (!pTarget)
      {
         Status = STATUS_INSUFFICIENT_RESOURCES;
         __leave;
      }

      if (IsFlagOn
          (Vcb->ext2->superblock.s_feature_incompat,
           EXT2_FEATURE_INCOMPAT_FILETYPE))
      {
         pDir->file_type = (UCHAR) FileType;
      }
      else
      {
         pDir->file_type = 0;
      }

      pDir->inode = InodeNumber;
      pDir->name_len =
         (UCHAR) (FileName->Length >
                  (EXT2_NAME_LEN * 2) ? EXT2_NAME_LEN : (FileName->Length / 2));

      FsdWcharToChar (pDir->name, FileName->Buffer, pDir->name_len);

      pDir->rec_len = (USHORT) (EXT2_DIR_REC_LEN (pDir->name_len));

      dwBytes = 0;

//Repeat:

      while ((LONGLONG) dwBytes < Dcb->CommonFCBHeader.AllocationSize.QuadPart)
      {
         RtlZeroMemory (pTarget, EXT2_DIR_REC_LEN (EXT2_NAME_LEN));

         /* FIXME: 
            // Reading the DCB contents
            Status = Ext2FsdReadInode(
            NULL,
            Vcb,
            Dcb->ext2_inode,
            dwBytes,
            (PVOID)pTarget,
            EXT2_DIR_REC_LEN(EXT2_NAME_LEN),
            &dwRet);

            if (!NT_SUCCESS(Status))
            {
            KdPrint(("Ext2AddDirectory: Reading Directory Content error.\n"));
            __leave;
            }
          */

         if ((pTarget->inode == 0)
             || (pTarget->rec_len >=
                 EXT2_DIR_REC_LEN (pTarget->name_len) + pDir->rec_len))
         {
            if (pTarget->inode)
            {
               RtlZeroMemory (pTarget, 2 * EXT2_DIR_REC_LEN (EXT2_NAME_LEN));

               /* FIXME: 
                  // Reading the DCB contents
                  Status = Ext2ReadInode(
                  NULL,
                  Vcb,
                  Dcb->ext2_inode,
                  dwBytes,
                  (PVOID)pTarget,
                  2 * EXT2_DIR_REC_LEN(EXT2_NAME_LEN),
                  &dwRet);

                  if (!NT_SUCCESS(Status))
                  {
                  KdPrint(("Ext2AddDirectory: Reading Directory Content error.\n"));
                  __leave;
                  }
                */

               Length = EXT2_DIR_REC_LEN (pTarget->name_len);

               pNewDir =
                  (pext2_dir_entry_t) ((PUCHAR) pTarget +
                                       EXT2_DIR_REC_LEN (pTarget->name_len));
               pNewDir->rec_len =
                  pTarget->rec_len - EXT2_DIR_REC_LEN (pTarget->name_len);

               pTarget->rec_len = EXT2_DIR_REC_LEN (pTarget->name_len);
            }
            else
            {
               pNewDir = pTarget;
               pNewDir->rec_len =
                  (USHORT) ((ULONG)
                            (Dcb->CommonFCBHeader.AllocationSize.QuadPart) -
                            dwBytes);
            }

            pNewDir->file_type = pDir->file_type;
            pNewDir->inode = pDir->inode;
            pNewDir->name_len = pDir->name_len;
            memcpy (pNewDir->name, pDir->name, pDir->name_len);
            Length += EXT2_DIR_REC_LEN (pDir->name_len);

            bFound = TRUE;
            break;
         }

         dwBytes += pTarget->rec_len;
      }

      if (bFound)               // Here we fininish the searching journel ...
      {
         ULONG dwRet;

         if ((!((pDir->name_len == 1) && (pDir->name[0] == '.')))
             &&
             (!((pDir->name_len == 2) && (pDir->name[0] == '.')
                && (pDir->name[1] == '.'))))
            Dcb->inode->i_links_count++;

         // FIXME: 
         //if (Ext2FsdWriteInode(IrpContext, Vcb, Dcb->inode, dwBytes, pTarget, Length, FALSE, &dwRet))
         Status = STATUS_SUCCESS;
      }
      else
      {                         /*
                                   // We should expand the size of the dir inode 
                                   if (!bAdding)
                                   {
                                   ULONG dwRet;

                                   bAdding = Ext2ExpandInode(IrpContext, Vcb, Dcb, &dwRet);

                                   if (bAdding) {

                                   Dcb->ext2_inode->i_size = Dcb->ext2_inode->i_blocks * SECTOR_SIZE;

                                   Ext2SaveInode(IrpContext, Vcb, Dcb->Ext2Mcb->Inode, Dcb->ext2_inode);

                                   Dcb->CommonFCBHeader.FileSize = Dcb->CommonFCBHeader.AllocationSize;

                                   goto Repeat;
                                   }

                                   __leave;

                                   }
                                   else  // Something must be error!
                                   {
                                   __leave;
                                   } */
         __leave;
      }
   }
   __finally
   {

      ExAcquireResourceExclusiveLite (&Dcb->MainResource, TRUE);
      Dcb->ReferenceCount--;
      ExReleaseResourceForThreadLite (&Dcb->MainResource,
                                      ExGetCurrentResourceThread ());

      if (MainResourceAcquired)
         ExReleaseResourceForThreadLite (&Dcb->MainResource,
                                         ExGetCurrentResourceThread ());

      if (pTarget != NULL)
      {
         ExFreePool (pTarget);
      }

      if (pDir)
         ExFreePool (pDir);
   }

   return Status;
}

/*!\brief Free an inode on the disk.
 * 
 * @param IrpContext Pointer to the context of the IRP
 * @param Vcb        Volume control block
 * @param Inode      Number of inode
 * @param Type       Type of entry (file or directory)
 *
 * @return If the operation finished successfully.
 */
BOOLEAN
Ext2FsdDeleteInode (PFSD_IRP_CONTEXT IrpContext, PFSD_VCB Vcb, ULONG Inode,
                    ULONG Type)
{
   RTL_BITMAP InodeBitmap;
   PVOID BitmapCache = NULL;
   PBCB BitmapBcb = NULL;

   ULONG BlockGroup;
   ULONG Length;
   LARGE_INTEGER Offset;

   ULONG dwIno;
   BOOLEAN bModified = FALSE;
   PFILE_OBJECT StreamObj;

   StreamObj = IoCreateStreamFileObjectLite (NULL, IrpContext->DeviceObject);


   BlockGroup = (Inode - 1) / Vcb->ext2->superblock.s_inodes_per_group;
   dwIno = (Inode - 1) % (Vcb->ext2->superblock.s_inodes_per_group);

   KdPrint (("Ext2FreeInode: Inode: %xh (BlockGroup/Off = %xh/%xh)\n", Inode,
             BlockGroup, dwIno));

   {
      Offset.QuadPart = (LONGLONG) Vcb->ext2->block_size;
      Offset.QuadPart =
         Offset.QuadPart * Vcb->ext2->group_desc[BlockGroup].bg_inode_bitmap;
      if (BlockGroup == Vcb->ext2->num_groups - 1)
      {
         Length =
            Vcb->ext2->superblock.s_inodes_count %
            Vcb->ext2->superblock.s_inodes_per_group;
      }
      else
      {
         Length = Vcb->ext2->superblock.s_inodes_per_group;
      }

      if (!CcPinRead
          (StreamObj, &Offset, Vcb->ext2->block_size, TRUE, &BitmapBcb,
           &BitmapCache))
      {
         KdPrint (("Ext2FreeInode: PinReading error ...\n"));
         return FALSE;
      }

      RtlInitializeBitMap (&InodeBitmap, BitmapCache, Length);

      if (RtlCheckBit (&InodeBitmap, dwIno) == 0)
      {

      }
      else
      {
         RtlClearBits (&InodeBitmap, dwIno, 1);
         bModified = TRUE;
      }

      if (!bModified)
      {
         CcUnpinData (BitmapBcb);
         BitmapBcb = NULL;
         BitmapCache = NULL;

         RtlZeroMemory (&InodeBitmap, sizeof (RTL_BITMAP));
      }
   }

   if (bModified)
   {
      CcSetDirtyPinnedData (BitmapBcb, NULL);

      /* FIXME: Do we have to do this?
         Ext2RepinBcb(IrpContext, BitmapBcb);
       */

      CcUnpinData (BitmapBcb);

      /* FIXME: Do we have to do something similar?
         Ext2AddMcbEntry(Vcb, Offset.QuadPart, (LONGLONG)Vcb->ext2->block_size);
       */

      //Updating BlockGroup Desc / Superblock
      if (Type == EXT2_FT_DIR)
         Vcb->ext2->group_desc[BlockGroup].bg_used_dirs_count--;

      Vcb->ext2->group_desc[BlockGroup].bg_free_inodes_count++;
      Ext2FsdSaveGroup (IrpContext, Vcb);

      Vcb->ext2->superblock.s_free_inodes_count++;
      Ext2FsdSaveSuper (IrpContext, Vcb);

      return TRUE;
   }

   return FALSE;
}

/*!\brief Remove a directory entry.
 * 
 * @param IrpContext Pointer to the context of the IRP
 * @param Vcb        Volume control block
 * @param Dcb        Directory control block
 * @param Inode      Number of inode
 *
 * @return If the routine succeeds, it must return STATUS_SUCCESS. 
 *         Otherwise, it must return one of the error status values 
 *         defined in ntstatus.h.
 */
NTSTATUS
Ext2FsdRemoveEntry (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                    IN PFSD_FCB Dcb, IN ULONG Inode)
{
   NTSTATUS Status = STATUS_UNSUCCESSFUL;

   pext2_dir_entry_t pTarget = NULL;
   pext2_dir_entry_t pPrevDir = NULL;

   USHORT PrevRecLen;

   ULONG Length = 0;
   ULONG dwBytes = 0;

   BOOLEAN bRet = FALSE;
   BOOLEAN MainResourceAcquired = FALSE;

   ULONG dwRet;

   if (!IsFlagOn (Dcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
      return FALSE;

   MainResourceAcquired =
      ExAcquireResourceExclusiveLite (&Dcb->MainResource, TRUE);


   __try
   {

      ExAcquireResourceExclusiveLite (&Dcb->MainResource, TRUE);
      Dcb->ReferenceCount++;
      ExReleaseResourceForThreadLite (&Dcb->MainResource,
                                      ExGetCurrentResourceThread ());

      pTarget =
         (pext2_dir_entry_t) ExAllocatePool (PagedPool,
                                             EXT2_DIR_REC_LEN (EXT2_NAME_LEN));
      if (!pTarget)
      {
         Status = STATUS_INSUFFICIENT_RESOURCES;
         __leave;
      }

      pPrevDir =
         (pext2_dir_entry_t) ExAllocatePool (PagedPool,
                                             EXT2_DIR_REC_LEN (EXT2_NAME_LEN));
      if (!pPrevDir)
      {
         Status = STATUS_INSUFFICIENT_RESOURCES;
         __leave;
      }

      dwBytes = 0;


      while ((LONGLONG) dwBytes < Dcb->CommonFCBHeader.AllocationSize.QuadPart)
      {
         RtlZeroMemory (pTarget, EXT2_DIR_REC_LEN (EXT2_NAME_LEN));

         /* FIXME: 
            Status = Ext2ReadInode(
            NULL,
            Vcb,
            Dcb->ext2_inode,
            dwBytes,
            (PVOID)pTarget,
            EXT2_DIR_REC_LEN(EXT2_NAME_LEN),
            &dwRet);

            if (!NT_SUCCESS(Status))
            {
            KdPrint(("Ext2RemoveEntry: Reading Directory Content error.\n"));
            __leave;
            }
          */

         if (pTarget->inode == Inode)
         {
            ULONG dwRet;
            ULONG RecLen;

            pPrevDir->rec_len += pTarget->rec_len;
            RecLen = EXT2_DIR_REC_LEN (pTarget->name_len);

            RtlZeroMemory (pTarget, RecLen);

            // FIXME: 
            //Ext2FsdWriteInode(IrpContext, Vcb, Dcb->ext2_inode, dwBytes - PrevRecLen, pPrevDir, 8, FALSE, &dwRet);
            //Ext2FsdWriteInode(IrpContext, Vcb, Dcb->ext2_inode, dwBytes, pTarget, RecLen, FALSE, &dwRet);

            // . / .. could not be removed.
            Dcb->inode->i_links_count--;

            Ext2FsdSaveInode (IrpContext, Vcb, Dcb->IndexNumber, Dcb->inode);

            bRet = TRUE;

            break;
         }
         else
         {
            RtlCopyMemory (pPrevDir, pTarget, EXT2_DIR_REC_LEN (EXT2_NAME_LEN));
            PrevRecLen = pTarget->rec_len;
         }

         dwBytes += pTarget->rec_len;
      }
   }

   __finally
   {

      ExAcquireResourceExclusiveLite (&Dcb->MainResource, TRUE);
      Dcb->ReferenceCount--;
      ExReleaseResourceForThreadLite (&Dcb->MainResource,
                                      ExGetCurrentResourceThread ());


      if (MainResourceAcquired)
         ExReleaseResourceForThreadLite (&Dcb->MainResource,
                                         ExGetCurrentResourceThread ());


      if (pTarget != NULL)
      {
         ExFreePool (pTarget);
      }

      if (pPrevDir)
         ExFreePool (pPrevDir);
   }

   return bRet;
}

/*!\brief Write group descriptor to the disk.
 * 
 * @param IrpContext Pointer to the context of the IRP
 * @param Vcb        Volume control block
 *
 * @return If the operation finished successfully.
 */
BOOLEAN
Ext2FsdSaveGroup (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb)
{
   LONGLONG Offset;
   ULONG Len;

   if (Vcb->ext2->block_size == EXT2_MIN_BLOCK)
   {

      Offset = (LONGLONG) (2 * Vcb->ext2->block_size);

   }
   else
   {

      Offset = (LONGLONG) (Vcb->ext2->block_size);
   }

   Len = (ULONG) (sizeof (group_desc_t) * Vcb->ext2->num_groups);

   // FIXME: 
   return TRUE;
   //return Ext2FsdSaveBuffer(IrpContext, Vcb, Offset, Len, Vcb->ext2->group_desc);
}

/*!\brief Write inode to the disk.
 * 
 * @param IrpContext Pointer to the context of the IRP
 * @param Vcb        Volume control block
 * @param Inode      Number of inode
 * @param inode_t    Pointer to the inode struct which has to be saved
 *
 * @return If the operation finished successfully.
 */
BOOLEAN
Ext2FsdSaveInode (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb,
                  IN ULONG Inode, IN inode_t * inode)
{
   LONGLONG Offset = 0;
   LARGE_INTEGER CurrentTime;

   KeQuerySystemTime (&CurrentTime);
   ext2_getModificationTimeNT (inode, &CurrentTime);
#if 0
   KdPrint (("Ext2SaveInode: Saving Inode %xh: Mode=%xh Size=%xh\n", Inode,
             inode->i_mode, inode->i_size));
#endif

   if (!Ext2FsdGetInodeLba (Vcb, Inode, &Offset))
   {
      KdPrint (("Ext2SaveInode: error get inode(%xh)'s addr.\n", Inode));
      return FALSE;
   }

   // FIXME: 
   return TRUE;
   //return Ext2FsdSaveBuffer(IrpContext, Vcb, Offset, sizeof(EXT2_INODE), inode);
}

/*!\brief Write super block to the disk.
 * 
 * @param IrpContext Pointer to the context of the IRP
 * @param Vcb        Volume control block
 *
 * @return If the operation finished successfully.
 */
BOOLEAN
Ext2FsdSaveSuper (IN PFSD_IRP_CONTEXT IrpContext, IN PFSD_VCB Vcb)
{
   LONGLONG Offset;

   Offset = (LONGLONG) (2 * DISK_BLOCK_SIZE);

   // FIXME: 
   return TRUE;
   //return Ext2FsdSaveBuffer(IrpContext, Vcb, Offset, (2 * SECTOR_SIZE), Vcb->ext2_super_block);
}

/*!\brief Get offset on physical partition.
 *
 * @param vcb    Volume control block
 * @param inode  Number of inode
 * @param offset Offset on physical partition
 *
 * @return If the operation finished successfully.
 */
BOOLEAN
Ext2FsdGetInodeLba (IN PFSD_VCB vcb, IN ULONG inode, OUT PLONGLONG offset)
{
   LONGLONG loc;

   if (inode < 1 || inode > vcb->ext2->superblock.s_inodes_count)
   {
      KdPrint (("Ext2GetInodeLba: Inode value %xh is invalid.\n", inode));
      *offset = 0;
      return FALSE;
   }

   loc =
      (vcb->ext2->
       group_desc[(inode -
                   1) /
                  vcb->ext2->superblock.s_inodes_per_group].bg_inode_table);
   loc = loc * vcb->ext2->block_size;
   loc =
      loc +
      ((inode -
        1) % (vcb->ext2->superblock.s_inodes_per_group)) * sizeof (inode_t);

   *offset = loc;

//  KdPrint(("Ext2GetInodeLba: inode=%xh lba=%xh offset=%xh\n",
//      inode, *lba, *offset));
   return TRUE;
}

#ifdef DRIVER

void
ext2_getAccessTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT)
{
   RtlSecondsSince1970ToTime (inode->i_atime, TimeNT);
}

void
ext2_getCreationTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT)
{
   RtlSecondsSince1970ToTime (inode->i_ctime, TimeNT);
}

void
ext2_getDeletionTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT)
{
   RtlSecondsSince1970ToTime (inode->i_dtime, TimeNT);
}

void
ext2_getModificationTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT)
{
   RtlSecondsSince1970ToTime (inode->i_mtime, TimeNT);
}

#endif
