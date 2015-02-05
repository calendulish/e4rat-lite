/*
 * buddycache.cc - Linux Kernel ext4 buddy cache
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

#include "buddycache.hh"

#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>

BuddyCache::BuddyCache(Device _device)
    : device(_device)
{
    refresh();
}

void BuddyCache::refresh()
{
    if(data.size())
        data.clear();
    
    std::filebuf fb;
    if (NULL == fb.open ( (std::string("/proc/fs/ext4/")
                           + device.getDeviceName()
                           + "/mb_groups").c_str(), std::ios::in))

        throw std::runtime_error(std::string(
                 _("cannot open buddy cache on device ")
                 + device.getDeviceName()));

    
    std::istream buddy_stream(&fb);
    std::string line;
    int group;
    //skip first line
    getline (buddy_stream,line);
    
    for(unsigned int i = 0; i< device.getGroupCount(); i++)
    {
        getline (buddy_stream,line);
        struct BuddyGroup p;
        sscanf(line.c_str(), "#%d : %hu %hu %hu [ %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
               &group,&p.free, &p.frags, &p.first,
               (int*)&p.s0, (int*)&p.s1,(int*)&p.s2,(int*)&p.s3,(int*)&p.s4,(int*)&p.s5,
               (int*)&p.s6,(int*)&p.s7,(int*)&p.s8, (int*)&p.s9,(int*)&p.s10,(int*)&p.s11,
               (int*)&p.s12,(int*)&p.s13);
        data.push_back(p);
    }
}

BuddyGroup& BuddyCache::at(int i)
{
    return data.at(i);
}

/*
 * check weather flex group is empty
 */
bool BuddyCache::isFlexEmpty(__u32 flex)
{
    int bg_start = flex << device.getLogGroupsPerFlex();
    __u64 total_free = 0;

    for (__u32 i = bg_start;
         i < (flex << device.getLogGroupsPerFlex())
             && i < device.getGroupCount();
         i++)
    {
             total_free += data.at(i).free;
    }

    if(total_free == device.freeBlocksPerFlex())
        return true;
         
    return true;
}

int BuddyCache::findEmptyFlex()
{
    int total_flex_cnt = device.getGroupCount()<< device.getLogGroupsPerFlex();

    for(int i = 0; i < total_flex_cnt; i++)
    {
        if(isFlexEmpty(i))
            return i;
    }
    return -1;
}

bool BuddyCache::isGroupEmpty(__u32 group)
{
    int free_blocks =  device.getBlocksPerGroup();
    if(0 == group % (1<<device.getLogGroupsPerFlex()))
    {
        //first gb of flex contains bitmaps and inode table
        free_blocks -= 514;
    }
    if(data.at(group).free == free_blocks)
        return true;
    if(data.at(group).free > free_blocks)
        throw std::logic_error(_("more blocks marked free than expected"));

    return false;
}

int BuddyCache::findEmptyGroup()
{
    for(__u32 i = 0; i < device.getGroupCount(); i++)
        if(isGroupEmpty(i))
            return i;
    return -1;
}

