/*
 * fiemap.cc - get physical block mapping
 *
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

#include "fiemap.hh"
#include "logging.hh"

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Call fiemap ioctl on file descriptor fd.
 * Determine the size of struct fiemap by extent_count.
 * If extent_count is set to 0, it chooses the size by itself to receive all extents.
 *
 * Returns NULL on error
 */
struct fiemap* ioctl_fiemap(int fd, unsigned int extent_count)
{
    if(!extent_count)
        extent_count = 10;
    
    struct fiemap* fmap = (struct fiemap*)calloc(1, 
                sizeof(struct fiemap) + extent_count * sizeof(struct fiemap_extent));

    fmap->fm_length = FIEMAP_MAX_OFFSET;
    fmap->fm_flags |= FIEMAP_FLAG_SYNC;
    fmap->fm_extent_count = extent_count;

    if(ioctl(fd, FS_IOC_FIEMAP, fmap) < 0)
    {
        char __filename[PATH_MAX];
        char __path2fd[1024];
        
        sprintf(__path2fd, "/proc/self/fd/%d", fd);
        int len;
        if((len = readlink(__path2fd, __filename, PATH_MAX)) != -1)
        {
            __filename[len] = '\0';    
            error(_("ioctl_fiemap: %s: %s"), __filename, strerror(errno));
        }
        else
            error(_("ioctl_fiemap and readlink failed: %s"), strerror(errno));
        
        free(fmap);
        return NULL;
    }
    
    if(fmap->fm_mapped_extents == fmap->fm_extent_count)
        return ioctl_fiemap(fd, extent_count<<1);

    if(fmap->fm_mapped_extents < fmap->fm_extent_count)
    {
        fmap = (struct fiemap*) realloc(fmap, sizeof(struct fiemap) 
                                        + fmap->fm_mapped_extents  * sizeof(struct fiemap_extent));
        fmap->fm_extent_count = fmap->fm_mapped_extents;
    }
    
    return fmap;
}

/*
 * Return struct fiemap by calling ioctl_fiemap
 */
struct fiemap* get_fiemap(const char* file)
{
    int fd;
    fd = open64(file, O_RDONLY);
    if (fd < 0)
    {
        error(_("open: %s: %s"), file, strerror(errno));
        return NULL;
    }
    struct fiemap* fmap = ioctl_fiemap(fd, 0);
    close(fd);
    return fmap;
}

/*
 * Test whether file is a sparse file
 */
bool is_sparse_file(struct fiemap* fmap)
{
    __u64 estimated = 0;
    for(unsigned int j=0; j < fmap->fm_mapped_extents; j++)
    {
        if(fmap->fm_extents[j].fe_logical != estimated)
            return true;
        estimated += fmap->fm_extents[j].fe_length;
    }
    return false;
}

/*
 * return size in bytes
 */

__u64 get_allocated_file_size(const char* file)
{
    return get_allocated_file_size(get_fiemap(file));
        
}
__u64 get_allocated_file_size(struct fiemap* fmap)
{
    __u64 result = 0;
    
    if(NULL == fmap)
        return 0;

    for(unsigned int j=0; j < fmap->fm_mapped_extents; j++)
        result += fmap->fm_extents[j].fe_length;

    return result;
}
/*
 * Calculate the inode's logical size in Bytes.
 * Sparse files may have holes of unallocated blocks between its extents.
 *
 * The return value will inherit those blocks too. 
 * So the returned number in Bytes can be used to create an appropriate donor file. 
 */
__u64 get_file_size(int fd)
{
    return get_file_size(ioctl_fiemap(fd));
}

__u64 get_file_size(struct fiemap* fmap)
{
    if(NULL == fmap)
        return 0;
    
    for(unsigned int j=0; j < fmap->fm_mapped_extents; j++)
        if(fmap->fm_extents[j].fe_flags & FIEMAP_EXTENT_LAST)
            return (fmap->fm_extents[j].fe_logical + fmap->fm_extents[j].fe_length);
    return 0;
}

/*
 * In case of sparse files ignore unallocated holes.
 */
__u32 get_frag_count(int fd)
{
    __u32 result = 1;
    struct fiemap* fmap;
    
    fmap = ioctl_fiemap(fd, 0);
    if(NULL == fmap)
        return 0;

    for(unsigned int j=1; j < fmap->fm_mapped_extents; j++)
        if(fmap->fm_extents[j].fe_physical != fmap->fm_extents[j-1].fe_physical + fmap->fm_extents[j].fe_logical - fmap->fm_extents[j-1].fe_logical)
            result++;

    return result;
}

