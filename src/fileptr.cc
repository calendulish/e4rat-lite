/*
 * fileptr.cc - Pointer to an unique file object
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

#include "fileptr.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdexcept>

bool isFileUnique(const char* path)
{
    struct stat st;
    if(stat(path, &st) < 0)
    {
        std::string err_msg = path;
        err_msg += ": Cannot receive file stat: ";
        err_msg += strerror(errno);
        throw std::runtime_error(err_msg);
    }
    
    FilePtr file(st.st_dev, st.st_ino, path);
    return file.unique();
}

DEFINE_SINGLETON(FileDepot);

FileDepot::FileDepot() 
{}
FileDepot::~FileDepot()
{}

bool FileDepot::key_t::operator<(const FileDepot::key_t& other) const
{
    if(this->dev < other.dev)
        return true;
    else if(this->dev == other.dev)
        if(this->ino < other.ino)
            return true;

    return false;
}

void FileDepot::insert(key_t key, FilePtr& file)
{
    std::pair<files_it_t, bool> ret = files.insert(files_pair_t(key, file));
    if(ret.second == false)
    {
        boost::shared_ptr<FilePtrPrivate> a = ret.first->second.lock();
        file = a;
    }
}

void FileDepot::insert(dev_t dev, ino_t ino, FilePtr& file)
{
    key_t key;
    genKey(&key, dev, ino);
    insert(key, file);
}

void FileDepot::insert(FilePtr& file)
{
    key_t key;
    struct stat st;

    if(stat(file.getPath().string().c_str(), &st) == 0)
    {
        genKey(&key, st.st_dev, st.st_ino);
        insert(key, file);
    }
}

void FileDepot::remove(FilePtr file)
{
    struct stat st;
    key_t key;
    
    if(!file)
        return;
    fs::path p = file.getPath();
    
    if(stat(file.getPath().string().c_str(), &st) == 0)
    {
        genKey(&key, st.st_dev, st.st_ino);
        files.erase(key);
    }
}


void FileDepot::genKey(key_t* key, dev_t dev, ino_t ino)
{
    key->dev = dev;
    key->ino = ino;
}

FilePtrPrivate::FilePtrPrivate()
{
    ino = dev = flags = 0;
    valid = true;
}

FilePtr::FilePtr(dev_t dev, ino_t ino, fs::path path, bool valid)
    : boost::shared_ptr<FilePtrPrivate>(new FilePtrPrivate)
{
    get()->path = path;
    get()->dev = dev;
    get()->ino = ino;
    FileDepot::instance()->insert(dev, ino, *this);

    if(false == valid)
        get()->valid = false;
}

FilePtr::FilePtr(fs::path path, bool valid)
    : boost::shared_ptr<FilePtrPrivate>(new FilePtrPrivate)
{
    struct stat st;
    if(stat(path.string().c_str(), &st) == 0)
    {
        get()->path = path;
        get()->dev = st.st_dev;
        get()->ino = st.st_ino;
        FileDepot::instance()->insert(st.st_dev, st.st_ino, *this);
        if(false == valid)
            get()->valid = false;
    }
}

FilePtr::FilePtr()
{}

FilePtr::~FilePtr()
{
    if(this->unique())
        FileDepot::instance()->remove(*this);
}

const fs::path& FilePtr::getPath() const
{
    return get()->path;
}

void FilePtr::setInvalid()
{
    get()->valid = false;
}
bool FilePtr::isValid() const
{
    return get()->valid;
}
ino_t FilePtr::getInode() const
{
    return get()->ino;
}

dev_t FilePtr::getDevice() const
{
    return get()->dev;
}
