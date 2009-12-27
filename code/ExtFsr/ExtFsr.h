/*
  Semesterarbeit (SS 2003) - Ext2 File System Driver for Windows XP
  Author: Michael Egli, Marc Winiger

  CVS Information:
    $Date: 2003/07/02 15:19:08 $
    $Source: /cvsroot/winlfs/code/ExtFsr/ExtFsr.h,v $
    $Revision: 1.15 $
*/

/*!\file ExtFsr.h
 * \brief Headerfile for ExtFsr.c
 * 
 * This file contains structures, constants and function prototypes used by ExtFsr.c.
 */

/*!\mainpage WinLFS Code Documentation
 *
 * \section intro Introduction
 *
 * This chapter contains the documentation of the code written or modified
 * during this thesis. The missing parts can be found in the documentation of
 * the continuing studies diploma thesis.
 */

#ifndef _EXTFSR_HEADER_
#define _EXTFSR_HEADER_

#include <ntdddisk.h>

#pragma pack(1)
#define SECTOR_SIZE (512)

/*!\brief Ext3 structure taken from linux source.
 *
 * Only attributes used in this driver are described. Short descriptions of
 * the other attributes can be found in the source code.
 */
struct ext3_super_block
{
   /* 00 */
   ULONG s_inodes_count;        // Inodes count 
   ULONG s_blocks_count;        // Blocks count 
   ULONG s_r_blocks_count;      // Reserved blocks count 
   ULONG s_free_blocks_count;   // Free blocks count 
   /* 10 */
   ULONG s_free_inodes_count;   // Free inodes count
   ULONG s_first_data_block;    // First Data Block
   ULONG s_log_block_size;      // Block size
   LONG s_log_frag_size;        // Fragment size
   /* 20 */
   ULONG s_blocks_per_group;    // # Blocks per group
   ULONG s_frags_per_group;     // # Fragments per group
   ULONG s_inodes_per_group;    // # Inodes per group
   ULONG s_mtime;               // Mount time
   /* 30 */
   ULONG s_wtime;               // Write time
   USHORT s_mnt_count;          // Mount count
   SHORT s_max_mnt_count;       // Maximal mount count

   /*!\brief Magic signature.
    *
	* This is the signature of the partition. The ext2 and ext3 partitions
	* have 0xEF53 at this place.
	*/
   USHORT s_magic;
   USHORT s_state;              // File system state
   USHORT s_errors;             // Behaviour when detecting errors
   USHORT s_minor_rev_level;    // minor revision level
   /* 40 */
   ULONG s_lastcheck;           // time of last check
   ULONG s_checkinterval;       // max. time between checks
   ULONG s_creator_os;          // OS
   ULONG s_rev_level;           // Revision level
   /* 50 */
   USHORT s_def_resuid;         // Default uid for reserved blocks
   USHORT s_def_resgid;         // Default gid for reserved blocks

/*
 * These fields are for EXT3_DYNAMIC_REV superblocks only.
 *
 * Note: the difference between the compatible feature set and
 * the incompatible feature set is that if there is a bit set
 * in the incompatible feature set that the kernel doesn't
 * know about, it should refuse to mount the filesystem.
 *
 * e2fsck's requirements are more strict; if it doesn't know
 * about a feature in either the compatible or incompatible
 * feature set, it must abort and not try to meddle with
 * things it doesn't understand...
 */
   ULONG s_first_ino;           // First non-reserved inode
   USHORT s_inode_size;         // size of inode structure
   USHORT s_block_group_nr;     // block group # of this superblock

   /*!\brief Compatible feature set.
    * 
	* This attribute is needed to determine if there is a ext2 or ext3
	* partition.
	*/
   ULONG s_feature_compat;

/* 60 */
   ULONG s_feature_incompat;    // incompatible feature set
   ULONG s_feature_ro_compat;   // readonly-compatible feature set

/* 68 */
   UCHAR s_uuid[16];            // 128-bit uuid for volume

/* 78 */
   CHAR s_volume_name[16];      // volume name

/* 88 */
   CHAR s_last_mounted[64];     // directory where last mounted

/* C8 */
   ULONG s_algorithm_usage_bitmap;      // For compression

/*
 * Performance hints. Directory preallocation should only
 * happen if the EXT3_FEATURE_COMPAT_DIR_PREALLOC flag is on.
 */
   UCHAR s_prealloc_blocks;     // Nr of blocks to try to preallocate*/
   UCHAR s_prealloc_dir_blocks; // Nr to preallocate for dirs
   USHORT s_padding1;

/*
 * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
 */

/* D0 */
   UCHAR s_journal_uuid[16];    // uuid of journal superblock

/* E0 */
   ULONG s_journal_inum;        // inode number of journal file
   ULONG s_journal_dev;         // device number of journal file
   ULONG s_last_orphan;         // start of list of inodes to delete

/* EC */
   ULONG s_reserved[197];       // Padding to the end of the block
};

#pragma pack()

typedef struct ext3_super_block EXT3_SUPER_BLOCK, *PEXT3_SUPER_BLOCK;

/*!\brief Load status structure.
 *
 * Structure to store loaded and failed statuses of the drivers.
 */
typedef struct _LOAD_STATUS
{
   BOOLEAN loaded;              //!< Stores whether driver is loaded already.
   BOOLEAN failed;              //!< Stores whether driver failed on a previous try to load.
} LOAD_STATUS, *PLOAD_STATUS;

/*!\brief Device extension structure.
 *
 * Structure to store persistent values that are needed by a later IRP request.
 */
typedef struct _ExtFsrDeviceExtension
{
   LOAD_STATUS ext3;            //!< Stores whether ext3 driver is loaded or failed.
   LOAD_STATUS ext2;            //!< Stores whether ext2 driver is loaded or failed. 
   BOOLEAN journalling;         //!< Stores whether recognized partition has journalling or not. 
} ExtFsrDeviceExtension, *DEVICE_EXTENSION;

#define EXT_SUPER_MAGIC 0xEF53

#define EXT3_FEATURE_COMPAT_HAS_JOURNAL 0x0004
#define EXT3_HAS_COMPAT_FEATURE(sb,mask) ( sb->s_feature_compat & mask )

#define DRIVER_NAME "ExtFsr"
#define FSD_RECOGNIZER_NAME L"\\FileSystem\\ExtFsRecognizer"

#define EXTFSR_REGISTRY_PATH L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\ExtFsr"

#define EXT2FSD_REGISTRY_PATH L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Ext2Fsd"

#define EXT3FSD_REGISTRY_PATH L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Ext3Fsd"

PDEVICE_OBJECT ExtFsrDeviceObject = NULL;

NTSTATUS ReadDisk (IN PDEVICE_OBJECT DeviceObject, IN LONGLONG Offset,
                   IN ULONG Length, OUT PVOID Buffer);

PEXT3_SUPER_BLOCK LoadSuperBlock (IN PDEVICE_OBJECT DeviceObject);

VOID ExtFsrUnload (IN PDRIVER_OBJECT DeviceObject);

NTSTATUS FileSystemControl (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

#endif  // _EXTFSR_HEADER_

