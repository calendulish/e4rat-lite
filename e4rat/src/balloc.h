/*
 * balloc.h - Declarations concerning ext4 filesystem
 * 
 * Copyright (C) 2009 NEC Software Tohoku, Ltd.
 * Copyright (C) 2011 by Andreas Rid
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef BALLOC_H
#define BALLOC_H

#ifdef __cplusplus
extern "C"
{
#endif 

#include <linux/types.h>
#include <linux/limits.h>
#include <linux/ioctl.h>

#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>

#ifndef EXT4_IOC_MOVE_EXT
#define EXT4_IOC_MOVE_EXT      _IOWR('f', 15, struct move_extent)
#endif

#ifndef EXT4_IOC_CONTROL_PA
#define EXT4_IOC_CONTROL_PA    _IOWR('f', 16, struct ext4_prealloc_info)
#endif

/* Macros for EXT4_IOC_CONTROL_PA */
#define EXT4_MB_MANDATORY       0x0001
#define EXT4_MB_ADVISORY        0x0002
#define EXT4_MB_DISCARD_PA      0x0004
#define EXT4_IOC_GET_PA        _IOWR('f', 17, struct ext4_prealloc_list)

/* The maximum number of inode PAs that EXT4_IOC_CONTROL_PA can set */
#define EXT4_MAX_PREALLOC       1024

/* The upper limit of length of prealloc which EXT4_IOC_CONTROL_PA can set */
//#define PREALLOC_MAX_BLK (blocks_per_group - 10)

/* Data type for filesystem-wide blocks number */
typedef unsigned long long ext4_fsblk_t;

struct fiemap_extent_data {
    __u64 len;              /* blocks count */
    __u64 logical;          /* start logical block number */
    ext4_fsblk_t physical;  /* start physical block number */
};
    
struct move_extent {
    __s32 reserved;         /* original file descriptor */
    __u32 donor_fd;         /* donor file descriptor */
    __u64 orig_start;       /* logical start offset in block for orig */
    __u64 donor_start;      /* logical start offset in block for donor */
    __u64 len;              /* block length to be moved */
    __u64 moved_len;        /* moved block length */
};

struct ext4_prealloc_info {
    __u64 pi_pstart; /* physical offset for the start of the PA from
                      * the beginning of the file (in/out) */
    __u32 pi_lstart; /* logical offset for the start of the PA from
                      * the beginning of the disk (in/out) */
    __u32 pi_len;    /* length for this PA (in/out) */
    __u32 pi_free;   /* the number of free blocks in this PA (out) */
    __u16 pi_flags;  /* flags for the inode PA setting ioctl (in) */
};
struct ext4_prealloc_list {
    __u32 pl_count;                         /* size of pl_space array (in) */
    __u32 pl_mapped;                        /* number of PAs that were mapped (out) */
    __u32 pl_entries;                       /* number of PAs the inode has (out) */
    struct ext4_prealloc_info pl_space[0];  /* array of mapped PAs (out) */
};

#ifdef __cplusplus
}
#endif 
#endif
