/*
 * buddycache.hh - Linux kernel ext4 buddy cache
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

#ifndef BUDDY_CACHE_HH
#define BUDDY_CACHE_HH

#include <vector>
#include "device.hh"

/*
 * Store free block ranges per block group
 */
struct BuddyGroup
{
        unsigned short free;
        unsigned short frags;
        unsigned short first;
        unsigned char  s0;
        unsigned char  s1;
        unsigned char  s2;
        unsigned char  s3;
        unsigned char  s4;
        unsigned char  s5;
        unsigned char  s6;
        unsigned char  s7;
        unsigned char  s8;
        unsigned char  s9;
        unsigned char  s10;
        unsigned char  s11;
        unsigned char  s12;
        unsigned char  s13;
};

/*
 * BuddyCache holds a snapshot of free block ranges on disk
 * by parsing the file /proc/fs/ext4/<partition>/mb_groups
 *
 * Unlike block bitmaps, buddy cache also marked pre-allocated space as used.
 * Therefore it reveals actual available free block ranges.
 *
 * The refresh()-method updates the snapshot.
 */
class BuddyCache
{
    public:
        BuddyCache(Device);

        //updates snapshot
        void refresh();

        //Return a reference to BuddyGroup of group id
        BuddyGroup& at(int group);

        //Return flex number of an empty flex
        int findEmptyFlex();

        //Return block group number of first found empty block group
        int findEmptyGroup();
    public:
        bool isFlexEmpty(__u32);
        bool isGroupEmpty(__u32);
        void scanGroup(unsigned int);
        Device device;
        std::vector<BuddyGroup> data;
};
        

#endif
