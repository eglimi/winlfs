/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions to handle Ext2 partitions

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/07/02 10:48:51 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/ext2.c,v $
    $Revision: 1.9 $
*/

/*!\file ext2.c
 * \brief Implements functions to handle Ext2 partitions.
 * 
 * <em>This file was not modified during this thesis.</em>
 */

#include "ext2.h"
#include "fsd.h"
#include "io.h"

#ifndef DRIVER
#include "stdio.h"
#include "time.h"
#endif

#ifdef DRIVER
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ext2_create)
#pragma alloc_text(PAGE, ext2_init)
#pragma alloc_text(PAGE, ext2_dispose)
#endif
#endif


ext2_t *
ext2_create (void)
{
   ext2_t *res = MALLOC (sizeof (ext2_t));

   ASSERT (res);

   return res;
}

EXT2_STATUS
ext2_init (ext2_t * this)
{
   if (ext2_load_super (this) != EXT2_STATUS_OK)
   {
      DEBUG ("ext2_init : couldn't load super\n");
      return EXT2_STATUS_ERROR;
   }

   if (this->superblock.s_magic != EXT2_SUPER_MAGIC)
   {
      DEBUG ("ext2_init : wrong magic number\n");
      return EXT2_STATUS_ERROR;
   }

   if (ext2_load_groups (this) != EXT2_STATUS_OK)
   {
      DEBUG ("ext2_init : couldn't load groups\n");
      return EXT2_STATUS_ERROR;
   }

   this->inode_chache = cache_create (100, sizeof (inode_t));
   ASSERT (this->inode_chache);

   this->block_chache = cache_create (50, this->block_size);
   ASSERT (this->block_chache);


   return EXT2_STATUS_OK;
}

void
ext2_dispose (ext2_t * this)
{
   FREE (this);
   //TODO
   //Free groups !!!
   //Free caches !!!
}
