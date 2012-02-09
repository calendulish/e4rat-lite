/*
 * e4rat-preload.cc - Preload files into page cache
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
#include "config.hh"
#include "logging.hh"
#include "fiemap.hh"
#include "parsefilelist.hh"

#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/limits.h>

#include <boost/foreach.hpp>

#define SORT 1

int flags;

class FileInfo;
std::vector<FileInfo> filelist;

class FileInfo
{
    public:
        FileInfo(fs::path p)
            : path(p)
        {
            dev = ino = 0;
        }
        FileInfo(dev_t d, ino_t i, fs::path p): dev(d), ino(i), path(p) {}

        bool operator<(const FileInfo& other) const
        {
            if(this->dev < other.dev)
                return true;
            else if(this->dev == other.dev)
                if(this->ino < other.ino)
                    return true;
            
            return false;
        }
        dev_t dev;
        ino_t ino;
        fs::path path;
};


void sortFileList()
{
    std::map<__u64, FileInfo> filemap;
    typedef std::pair<__u64, FileInfo> map_t;
    
    BOOST_FOREACH(FileInfo& file, filelist)
    {
        struct fiemap* fmap = get_fiemap(file.path.string().c_str());
        if(!fmap)
            continue;
        
        filemap.insert(map_t(
                           fmap->fm_extents[0].fe_physical,
                           file));
    }

    filelist.clear();

    BOOST_FOREACH(map_t i, filemap)
        filelist.push_back(i.second);
}

void dumpFileList(std::ostream& out)
{
    BOOST_FOREACH(FileInfo& file, filelist)
        out << file.dev << '\t' << file.ino << '\t' << file.path.string() << std::endl;
}

void preloadInodes()
{
    struct stat st;
    std::vector<FileInfo> filelist_sorted = filelist;
    std::sort(filelist_sorted.begin(), filelist_sorted.end());

    BOOST_FOREACH(FileInfo& file, filelist_sorted)
    {
        lstat(file.path.string().c_str(), &st);
    }
}

void preloadFiles()
{
    int fd;
    BOOST_FOREACH(FileInfo& file, filelist)
    {
        fd = open(file.path.string().c_str(), O_RDONLY | O_NOFOLLOW);
        if(-1 == fd)
            continue;

        readahead(fd, 0, 1024*1024*100);
        close(fd);
    }
}

void printUsage()
{
    std::cout <<
"Usage: " PROGRAM_NAME "-preload [ option(s) ] file(s)\n"
"\n"
"    -V --version                    print version and exits\n"
"    -h --help                       print help and exits\n"
"    -v --verbose                    increment verbosity level\n"
"    -q --quiet                      set verbose level to 0\n"
"    -l --loglevel <number>          set log level\n"
"\n"
"    -o --output <file>              dump generated file list to file\n"
"    -s --sort                       Sort file list and exit\n"
"    -x --execute <command>          execute while pre-loading\n"
"\n"
        ;
}
int main(int argc, char* argv[])
{
    flags = 0;

    Config::instance()->load();

    int loglevel = Config::get<int>("loglevel");
    int verbose  = Config::get<int>("verbose");

    const char* execute = NULL;
    const char* outPath = NULL;
    FILE* outStream     = NULL;

    static struct option long_options[] =
        {
            {"version",  no_argument,       0, 'V'},
            {"help",     no_argument,       0, 'h'},
            {"verbose",  no_argument,       0, 'v'},
            {"quiet",    no_argument,       0, 'q'},
            {"loglevel", required_argument, 0, 'l'},
            {"output",   required_argument, 0, 'o'},
            {"sort",     no_argument,       0, 's'},
            {"execute",  required_argument, 0, 'x'},
            {0, 0, 0, 0}
        };

    char c;
    int option_index = 0;
    opterr = 0;
    while ((c = getopt_long (argc, argv, "Vhvql:o:x:s", long_options, &option_index)) != EOF)
    {
        // parse optional arguments
        if(optarg != NULL && optarg[0] == '-')
        {
            optopt = c;
            c = '?';
            --optind;
        }
        switch(c)
        {
            case 'h':
                printUsage();
                return 0;
            case 'V':
                std::cout << PROGRAM_NAME << " " << VERSION << std::endl;
                return 0;
            case 'v':
                verbose <<= 1;
                verbose |= 1;
                break;
            case 'q':
                verbose = 0;
                break;
            case 'l':
                loglevel = atoi(optarg);
                break;
            case 's':
                flags |= SORT;
                break;
            case 'o':
                outPath = optarg;
                break;
            case 'x':
                execute = optarg;
                break;
            case '?':
                if (optopt == 'o') // optional parameter for output is missing
                    outStream = stdout;
                else
                {
                    for(int i=0; long_options[i].val; i++)
                        if(long_options[i].val == optopt)
                        {
                            fprintf(stderr, "Option requires an argument -- '%c'\n", optopt);
                            exit(1);
                        }
                    fprintf(stderr, "Unrecognised option --  '%c'\n", optopt);
                    
                    return -1;
                }
                break;
        }
    }

    logger.setVerboseLevel(verbose);
    logger.setLogLevel(loglevel);

    if(NULL == outPath && flags & SORT)
        logger.redirectStdout2Stderr(true);

    try {
    if(getpid() == 1)
    {
        const char* logfile = Config::get<std::string>("startup_log_file").c_str();
        notice("Open %s ... ", logfile);
        FILE* infile = fopen(logfile, "r");
        if(!infile)
        {
            error("%s is not accessible", logfile);
            execv(Config::get<std::string>("init").c_str(), argv);
        }
        
        parseInputStream(infile, filelist);
        fclose(infile);
    }
    else
    {
        if(flags & SORT && !outPath && !outStream)
        {
            for(int i=optind; i < argc; i++)
            {
                notice("Parsing file %s ...", argv[i]);
                FILE* file = fopen(argv[i], "r");
                if(NULL == file)
                    warn("File %s does not exist", argv[i]);
                else
                {
                    parseInputStream(file, filelist);
                    fclose(file);
                    sortFileList();
                    file = fopen(argv[i], "w");
                    if(NULL == file)
                        warn("Cannot save sorted list to %s: %s", argv[i], strerror(errno));
                    else
                    {
                        notice("Save sorted file list to %s ...", argv[i]);
                        BOOST_FOREACH(FileInfo& f, filelist)
                            fprintf(file, "%u %u %s\n", (__u32)f.dev, (__u32)f.ino, f.path.string().c_str());
                        fclose(file);
                    }
                    filelist.clear();
                }
            }
            return 0;
        }
        else
        {
            for(int i=optind; i < argc; i++)
            {
                notice("Parsing file %s ...", argv[i]);
                FILE* file = fopen(argv[i], "r");
                if(NULL == file)
                    warn("File %s does not exist", argv[i]);
                else
                {
                    parseInputStream(file, filelist);
                    fclose(file);
                }
            }
            setStdIn2NonBlocking();
            if(EOF != peek(stdin))
            {
                notice("Parsing from stdin ...");
                parseInputStream(stdin, filelist);
	    }
            if(filelist.empty())
                goto out;
        }
    }
    }
    catch(std::exception& e)
    {
        error(e.what());
        return 1;
    }
    notice("%d files scanned", filelist.size());

    if(flags & SORT)
    {
        notice("Sort total file list ...");
        sortFileList();

        // Dump sorted list
        if(outPath && !outStream)
        {
            outStream = fopen(outPath, "w");
        }
        if(NULL == outStream)
            error("Invalid output file: %s: %s", outPath, strerror(errno));
        else
        {
            if(outStream != stdout)
                notice("Save sorted file list to %s ...", outPath);
            
            BOOST_FOREACH(FileInfo& file, filelist)
                fprintf(outStream, "%u %u %s\n", (__u32)file.dev, (__u32)file.ino, file.path.string().c_str());
            fclose(outStream);
        }
        return 0;
    }

    /*
     * Preload Inodes
     */
    notice("Pre-loading I-Nodes ...");
    preloadInodes();

    /*
     * Run init or command and proceed execution on child process
     */
    if(execute || 1 == getpid())
    {
        if(execute)
            notice("Execute `%s' ...", execute);
        else
            notice("Execute `%s' ...", Config::get<std::string>("init").c_str());

        switch(fork())
        {
            case -1:
                std::cerr << "Fork failed! " << strerror(errno) << std::endl;
                exit(1);
                break;
            case 0: //child process
                break;
            default:
                if(execute)
                {
                    if(-1 == system(execute))
                        error("system: %s", strerror(errno));
                }
                else
                  execv(Config::get<std::string>("init").c_str(), argv);

                exit(0);
         }
    }

    notice("Pre-loading file content ...");
    preloadFiles();

    notice("Successfully transferred files into page cache");
    exit(0);
out:
    printUsage();
    exit(1);
}
