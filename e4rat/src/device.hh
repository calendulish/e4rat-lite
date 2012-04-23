/*
 * device.hh - get/set paramters for a block device
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

#ifndef DEVICE_HH
#define DEVICE_HH

#include "common.hh"
#include <string>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>


fs::path __getMountPoint(fs::path path);


/*
 * Physical extent of free blocks. On disk start block and length.
 * Unlike ext4 extents this class does not store logical file offset
 */
class Extent
{
    public:
        Extent(__u64 s, __u32 l) : start(s), len(l) {}
        Extent() : start(0), len(0) {}
        unsigned long long start;
        size_t len;
};


/*
 * Private Data of Device Object.
 */
class DevicePrivate
{
        friend class Device;
    public:
        ~DevicePrivate();
    private:
        DevicePrivate();
        
        ext2_filsys fs;
        dev_t devno;
        std::string deviceName; // Linux block device name. Example: "sda1"
        std::string devicePath;
        fs::path mount_point;
        std::string fs_name;    // name of the filesystem. Example: "ext4"
};

/*
 * Device is the source of information of a specific filesystem.
 * To create this class a file of the desired filesystem is needed.
 *
 * Device is derived from shared_ptr. So all created copies have the
 * same Private Data. Shared pointer is responsible for destructing of the
 * ext2_filsys object.
 */
class Device: private boost::shared_ptr<DevicePrivate>
{
    public:
        //file is a path belongs to device/filesystem
        //it is not the path to the device themself
        Device(fs::path file);
        Device(dev_t);
        bool open();
        std::string getDeviceName();
        std::string getDevicePath();
        fs::path    getMountPoint();
        std::string getFileSystem();
        void        setTuningParameter(std::string, unsigned int val);
        __u32       getTuningParameter(std::string);
        bool        hasExtentFeature();
        __u32       getBlockSize();
        __u64       freeBlocksPerFlex();
        __u64       freeBlocksPerGroup();
        __u32       getBlocksPerGroup();
        __u32       getGroupCount();
        __u32       getLogGroupsPerFlex();

        void preallocate(int   fd,
                         __u64 physical,
                         __u32 logical,
                         __u32 len,
                         __u16 flags);

        void moveExtent( int   orig_fd,
                         int   donor_fd,
                         __u64 logical,  //logical block offset
                         __u64 len);     //block count to be moved
        bool operator<(const Device&) const;
    private:
        int getDevNameFromDevfs();
        int getDevNameFromMajorMinor();
        void parseMtab();
        void parseMtabFile(const char* path);
        void openSysFsExt4File(
                        std::filebuf* fb,
                        std::string filename,
                        std::ios_base::openmode mode);
};

#endif
