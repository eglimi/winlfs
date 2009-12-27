/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle Group Descriptors

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/07/02 10:48:51 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/group.c,v $
    $Revision: 1.7 $
*/

/*!\file group.c
 * \brief Implements functions to handle group descriptors.
 * 
 * <em>This file was not modified during this thesis.</em>
 */

#include "ext2.h"
#include "io.h"

#ifndef DRIVER
#include <stdio.h>
#endif

#ifdef DRIVER
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ext2_load_groups)
#pragma alloc_text(PAGE, ext2_print_groups)
#endif
#endif


EXT2_STATUS
ext2_load_groups (ext2_t * this)
{
   _u32 size;
   _u64 offset;

   DEBUG ("Executing 'load_groups'\n");

   /* How many groups are there? */
   /* Funky math insures integer divide will round up */
   if (this->superblock.s_blocks_per_group == 0)
   {
      /* error checking */
      return EXT2_STATUS_ERROR;
   }

   this->num_groups =
      (this->superblock.s_blocks_count - this->superblock.s_first_data_block +
       this->superblock.s_blocks_per_group -
       1) / this->superblock.s_blocks_per_group;

   size = sizeof (group_desc_t) * this->num_groups;

   if ((this->group_desc = (group_desc_t *) MALLOC (size)) == NULL)
   {
      /* Memory problem, signal failure */
      ASSERT (this->group_desc);
      DEBUG ("Memory problem in load_groups.\n");
      return EXT2_STATUS_ERROR;
   }

   if (this->block_size == 1024)
   {
      offset.QuadPart = sizeof (super_block_t) + 1024;
   }
   else
   {
      offset.QuadPart = 0x1000; //sizeof(super_block_t)+1024;
   }


   DEBUG ("ext2_load_groups : ng=%i size=%i", this->num_groups, size);

   if (READDISK (this, (byte *) this->group_desc, offset, size) != size)
   {
      /* Disk problem, signal failure */
      DEBUG ("Disk problem in load_groups.\n");
      return EXT2_STATUS_ERROR;
   }

   return EXT2_STATUS_OK;       /* Otherwise everything is OK */
}


/* print_groups

 * print out group information (useful for debugging)
 */
void
ext2_print_groups (ext2_t * this)
{
   _u32 i;
   _u32 first_block, last_block, first_inode, last_inode;

   for (i = 0; i < this->num_groups; i++)
   {
      first_block = i * this->superblock.s_blocks_per_group + 1;
      last_block = (i + 1) * this->superblock.s_blocks_per_group;
      first_inode = i * this->superblock.s_inodes_per_group + 1;
      last_inode = (i + 1) * this->superblock.s_inodes_per_group;

      DEBUG ("----------------------------\n");
      DEBUG ("Group Number: %d    Blocks: %ld-%ld   Inodes:%ld-%ld\n", i,
             first_block, last_block, first_inode, last_inode);
      DEBUG ("Blocks Bitmap Block: %lu\n", this->group_desc[i].bg_block_bitmap);
      DEBUG ("Inodes Bitmap Block: %lu\n", this->group_desc[i].bg_inode_bitmap);
      DEBUG ("Inodes Table Block: %lu\n", this->group_desc[i].bg_inode_table);
      DEBUG ("Free Blocks: %u\n", this->group_desc[i].bg_free_blocks_count);
      DEBUG ("Free Inodes: %u\n", this->group_desc[i].bg_free_inodes_count);
      DEBUG ("Used Directories: %u\n", this->group_desc[i].bg_used_dirs_count);
   }
}
