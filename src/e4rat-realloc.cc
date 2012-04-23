/*
 * e4rat-realloc.cc - Relevant file defragmentation tool
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

#include "defrag.hh"
#include "logging.hh"
#include "common.hh"
#include "config.hh"
#include "parsefilelist.hh"

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/limits.h>

#include <boost/foreach.hpp>

#define PID_FILE "/var/run/e4rat-realloc.pid"

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
"Usage: " PROGRAM_NAME "-realloc [ option(s) ] [ mode ] files(s)\n"
"\n"
"  OPTIONS:\n"
"    -V --version                    print version and exit\n"
"    -h --help                       print help and exit\n"
"    -v --verbose                    increment verbosity level\n"
"    -q --quiet                      set verbose level to 0\n"
"    -l --loglevel <number>          set log level\n"
"    -f --force                      force reallocating files\n"
"\n"
"  DEFRAGMENTATION MODES:\n"
"    -p --use-prealloc               Use pre-allocation kernel patch\n"
"    -g --use-locality-group         Create donor files in locality group.\n"
"                                    Please avoid other filesystem activities.\n"
"    -t --use-tld                    Take advantage of orlov's top-level direc-\n"
"                                    tory spreading algorithm. This typically\n"
"                                    leads to small holes between the files.\n\n"
        ;
}

int main(int argc, char* argv[])
{
    Optimizer optimizer;

    Config::instance()->load();
    
    int loglevel = Config::get<int>("loglevel");
    int verbose  = Config::get<int>("verbose");

    std::vector<FileInfo> filelist;

    static struct option long_options[] =
        {
            {"verbose",            no_argument,       0, 'v'},
            {"version",            no_argument,       0, 'V'},
            {"quiet",              no_argument,       0, 'q'},
            {"help",               no_argument,       0, 'h'},
            {"loglevel",           required_argument, 0, 'l'},
            {"force",              no_argument,       0, 'f'},
            {"use-prealloc",       no_argument,       0, 'p'},
            {"use-locality-group", no_argument,       0, 'g'},
            {"use-tld",            no_argument,       0, 't'},
            {0, 0, 0, 0}
        };

    
    char c;
    int option_index = 0;
    while ((c = getopt_long (argc, argv, "Vvhql:fpgt", long_options, &option_index)) != EOF)
    {
        switch(c)
        {
            case 'h':
                goto out;
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
            case 'f':
                Config::set("force", true);
                break;
            case 'p':
                Config::set("defrag_mode", "pa");
                break;
            case 'g':
                Config::set("defrag_mode", "locality_group");
                break;
            case 't':
                Config::set("defrag_mode", "tld");
                break;
            default:
                std::cerr << "Unrecognised option: " << optopt << std::endl;
                goto out;
        }
    }

    logger.setVerboseLevel(verbose);
    logger.setLogLevel(loglevel);

    if(getuid() != 0)
    {
        std::cerr << "You need root privileges to run this program.\n";
        exit(1);
    }

    if(!createPidFile(PID_FILE))
    {
        std::cerr << "It seems that e4rat-realloc is already running.\n";
        std::cerr << "Remove pid file " << PID_FILE << " to unlock.\n";
        exit(1);
    }
    /*
     * Register signal handler
     */
    struct sigaction sa;
    memset(&sa, '\0', sizeof(struct sigaction));
    sa.sa_handler = signalHandler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    try {
        FILE* file;
        /*
         * parse file list given as arguments
         */
        for(int i=optind; i < argc; i++)
        {
            file = fopen(argv[i], "r");
            if(NULL == file)
                warn("File %s does not exist.", argv[i]);
            else
            {
                notice("Parsing file %s", argv[i]);
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
             notice("Parsing from stdin");
             parseInputStream(stdin, filelist);
        }
        
        if(filelist.empty() && optind == argc)
        {
            file = fopen("./e4rat-collect.log", "r");
            if(NULL != file)
            {
                notice("Parsing file ./e4rat-collect.log");
                parseInputStream(file, filelist);
                fclose(file);
            }
            else 
	        goto out;
        }
        //type casting necessary not to break strict-aliasing rules
        std::vector<fs::path>* fp = (std::vector<fs::path>*)&filelist;
        optimizer.relatedFiles(*fp);
    }
    catch(std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    unlink(PID_FILE);
    return 0;
out:
    unlink(PID_FILE);
    printUsage();
    exit(1);
}
