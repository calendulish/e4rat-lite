/*
 * device.cc - get/set paramters for a block device
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

#include "device.hh"
#include "balloc.h"
#include "logging.hh"

#include <fstream>
#include <stdexcept>
#include <errno.h>
#include <libintl.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mntent.h>

#include <boost/lexical_cast.hpp>

#define BLOCKS_PER_GROUP(fs)      (fs->super->s_blocks_per_group)
#define BLOCKS_PER_FLEX(fs)       (BLOCKS_PER_GROUP(fs) << fs->super->s_log_groups_per_flex)
#define FREE_BLOCKS_PER_GROUP(fs) ( BLOCKS_PER_GROUP(fs)        \
                    - 2                    \
                    - fs->super->s_inode_size * fs->super->s_inodes_per_group / fs->blocksize \
                              )

#define FREE_BLOCKS_PER_FLEX(fs)  (FREE_BLOCKS_PER_GROUP(fs) << fs->super->s_log_groups_per_flex)

#define _(x) gettext(x)

/*
 * get mount-point from arbitrary path
 * path should have a root directory '/'
 */
fs::path __getMountPoint(fs::path path)
{
    struct stat st;
    dev_t dev;

    fs::path cur_dir = path.parent_path();

    //test weather path is root directory '/'
    if(cur_dir.string().empty())
        return path;

    if(0 > stat(cur_dir.string().c_str(), &st))
        goto err;
    dev = st.st_dev;

    while(1)
    {
        if(0 > stat(cur_dir.string().c_str(), &st))
            goto err;
        if(st.st_dev != dev)
            return path;

        cur_dir = cur_dir.parent_path();

        if(cur_dir == "/" || cur_dir.empty())
            return "/";
    }

err:
    throw std::runtime_error(std::string(_("Cannot get MountPoint of path: "))
                             +path.string());
}


DevicePrivate::DevicePrivate()
{
    fs = 0;
}

DevicePrivate::~DevicePrivate()
{
    if(fs)
        ext2fs_close(fs);
}


Device::Device(fs::path file)
    : boost::shared_ptr<DevicePrivate>(new DevicePrivate)
{
    struct stat st;
    if(lstat(file.string().c_str(), &st))
    {
        std::stringstream ss;
        ss << _("Cannot get devno from file ") << file.string() << _(" to create Device object");
        throw std::runtime_error(ss.str());
    }
    if(S_ISBLK(st.st_mode))
        get()->devno = st.st_rdev;
    else
        get()->devno = st.st_dev;
}

Device::Device(dev_t dev)
    : boost::shared_ptr<DevicePrivate>(new DevicePrivate)
{
    get()->devno = dev;
}

void Device::parseMtabFile(const char* path)
{
    FILE* fmtab;
    struct mntent   *mnt = NULL;
    struct stat st;

    fmtab = setmntent(path, "r");
    if(fmtab == NULL)
        throw std::runtime_error(std::string(_("Cannot access ")) + path + ": " + strerror(errno));

    while((mnt = getmntent(fmtab)) != NULL)
    {
        if(0 == strcmp(mnt->mnt_type, "rootfs"))
            continue;
        if(stat(mnt->mnt_dir, &st))
            continue;
        if(st.st_dev == get()->devno)
        {
            get()->mount_point = mnt->mnt_dir;
            get()->fs_name = mnt->mnt_type;
            break;
        }
    }

    endmntent(fmtab);
}

void Device::parseMtab()
{
    if(0 ==access("/proc/mounts", R_OK))
    {
        parseMtabFile("/proc/mounts");
        if( get()->fs_name == "ext2")
            // maybe /proc/mounts is not up to date cause user forget to setup
            // rootfstype=ext4 to kernel parameters.
            parseMtabFile(MOUNTED);
    }
    else if(0 == access(MOUNTED, R_OK))
        parseMtabFile(MOUNTED);
    else
        throw std::runtime_error(_("Neither /proc/mounts nor /etc/mtab is readable."));
}
fs::path Device::getMountPoint()
{
    if(get()->mount_point.empty())
        parseMtab();

    return get()->mount_point;
}

/*
 * Create ext2fs object which gave access to the ext2 superblock and other
 * filesystem information
 */
bool Device::open()
{
    if( ext2fs_open(getDevicePath().c_str(),
                    EXT2_FLAG_RW | EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES,
                    0, 0, unix_io_manager, &get()->fs))
        return false;
    return true;
}

std::string Device::getFileSystem()
{
    if(get()->fs_name.empty())
        parseMtab();
    return get()->fs_name;
}

/*
 * convert dev_t to device string
 * Iterate over /dev directory
 * return 0 on success otherwise -1
 */
int Device::getDevNameFromDevfs()
{
    struct stat st;
    fs::directory_iterator end_itr; // default construction yields past-the-end
    for ( fs::directory_iterator it("/dev");
        it != end_itr;
        ++it )
    {
        if(it->path().filename().string() == "root")
            continue;
        if(lstat(it->path().filename().c_str(), &st))
            continue;
        if(st.st_rdev == get()->devno)
        {
            get()->deviceName = it->path().filename().string();
            get()->devicePath = "/dev/" + get()->deviceName;
            return 0;
        }
    }
    return -1;
}

/*
 * return 0 success otherwise -1
 */
int Device::getDevNameFromMajorMinor()
{
    std::stringstream ss;
    int major = major(get()->devno);
    int minor = minor(get()->devno);

    switch(major)
    {
        case 0:
            // the minor number of virtual filesystems are allocated dynamically in function set_anon_super() in fs/super.c
            // for convenience set deviceName and devicePath to a common name
            get()->deviceName = "virtual file system";
            get()->devicePath = get()->mount_point.filename().string();
            return 0;
        case 2:
            ss << "fd";
            goto devicename_has_no_letter;
        case 3:
            ss << "hd"; break;
        case 8:
            ss << "sd"; break;
        case 254:
            ss << "dm-";
            goto devicename_has_no_letter;
        default:
            return -1;
    }

    ss << (char)(0x61 + (minor >>4));

devicename_has_no_letter:
    ss << (minor & 0b1111);

    get()->deviceName = ss.str();
    get()->devicePath = "/dev/" + get()->deviceName;
    return 0;
}


bool isMountPoint(fs::path p)
{
    struct stat st1, st2;
    if(-1 == stat(p.string().c_str(), &st1)
        || -1 == stat(p.parent_path().string().c_str(), &st2))
        return false;

    if(st1.st_dev == st2.st_dev)
        return false;
    else
        return true;
}

/*
 * Throw runtime_error on error
 */
std::string Device::getDeviceName()
{
    if(-1 == getDevNameFromMajorMinor())
    {
        if(!isMountPoint("/dev"))
            throw std::runtime_error(_("Unknown block device: devfs is not mounted"));

        if(-1 == getDevNameFromDevfs())
            throw std::runtime_error(_("Unknown block device: no such device found in /dev"));
    }

    return get()->deviceName;
}

/*
 * return full path of device
 * example: /dev/sda1
 */
std::string Device::getDevicePath()
{
    if(get()->devicePath.empty())
        getDeviceName();
    return get()->devicePath;
}

/*
 * this function is used by read and write ext4 tuning parameters
 */
void Device::openSysFsExt4File(std::filebuf* fb, std::string filename, std::ios_base::openmode mode)
{
    std::string fullPath = std::string("/sys/fs/ext4/")
                           + getDeviceName()
                           + "/" + filename;

    if(NULL == fb->open(fullPath.c_str(), mode))
        throw std::runtime_error(std::string(_("Cannot open file: ")) + fullPath);
}

/*
 * set tuning parameter to current ext4 filesystem
 * write value to /sys/fs/ext4/<device>/<option>
 */
void Device::setTuningParameter(std::string option, unsigned int val)
{
    std::ofstream os;
    openSysFsExt4File(os.rdbuf(), option, std::ios_base::out);
    os << val;
    os.close();
}

/*
 * receive current ext4 tuning parameter.
 */
unsigned int Device::getTuningParameter(std::string option)
{
    unsigned int ret;
    std::ifstream is;
    openSysFsExt4File(is.rdbuf(), option, std::ios_base::in);

    is >> ret;
    is.close();
    return ret;
}

bool Device::hasExtentFeature()
{
    return EXT2_HAS_INCOMPAT_FEATURE(get()->fs->super, EXT3_FEATURE_INCOMPAT_EXTENTS);
}

/*
 * Call preallocate ioctl
 * without a limitation in block len
 */
void Device::preallocate(int   fd,
                         __u64 physical,
                         __u32 logical,
                         __u32 len,
                         __u16 flags)
{
    for(__u64 done = 0; done < len && !(flags & EXT4_MB_DISCARD_PA);)
    {
        struct ext4_prealloc_info pi;
        memset(&pi, '\0', sizeof(struct ext4_prealloc_info));

        pi.pi_pstart = physical + done;
        pi.pi_lstart = logical  + done;
        pi.pi_len    = std::min( len - done,
                                 (__u64)getBlocksPerGroup() - 10);
        pi.pi_flags  = flags;

        if(0 > ioctl(fd, EXT4_IOC_CONTROL_PA, &pi))
        {
            if(errno == ENOTTY)
                throw std::logic_error(_("Your actual Kernel does not support prefered block allocation."));
            else if(errno == ENOSPC && pi.pi_len)
                throw Extent(pi.pi_pstart, pi.pi_len);
            else
            {
                std::stringstream ss;
                ss << _("Cannot preallocate blocks: ")
                   << getPathFromFd(fd)             << "\n"
                   << strerror(errno)               << "\n"
                   << _("parameter:")                  << "\n"
                   << _("\tfd:      ") << fd           << "\n"
                   << _("\tphysical:") << physical     << "\n"
                   << _("\tlogical: ") << logical      << "\n"
                   << _("\tlen:     ") << len          << "\n"
                   << _("\tflags:   ") << flags        << "\n"
                   << _("return values:" )             << "\n"
                   << _("\tpstart:  ") << pi.pi_pstart << "\n"
                   << _("\tlstart:  ") << pi.pi_lstart << "\n"
                   << _("\tlen:     ") << pi.pi_len    << "\n";

                throw std::invalid_argument(ss.str());
            }
        }
        else
            done = pi.pi_len;
    }
}

void Device::moveExtent( int orig_fd,
                         int   donor_fd,
                         __u64 logical,
                         __u64 len)
{
    __u64 moved_blocks = 0;
    while(moved_blocks < len)
    {
        struct move_extent  move_data;
        memset(&move_data, 0, sizeof(struct move_extent));

        move_data.donor_fd    = donor_fd;
        move_data.orig_start  = (logical + moved_blocks)
                             * getBlockSize();
        /* Logical offset of orig and donor should be same */
        move_data.donor_start = move_data.orig_start;
        move_data.len         = (len - moved_blocks)
                             * getBlockSize();

        if(0 >  ioctl(orig_fd, EXT4_IOC_MOVE_EXT, &move_data))
        {
            std::stringstream ss;
            ss << _("Cannot move extent: ")
               << strerror(errno) << "\n"
               << _("orig:    ") << orig_fd << " "
               << getPathFromFd(orig_fd)  << "\n"
               << _("donor:   ") << donor_fd << " "
               << getPathFromFd(donor_fd)<< "\n"
               << _("logical: ") << logical << "\n"
               << _("len:     ") << len     << "\n";

            throw std::runtime_error(ss.str());
        }

        moved_blocks += move_data.moved_len<<12;
    }
}

__u32 Device::getBlockSize()
{
    return get()->fs->blocksize;
}

__u32 Device::getGroupCount()
{
    return get()->fs->group_desc_count;
}

__u64 Device::freeBlocksPerFlex()
{
    return FREE_BLOCKS_PER_FLEX(get()->fs);
}

__u64 Device::freeBlocksPerGroup()
{
    return FREE_BLOCKS_PER_GROUP(get()->fs);
}

__u32 Device::getBlocksPerGroup()
{
    return BLOCKS_PER_GROUP(get()->fs);
}

__u32 Device::getLogGroupsPerFlex()
{
    return get()->fs->super->s_log_groups_per_flex;
}

bool Device::operator<(const Device& other) const
{
    return get()->devno < other.get()->devno;
}
