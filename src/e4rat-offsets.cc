/*
 * e4rat-offsets.cc - display physical block allocation and their offset
 *                 of a list of files
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

#include "common.hh"
#include "fiemap.hh"
#include "parsefilelist.hh"

#include <iostream>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <linux/limits.h>
#include <boost/foreach.hpp>

/*
 * FileInfo is a wrapper class of fs::path
 * Constructor are necessary to use common file list parser
 */
class FileInfo : public boost::filesystem::path
{
    public:
        FileInfo(dev_t, ino_t, fs::path p)
            : boost::filesystem::path(p) {}
        FileInfo(fs::path p)
            : boost::filesystem::path(p) {}
};

void printUsage()
{
    std::cout <<
        "Usage: "PROGRAM_NAME"-offsets [file(s)]\n"
        ;
}
int main(int argc, char* argv[])
{
    struct fiemap* fmap;
    int fd;
    int prev_block = 0;
    int l = 13;

    
    std::vector<FileInfo> filelist;

    try {
        FILE* file;
        /*
         * parse file list given as arguments
         */
        for(int i=optind; i < argc; i++)
        {
            file = fopen(argv[i], "r");
            if(NULL == file)
                fprintf(stderr, "File %s does not exists: %s\n", argv[i], strerror(errno));
            else
            {
                printf("Parsing file %s\n", argv[i]);
                parseInputStream(file, filelist);
                fclose(file);
            }
        }

        /*
         * parse file list on stdin
         */
        setStdIn2NonBlocking();
        if(EOF != peek(stdin))
        {
             printf("Parsing from stdin\n");
             parseInputStream(stdin, filelist);
        }
    }
    catch(std::exception&e )
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    if(filelist.empty())
        goto out;

    printf("%*s%*s%*s%*s%*s   %s\n", 3, "ext", l, "start",l, "end",l, "length", l, "offset", "file");
    
    BOOST_FOREACH(fs::path& file, filelist)
    {
        fd = open(file.string().c_str(), O_RDONLY | O_NOFOLLOW);
        if(-1 == fd)
        {
            std::cerr << "Cannot open file: "
                      << file.string() << ": "
                      << strerror(errno) << std::endl;
            continue;
        }
        
        fmap = ioctl_fiemap(fd);
        if(NULL == fmap)
        {
            std::cerr << "Cannot receive file extents: "
                      << file.string() << ": "
                      << strerror(errno) << std::endl;
            close(fd);
            continue;
        }

        for(__u32 i = 0; i < fmap->fm_mapped_extents; i++)
        {
            int start = fmap->fm_extents[i].fe_physical>>12;
            int end   = start + (fmap->fm_extents[i].fe_length>>12) - 1;
            if(1 == fmap->fm_mapped_extents)
                printf("%*s", 3, " ");
            else
                printf("%*d", 3, i+1);
            printf("%*d", l, start);
            printf("%*d", l, end);
            printf("%*d", l, end - start + 1);
            printf("%*d", l, start - prev_block  -1);
            prev_block = end;
            if(0 == i)
                printf("   %s", file.string().c_str());
            printf("\n");
                    
        }
        free(fmap);
        close(fd);
    }
    exit(0);
out:
    printUsage();
    exit(1);
}
