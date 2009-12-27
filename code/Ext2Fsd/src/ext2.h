/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  Description: Headerfile for ext2.c, super.c, group.c, inode.c and cache.c

  CVS Information:
    $Date: 2003/07/02 15:19:08 $
    $Source: /cvsroot/winlfs/code/Ext2Fsd/src/ext2.h,v $
    $Revision: 1.11 $
*/

/*!\file ext2.h
 * \brief Headerfile for ext2.c, super.c, group.c, inode.c and cache.c
 * 
 * Declares any structures and function prototypes used by the driver. Only
 * fields used during this thesis are listed. Short description of other 
 * fields can be found in the source code.
 */

#ifndef _EXT2_H_
#define _EXT2_H_

#if DBG
#define Ext2FsdBreakPoint()    DbgBreakPoint()
#else
#define Ext2FsdBreakPoint()
#endif

#ifdef DRIVER
#include "ntifs.h"
#else
#include "stdlib.h"
#include "windows.h"
#endif

#include "cache.h"

/*
 *  Typedefs for Ext2. Mapping Linux types to windows types.
 */

typedef signed char _s8;
typedef unsigned char _u8;
typedef unsigned char byte;

typedef signed short _s16;
typedef unsigned short _u16;

typedef signed long _s32;
typedef unsigned long _u32;

typedef LARGE_INTEGER _s64;
typedef ULARGE_INTEGER _u64;

/*
 * Special inodes numbers
 */

#define	EXT2_BAD_INODE		 1      /* Bad blocks inode */
#define EXT2_ROOT_INODE		 2      /* Root inode */
#define EXT2_ACL_IDX_INODE	 3      /* ACL inode */
#define EXT2_ACL_DATA_INODE	 4      /* ACL inode */
#define EXT2_BOOT_LOADER_INODE	 5      /* Boot loader inode */
#define EXT2_UNDEL_DIR_INODE	 6      /* Undelete directory inode */
#define EXT2_RESIZE_INODE		 7      /* Reserved group descriptors inode */
#define EXT2_JOURNAL_INODE	 8      /* Journal inode */
#define EXT2_GOOD_OLD_FIRST_INODE	11  /* First non-reserved inode for old ext2 filesystems */
#define EXT2_SUPER_MAGIC 0xEF53

/*
 * Feature set definitions
 */

#define EXT2_HAS_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_es->s_feature_compat & cpu_to_le32(mask) )
#define EXT2_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_es->s_feature_ro_compat & cpu_to_le32(mask) )
#define EXT2_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_es->s_feature_incompat & cpu_to_le32(mask) )
#define EXT2_SET_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_compat |= cpu_to_le32(mask)
#define EXT2_SET_RO_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_ro_compat |= cpu_to_le32(mask)
#define EXT2_SET_INCOMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_incompat |= cpu_to_le32(mask)
#define EXT2_CLEAR_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_compat &= ~cpu_to_le32(mask)
#define EXT2_CLEAR_RO_COMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_ro_compat &= ~cpu_to_le32(mask)
#define EXT2_CLEAR_INCOMPAT_FEATURE(sb,mask)			\
	EXT2_SB(sb)->s_es->s_feature_incompat &= ~cpu_to_le32(mask)

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO		0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_COMPAT_ANY			0xffffffff

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT2_FEATURE_RO_COMPAT_ANY		0xffffffff

#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_ANY		0xffffffff

#define EXT2_FEATURE_COMPAT_SUPP	EXT2_FEATURE_COMPAT_EXT_ATTR
#define EXT2_FEATURE_INCOMPAT_SUPP	EXT2_FEATURE_INCOMPAT_FILETYPE
#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT2_FEATURE_RO_COMPAT_SUPP
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	~EXT2_FEATURE_INCOMPAT_SUPP

#define EXT2_MIN_BLOCK  1024
#define EXT2_MIN_FRAG   1024

#define EXT2_INVALID_BLOCK 0
#define EXT2_INVALID_INODE 0

/*
 * Inode flags
 */
#define S_IPERMISSION_MASK 0x1FF        /*  */

#define S_IRWXU 0x01C0          /*  00700 */
#define S_IRUSR 0x0100          /*  00400 */
#define S_IWUSR 0x0080          /*  00200 */
#define S_IXUSR 0x0040          /*  00100 */

#define S_IRWXG 0x0038          /*  00070 */
#define S_IRGRP 0x0020          /*  00040 */
#define S_IWGRP 0x0010          /*  00020 */
#define S_IXGRP 0x0008          /*  00010 */

#define S_IRWXO 0x0007          /*  00007 */
#define S_IROTH 0x0004          /*  00004 */
#define S_IWOTH 0x0002          /*  00002 */
#define S_IXOTH 0x0001          /*  00001 */

#define S_IFMT   0x0F000        /*017 0000 */

#define S_IFSOCK 0x0C000        /*014 0000 */
#define S_IFLNK  0x0A000        /*012 0000 */
#define S_IFFIL  0x08000        /*010 0000 */
#define S_IFBLK  0x06000        /*006 0000 */
#define S_IFDIR  0x04000        /*004 0000 */
#define S_IFCHR  0x02000        /*002 0000 */
#define S_IFIFO  0x01000        /*001 0000 */

#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)
#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#define S_ISFIL(m)      (((m) & S_IFMT) == S_IFFIL)
#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)

/*not yet supported: */
#define S_IFUID  0x00800        /*000 4000 */
#define S_IFGID  0x00400        /*000 2000 */
#define S_IFVTX  0x00200        /*000 1000 */

#define S_ISREADABLE(m)    (((m) & S_IPERMISSION_MASK) == (S_IRUSR | S_IRGRP | S_IROTH))
#define S_ISWRITABLE(m)    (((m) & S_IPERMISSION_MASK) == (S_IWUSR | S_IWGRP | S_IWOTH))

#define Ext2FsdSetReadable(m) (m) = ((m) | (S_IRUSR | S_IRGRP | S_IROTH))
#define Ext2FsdSetWritable(m) (m) = ((m) | (S_IWUSR | S_IWGRP | S_IWOTH))

#define Ext2FsdSetReadOnly(m) (m) = ((m) & (~(S_IWUSR | S_IWGRP | S_IWOTH)))
#define Ext2FsdIsReadOnly(m)  (!((m) & (S_IWUSR | S_IWGRP | S_IWOTH)))


/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */

#define EXT2_FT_UNKNOWN		0
#define EXT2_FT_REG_FILE	1
#define EXT2_FT_DIR			2
#define EXT2_FT_CHRDEV		3
#define EXT2_FT_BLKDEV 		4
#define EXT2_FT_FIFO		5
#define EXT2_FT_SOCK		6
#define EXT2_FT_SYMLINK		7
#define EXT2_FT_MAX			8

#define EXT2_MIN_BLOCK_SIZE		1024
#define	EXT2_MAX_BLOCK_SIZE		4096

#define DISK_BLOCK_SIZE  512UL  /*size of harddisk sectors in byte */

#define TIMEZONEDIFF    7200    /* (Your local time - GMT) *3600 */
#define TIMEZONE        TIMEZONEDIFF+14400

#define EXT2_STATUS         _s32
#define EXT2_STATUS_ERROR   FALSE
#define EXT2_STATUS_OK      TRUE

#define EXT2_NAME_LEN 255

/*
 * EXT2_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define EXT2_DIR_PAD		 	4
#define EXT2_DIR_ROUND 			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & ~EXT2_DIR_ROUND)


/*!\brief Super block structure
 *
 * This structure represents the super block in memory.
 * 
 * Block number 1 of a block group.
 */
typedef struct
{
   _u32 s_inodes_count;         //!< Total number of inodes
   _u32 s_blocks_count;         //!< Filesystem size in blocks
   _u32 s_r_blocks_count;       /* Number of reserved blocks */
   _u32 s_free_blocks_count;    /* Free blocks count */
   _u32 s_free_inodes_count;    /* Free inodes count */
   _u32 s_first_data_block;     /* First Data Block */
   _u32 s_log_block_size;       /* Block size */
   _s32 s_log_frag_size;        /* Fragment size */
   _u32 s_blocks_per_group;     //!< Number of blocks per group
   _u32 s_frags_per_group;      /* # Fragments per group */
   _u32 s_inodes_per_group;     //!< Number of inodes per group */
   _u32 s_mtime;                /* Mount time */
   _u32 s_wtime;                /* Write time */
   _u16 s_mnt_count;            /* Mount count */
   _s16 s_max_mnt_count;        /* Maximal mount count */
 
   /*!\brief Magic signature
    *
	* This is the signature of the partition. The ext2 and ext3 partitions
	* have 0xEF53 at this place.
	*/
   _u16 s_magic;                /* Magic signature */
   _u16 s_state;                /* File system state */
   _u16 s_errors;               /* Behaviour when detecting errors */
   _u16 s_minor_rev_level;      /* Minor revision level */
   _u32 s_lastcheck;            /* time of last check */
   _u32 s_checkinterval;        /* max. time between checks */
   _u32 s_creator_os;           /* OS */
   _u32 s_rev_level;            /* Revision level */
   _u16 s_def_resuid;           /* Default uid for reserved blocks */
   _u16 s_def_resgid;           /* Default gid for reserved blocks */

   /* These fields are for EXT2_DYNAMIC_REV superblocks only. */
   _u32 s_first_ino;            /* First non-reserved inode */
   _u16 s_inode_size;           /* size of inode structure */
   _u16 s_block_group_nr;       /* block group # of this superblock */
   _u32 s_feature_compat;       /* compatible feature set */
   _u32 s_feature_incompat;     /* incompatible feature set */
   _u32 s_feature_ro_compat;    /* readonly-compatible feature set */
   _u32 s_pad[230];             /* Padding to the end of the block */
}
super_block_t;

/*! \brief Group descripton structure
 *
 * This structure represents the group description block in memory.
 *
 * Block number 2 of a block group.
 */
typedef struct
{
   _u32 bg_block_bitmap;        //!< Block number of block bitmap
   _u32 bg_inode_bitmap;        //!< Block number of inode bitmap
   _u32 bg_inode_table;         //!< Block number of first inode table block
   _u16 bg_free_blocks_count;   //!< Number of free blocks in the group
   _u16 bg_free_inodes_count;   //!< Number of free inodes in the group
   _u16 bg_used_dirs_count;     /* Directories count */
   _u16 bg_pad;
   _u32 bg_reserved[3];
}
group_desc_t;

/*
 * Inode Structure
 */
typedef struct
{
   _u16 i_mode;                 //!< File type and access rights
   _u16 i_uid;                  /* Owner Uid */
   _u32 i_size;                 /<!< File size in bytes
   _u32 i_atime;                //!< Access time
   _u32 i_ctime;                //!< Creation time
   _u32 i_mtime;                //!< Modification time
   _u32 i_dtime;                //!< Deletion time
   _u16 i_gid;                  /* Group Id */
   _u16 i_links_count;          //!< Hard links counter
   _u32 i_blocks;               //!< Number of data blocks of the file
   _u32 i_flags;                //!< File flags
   _u32 i_reserved1;            /* Reserved 1 */
   _u32 i_block[15];            /* Pointers to blocks */
   _u32 i_version;              /* File version (for NFS) */
   _u32 i_file_acl;             /* File ACL */
   _u32 i_dir_acl;              /* Directory ACL */
   _u32 i_faddr;                /* Fragment address */
   _u8 i_frag;                  /* Fragment number */
   _u8 i_fsize;                 /* Fragment size */
   _u16 i_pad1;
   _u16 i_uid_high;             /* high bits of uid */
   _u16 i_gid_high;             /* high bits of gid */
   _u32 i_reserved2;
// _u32 i_reserved2[2];         /* Reserved 2 */
}
inode_t;

/*
 * Directory Structure
 */

struct dir
{
   _u32 inode_num;
   _u16 rec_len;
   _u8 name_len;
   _u8 file_type;
   char name[256];              /* between 0 and 256 chars */
};


typedef struct
{
   _u32 entries;
   _u32 size_of_entry;
   ULONGLONG counter;
   _u8 *cache;
}
cache_t;

typedef struct
{
   _u32 index;
   ULONGLONG age;
   _u8 used;
   _u8 *data;
}
cache_header_t;


/*!\brief Ext2 structure
 *
 * This structure represents an ext2 partition in memory.
 */
typedef struct
{
   /*!\brief Super block of this ext2 partition.
	*/
   super_block_t superblock;

   /*!\brief Pointer to the group descriptor of this partition.
    */
   group_desc_t *group_desc;

   _u32 num_groups;

   _u32 block_size;             //Global variables calculated from the super block info
   cache_t *inode_chache;
   cache_t *block_chache;
#ifdef DRIVER
   PDEVICE_OBJECT DeviceObject;
#endif
}
ext2_t;

/*
 * Structure of a directory entry
 */
typedef struct
{
   _u32 inode;                  /* Inode number */
   _u16 rec_len;                /* Directory entry length */
   _u8 name_len;                /* Name length */
   _u8 file_type;
   char name[EXT2_NAME_LEN + 1];        /* File name */
}
ext2_dir_entry_t, *pext2_dir_entry_t;

/*
 * Structure and functions of a directory iterator
 */
typedef struct
{
   _u32 position;
   ext2_dir_entry_t ext2_dir_entry;
}
dir_iterator_t;

dir_iterator_t *dir_iterator_create ();

void dir_iterator_dispose (dir_iterator_t * this);

/*
 * Ext2 functions
 */
ext2_t *ext2_create (void);

EXT2_STATUS ext2_init (ext2_t * this);

void ext2_dispose (ext2_t * this);

/*
 * Superblock functions
 */
EXT2_STATUS ext2_load_super (ext2_t * this);

void ext2_print_super (ext2_t * this);

/*
 * Group functions
 */
EXT2_STATUS ext2_load_groups (ext2_t * this);

void ext2_print_groups (ext2_t * this);

/*
 * Inode functions
 */

EXT2_STATUS ext2_load_inode (ext2_t * this, inode_t * inode, _u32 inode_no);

void ext2_print_inode (inode_t * inode, _u32 inode_num);

BOOLEAN ext2_inode_valid (ext2_t * this, _u32 inode_no);

_u32 ext2_resolve_softlink (ext2_t * this, inode_t * inode, _u32 parent);

EXT2_STATUS ext2_read_inode (ext2_t * this, inode_t * inode, char *buffer,
                             _u32 offset, _u32 size, _u32 * valid_size);

_u32 ext2_lookup (ext2_t * this, char *file, _u32 inode_num);

EXT2_STATUS ext2_list_dir (ext2_t * this, char *buffer, _u32 size,
                           dir_iterator_t * dir_iterator);

inode_t *Ext2FsdAllocateInode ();

void Ext2FsdFreeInode (inode_t * inode);

/*
 * Time functions
 */

#ifdef DRIVER
void ext2_getAccessTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT);
void ext2_getCreationTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT);
void ext2_getDeletionTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT);
void ext2_getModificationTimeNT (inode_t * inode, PLARGE_INTEGER TimeNT);
#endif


cache_t *cache_create (_u32 entries, _u32 size_of_entry);
void cache_add (cache_t * this, _u32 index, void *data);
void *cache_find (cache_t * this, _u32 index, BOOLEAN aging);

#endif /* _EXT2_H_ */
