/*
 * fileptr.hh - Pointer to an unique file object
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

#ifndef FILEPTR_HH
#define FILEPTR_HH

#include "singleton.hh"

#include <string>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;


struct FilePtrPrivate
{
        friend class FilePtr;
    private:
        FilePtrPrivate();
        ino_t ino;
        dev_t dev;
        unsigned int flags;
        fs::path path;
        bool valid;
};

typedef boost::weak_ptr<FilePtrPrivate> WeakFilePtr;

class FilePtr : public boost::shared_ptr<FilePtrPrivate>
{
        typedef std::map<ino_t,WeakFilePtr> map_t;
        
    public:
        FilePtr(dev_t, ino_t, fs::path, bool = true);
        FilePtr(fs::path, bool = true);
    public:
        FilePtr();
        ~FilePtr();
        FilePtr(boost::shared_ptr<FilePtrPrivate>& other)
            : boost::shared_ptr<FilePtrPrivate>(other)
        {
            
        }
        void setInvalid();
        bool isValid() const;
        ino_t getInode() const;
        dev_t getDevice() const;
        const fs::path& getPath() const;
    private:
};

class FileDepot
{
        DECLARE_SINGLETON(FileDepot);
        friend class Collector;
        friend class FilePtr;
    public:
        struct key_t
        {
                dev_t dev;
                ino_t ino;
                bool operator<(const key_t&) const;
        };
        
        typedef std::pair<key_t, WeakFilePtr> files_pair_t;
        typedef std::map<key_t, WeakFilePtr> files_t;        
        typedef files_t::iterator files_it_t;
        
    protected:
        void insert(dev_t, ino_t, FilePtr&);
        void insert(FilePtr&);
        void remove(FilePtr);

    private:
        files_t files;
        inline void genKey(key_t*, dev_t, ino_t);
        void insert(key_t, FilePtr&);
};

#endif
