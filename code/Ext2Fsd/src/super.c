/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle the ext2 superblock

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/07/02 10:48:51 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/super.c,v $
    $Revision: 1.7 $
*/

/*!\file super.c
 * \brief Implements functions to handle the ext2 superblock.
 * 
 * <em>This file was not modified during this thesis.</em>
 */

#include "ext2.h"
#include "io.h"

#ifndef DRIVER
#include "stdio.h"
#include "time.h"
#endif

#ifdef DRIVER
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ext2_load_super)
#pragma alloc_text(PAGE, ext2_print_super)
#endif
#endif


EXT2_STATUS
ext2_load_super (ext2_t * this)
{
   _u64 offset;

   DEBUG ("Executing 'load_super'\n");

   offset.QuadPart = 1024;

   if (READDISK
       (this, (byte *) & this->superblock, offset,
        sizeof (super_block_t)) != sizeof (super_block_t))
   {
      DEBUG ("Drive unreadable.\n");

      return EXT2_STATUS_ERROR; /* read failed */
   }


   this->block_size = EXT2_MIN_BLOCK << this->superblock.s_log_block_size;

   return EXT2_STATUS_OK;       /* everything OK */
}



/* print_super

 * print out a super block (good for debugging)
 */
void
ext2_print_super (ext2_t * this)
{
   DEBUG ("Inode Count: %lu\n", this->superblock.s_inodes_count);
   DEBUG ("Block Count: %lu\n", this->superblock.s_blocks_count);
   DEBUG ("Reserved Block Count: %lu\n", this->superblock.s_r_blocks_count);
   DEBUG ("Free Blocks: %lu\n", this->superblock.s_free_blocks_count);
   DEBUG ("Free Inodes: %lu\n", this->superblock.s_free_inodes_count);
   DEBUG ("First Data Block: %lu\n", this->superblock.s_first_data_block);
   DEBUG ("Log Block Size: %lu\n", this->superblock.s_log_block_size);
   DEBUG ("Abs.Block Size: %u\n", this->block_size);
//    DEBUG("Log Frag Size: %ld\n", this->superblock.s_log_frag_size);
   DEBUG ("Blocks per Group: %lu\n", this->superblock.s_blocks_per_group);
   DEBUG ("Fragments per Group: %lu\n", this->superblock.s_frags_per_group);
   DEBUG ("Inodes per Group: %lu\n", this->superblock.s_inodes_per_group);
//    DEBUG("Mount Time: %s", ctime((time_t *) & (this->superblock.s_mtime)));
   //DEBUG("Write Time: %s", ctime((time_t *) & (this->superblock.s_wtime)));
   DEBUG ("Mount Count: %u\n", this->superblock.s_mnt_count);
   DEBUG ("Max Mount Count: %d\n", this->superblock.s_max_mnt_count);
   DEBUG ("Magic Number: %X  (%s)\n", this->superblock.s_magic,
          this->superblock.s_magic == EXT2_SUPER_MAGIC ? "OK" : "BAD");
   DEBUG ("File System State: %X\n", this->superblock.s_state);
   DEBUG ("Error Behaviour: %X\n", this->superblock.s_errors);
   DEBUG ("Minor Revision Level: %u\n", this->superblock.s_minor_rev_level);
//    DEBUG("Last Check: %s", ctime((time_t *) & (this->superblock.s_lastcheck)));
   DEBUG ("Check Interval: %lu\n", this->superblock.s_checkinterval);
   DEBUG ("Creator OS: %lu\n", this->superblock.s_creator_os);
   DEBUG ("Revision Level: %lu\n", this->superblock.s_rev_level);
   DEBUG ("Reserved Block Default UID: %u\n", this->superblock.s_def_resuid);
   DEBUG ("Reserved Block Default GID: %u\n", this->superblock.s_def_resgid);
   DEBUG ("First Inode:              %lu\n", this->superblock.s_first_ino);
   DEBUG ("Inode Size:               %u\n", this->superblock.s_inode_size);
   DEBUG ("Block Group No:	      %u\n", this->superblock.s_block_group_nr);
   DEBUG ("Compatible Feature Set:   %lX\n", this->superblock.s_feature_compat);
   DEBUG ("Incompatible Feature Set: %lX\n",
          this->superblock.s_feature_incompat);
   DEBUG ("Read Only Feature Set:    %lX\n",
          this->superblock.s_feature_ro_compat);
   DEBUG ("An' a bunch of padding we ain't gonna show ya!\n");
}
