
/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Functions for caching inodes and blocks. This functions work
               only, if the file system driver is read only!

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/07/02 08:34:48 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/cache.c,v $
    $Revision: 1.6 $
*/

/*!\file cache.c
 * \brief Implements caching functions.
 * 
 * <em>This file was not modified during this thesis.</em>
 */

#include "ext2.h"
#include "io.h"

cache_t *
cache_create (_u32 entries, _u32 size_of_entry)
{
   cache_t *result;
   cache_header_t *header;
   _u32 i;

   result = MALLOC (sizeof (cache_t));

   if (result == NULL)
      return NULL;

   result->cache = MALLOC (entries * sizeof (cache_header_t));

   if (result->cache == NULL)
   {
      FREE (result);
      return NULL;
   }

   result->counter = 0;
   result->entries = entries;
   result->size_of_entry = size_of_entry;

   memset (result->cache, 0, entries * sizeof (cache_header_t));

   header = (cache_header_t *) result->cache;

   for (i = 0; i < entries; i++)
   {
      header[i].data = MALLOC (size_of_entry);
   }

   return result;
}

cache_header_t *
cache_get_oldest (cache_t * this)
{
   _u32 i;
   cache_header_t *header;
   cache_header_t *headerFound = NULL;
   ULONGLONG oldest = 0xFFFFFFFFFFFFFFFF;

   header = (cache_header_t *) this->cache;

   for (i = 0; i < this->entries; i++)
   {
      if (header[i].age == 0)
      {
         headerFound = &header[i];
         break;
      }

      if (header[i].age < oldest)
      {
         oldest = header[i].age;
         headerFound = &header[i];
      }

   }

   if (headerFound == NULL)
   {
      headerFound = &header[0];
   }

   return headerFound;
}

void
cache_add (cache_t * this, _u32 index, void *data)
{
   cache_header_t *header;

   this->counter++;

   header = cache_find (this, index, FALSE);

   if (header)
   {
   }
   else
   {
      header = cache_get_oldest (this);
      header->index = index;
      memcpy (header->data, data, this->size_of_entry);
   }

   header->age = this->counter;
}

void *
cache_find (cache_t * this, _u32 index, BOOLEAN aging)
{
   void *data = NULL;
   _u32 i;
   cache_header_t *header;

   header = (cache_header_t *) this->cache;

   for (i = 0; i < this->entries; i++)
   {
      if (header[i].index == index)
      {
         if (aging)
         {
            header[i].age = this->counter;
         }

         data = header[i].data;
         break;
      }
   }

   return data;
}
