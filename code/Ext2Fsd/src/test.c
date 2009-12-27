/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: User mode test application. Please note, that this application
               needs a ext2 partition dump. You have to specify in io.c where
               this dump is

  Not modified during this thesis.

  CVS Information:
    $Date: 2003/07/02 10:48:51 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/test.c,v $
    $Revision: 1.6 $
*/

#include "io.h"
#include "ext2.h"
#include "stdio.h"
#include "stdlib.h"
#include "windows.h"
#include "winioctl.h"


void
typeInfo ()
{
   printf ("sizeof(_s8)  = %i\n", sizeof (_s8));
   printf ("sizeof(byte) = %i\n", sizeof (byte));
   printf ("sizeof(_u8)  = %i\n", sizeof (_u8));

   printf ("sizeof(_s16) = %i\n", sizeof (_s16));
   printf ("sizeof(_u16) = %i\n", sizeof (_u16));

   printf ("sizeof(_s32) = %i\n", sizeof (_s32));
   printf ("sizeof(_u32) = %i\n", sizeof (_u32));
}

void
printInode (ext2_t * this, dir_iterator_t * list_dir_iterator, _u32 parentInode)
{
   inode_t inode;

   printf ("%6i ", list_dir_iterator->ext2_dir_entry.inode);
   printf ("%30s ", list_dir_iterator->ext2_dir_entry.name);

   ext2_load_inode (this, &inode, list_dir_iterator->ext2_dir_entry.inode);
   if (S_ISSOCK (inode.i_mode))
      printf ("soc ");
   else if (S_ISLNK (inode.i_mode))
   {
      ext2_resolve_softlink (this, &inode, parentInode);
      printf ("lnk ");
   }
   else if (S_ISFIL (inode.i_mode))
      printf ("fil ");
   else if (S_ISBLK (inode.i_mode))
      printf ("blk ");
   else if (S_ISDIR (inode.i_mode))
      printf ("dir ");
   else if (S_ISCHR (inode.i_mode))
      printf ("chr ");
   else if (S_ISFIFO (inode.i_mode))
      printf ("fif ");
   else
      printf ("??? ");

   printf ("%i ", inode.i_size);


   printf ("\n");
}

char current_path[1000];
int current_inode = 2;
ext2_t *ext2;

void
doCD (char *path)
{
   inode_t inode;
   _s32 inode_num = ext2_lookup (ext2, path, EXT2_ROOT_INODE);

   if (inode_num > 0)
   {
      ext2_load_inode (ext2, &inode, inode_num);
      current_inode = inode_num;
      strcpy (current_path, path);
      ext2_print_inode (&inode, inode_num);
   }
   else
   {
      printf ("Inode not found.\n");
   }
}

void
doCAT ()
{
   inode_t inode;
   char *buffer;

   if (ext2_load_inode (ext2, &inode, current_inode) != EXT2_STATUS_OK)
   {
      printf ("ls couldn't load inode %i\n", current_inode);
      return;
   }

   buffer = (char *) malloc (inode.i_size + 1);
   ASSERT (buffer);

   ext2_read_inode (ext2, &inode, buffer, 0, inode.i_size, 0);
   buffer[inode.i_size] = 0;

   printf ("size: %d\n", inode.i_size);
   printf ("%s\n", buffer);

   free (buffer);
}

void
doTEST ()
{
   inode_t inode;
   char *buffer;

   if (ext2_load_inode (ext2, &inode, current_inode) != EXT2_STATUS_OK)
   {
      printf ("ls couldn't load inode %i\n", current_inode);
      return;
   }

   buffer = (char *) malloc (inode.i_size + 1);
   ASSERT (buffer);

   ext2_read_inode (ext2, &inode, buffer, 0, inode.i_size, 0);
   buffer[inode.i_size] = 0;

   printf ("%s\n", buffer);

   free (buffer);
}

int
getChecksum (inode_t * inode)
{
   char *buffer;

   int result = 0;
   _u32 i;


   buffer = (char *) malloc (inode->i_size + 1);
   ASSERT (buffer);

   if (buffer == 0)
   {
      printf ("out of memory %i \n", inode->i_size);
      exit (1);
   }

   ext2_read_inode (ext2, inode, buffer, 0, inode->i_size, 0);

   for (i = 0; i < inode->i_size; i++)
   {
      result = result + buffer[i];
   }

   free (buffer);

   return i;
}

void
_doCHECK (char *path, _s32 rootInode)
{
   char *buffer;
   dir_iterator_t *dir_iterator;
   inode_t inode;
   inode_t newInode;
   _s32 inode_num_2;

   //_s32 inode_num = ext2_lookup(ext2, path, rootInode);

   if (rootInode > 0)
   {
      ext2_load_inode (ext2, &inode, rootInode);
   }
   else
   {
      printf ("doCheck : inode_num invalid");
      exit (1);
   }

   buffer = (char *) malloc (inode.i_size);
   ASSERT (buffer);

   ext2_read_inode (ext2, &inode, buffer, 0, inode.i_size, 0);

   dir_iterator = dir_iterator_create ();

   while (ext2_list_dir (ext2, buffer, inode.i_size, dir_iterator) ==
          EXT2_STATUS_OK)
   {

      inode_num_2 =
         ext2_lookup (ext2, dir_iterator->ext2_dir_entry.name, rootInode);

      if (inode_num_2 > 0)
      {
         ext2_load_inode (ext2, &newInode, inode_num_2);
      }
      else
      {
         printf ("doCheck [2] : inode_num invalid");
         //exit(1);
         continue;
      }


      printf ("- %s\n", dir_iterator->ext2_dir_entry.name);

      if ((strcmp (".", dir_iterator->ext2_dir_entry.name) != 0)
          && (strcmp ("..", dir_iterator->ext2_dir_entry.name) != 0))
      {
         if (S_ISDIR (newInode.i_mode))
         {
            _doCHECK (dir_iterator->ext2_dir_entry.name, inode_num_2);
         }
         else
         {
            printf (" + %8x\n", getChecksum (&newInode));
         }
      }

   }

   dir_iterator_dispose (dir_iterator);

   free (buffer);
}

void
doCHECK ()
{
   _doCHECK ("\\", EXT2_ROOT_INODE);
}

void
doLS ()
{
   inode_t inode;
   char *buffer;
   dir_iterator_t *dir_iterator;

   if (ext2_load_inode (ext2, &inode, current_inode) != EXT2_STATUS_OK)
   {
      printf ("ls couldn't load inode %i\n", current_inode);
      return;
   }

   buffer = (char *) malloc (inode.i_size);
   ASSERT (buffer);

   ext2_read_inode (ext2, &inode, buffer, 0, inode.i_size, 0);

   dir_iterator = dir_iterator_create ();

   while (ext2_list_dir (ext2, buffer, inode.i_size, dir_iterator) ==
          EXT2_STATUS_OK)
   {
      //printf("ls : %i %i \n", dir_iterator->position, dir_iterator->ext2_dir_entry.rec_len);
      printInode (ext2, dir_iterator, current_inode);
   }

   dir_iterator_dispose (dir_iterator);

   free (buffer);
}

int __cdecl
main (int argc, char *argv[])
{
   char x[10][1000];

   char string[1000];
   char *start;
   char *pos = string;
   int arg_count = 0;
   int i;

   ext2 = ext2_create ();
   ext2_init (ext2);

   ext2_print_super (ext2);


   current_path[0] = 0;


   for (;;)
   {
      printf ("%s>", current_path);

      for (i = 0; i < 10; i++)
         x[i][0] = 0;

      pos = string;
      for (;;)
      {
         fread (pos, 1, 1, stdin);
         if (*pos == 0x0A)
            break;
         pos++;
      }
      *pos = 0;

      pos = string;
      arg_count = 0;

      while (*pos)
      {
         //Kill spaces
         while (*pos <= ' ' && !(*pos == 0))
            pos++;

         start = pos;
         while (*pos > ' ' && !(*pos == 0))
            pos++;

         memcpy (x[arg_count], start, pos - start);
         x[arg_count][pos - start] = 0;
         arg_count++;
      }

      if (arg_count > 0)
      {
         if (strcmp ("quit", x[0]) == 0)
            break;
         else if (strcmp ("ls", x[0]) == 0)
            doLS ();
         else if (strcmp ("cd", x[0]) == 0)
            doCD (x[1]);
         else if (strcmp ("cat", x[0]) == 0)
            doCAT ();
         else if (strcmp ("test", x[0]) == 0)
            doTEST ();
         else if (strcmp ("check", x[0]) == 0)
            doCHECK ();
         else
            printf ("??\n");
      }


   }

   ext2_dispose (ext2);

   return 0;
}
