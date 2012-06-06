/*
 * common.hh - Collection of common functions and classes 
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
#include "logging.hh"

#include <iostream>
#include <fstream>
#include <sstream>
#include <linux/fs.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <errno.h>
#include <libintl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mntent.h>
#include <execinfo.h>
#include <signal.h>

#define _(x) gettext(x)

/*
 * Setup SIGABRT and SIGSEGV signal handler for backtracing
 */
static void __attribute__((constructor)) setup_kill_signals()
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signalHandler;
    action.sa_flags = SA_SIGINFO;

    if(sigaction(SIGSEGV, &action, NULL) < 0)
        perror("sigaction");
    if(sigaction(SIGABRT, &action, NULL) < 0)
        perror("sigaction");
}

/*
 * Print backtrace to stderr.
 */
void printBacktrace()
{
    int j, nptrs;
#define SIZE 100
    void *buffer[100];
    char **strings;

    nptrs = backtrace(buffer, SIZE);
    std::cerr << _("backtrace() returned ") << nptrs << _(" addresses\n");

   /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */

   strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

   for (j = 0; j < nptrs; j++)
       std::cerr << strings[j] << std::endl;

   free(strings);
}

/*
 * This is the default signal handler funktion on UNIX signals.
 *
 * Print backtrace on SIGABRT or SIGSEGV signal. SIGABRT is send when an assert() failed.
 * Assert calls abort(3), which raise SIGABRT before forcing process termination.
 * It does this by restoring the default disposition for SIGABRT and then raising
 * the signal for a second time.
 *
 * On all other signals, stop execution when process reaches
 * an InterruptAble::interruptionPoint().
 */
void signalHandler(int signum)
{
    if(signum == SIGABRT || signum == SIGSEGV)
    {
        std::cerr <<  strsignal(signum) << std::endl;
         printBacktrace();
        exit(1);
    }

    Interruptible::interrupt();
}

/*
 * To support redirecting file-streams to stdin at startup
 * we have to set stdin to non-blocking.
 */
void setStdIn2NonBlocking()
{
    int cinfd = fileno(stdin);

    const int fcflags = fcntl(cinfd,F_GETFL);

    if (fcflags<0)
    {
        std::cerr << _("cannot read stdin flags\n");
    }

    if (fcntl(cinfd,F_SETFL,fcflags | O_NONBLOCK) <0)
    {
        std::cerr << _("cannot set stdin to non-blocking\n");
    }

    std::cin.exceptions ( std::ifstream::eofbit
                          | std::ifstream::failbit
                          | std::ifstream::badbit );
}

/*
 * Convert string containing wildcard characters to a valid boost regular expression.
 *   1: Replace . with \. (i.e. escape it)
 *   2: Replace any ? with .
 *   3: Replace any * with .*
 */
const boost::regex path2regex(std::string path)
{
    return boost::regex(
        boost::regex_replace(path,
                             boost::regex("(\\.)|(\\?)|(\\*)"),
                             "(?1\\\\.)(?2.)(?3.*)",
                             boost::match_default | boost::format_all ));
}

/*
 * return list of paths match filesearch filter
 * filesearch string may consist of simple wildcards:
 *    VALID:   /dev/?da*
 *    INVALID: * /bin/ *   (inserted spaces to avoid and of comment)
 */
std::vector<std::string> matchPath( const std::string & filesearch )
{
    std::vector<std::string> fileset;
    // Initialize path p (if necessary prepend with ./ so current directory is used) 
    fs::path p(filesearch);
    if( !fs::path(filesearch).has_parent_path() )
    {
        p = "./";
        p /= filesearch;
    }
    // Initialize regex filter - use * as default if nothing is given in filesearch
    std::string f( p.has_filename() ? p.filename() : "*");

    fs::path dir(system_complete(p).parent_path());
    if( is_directory(dir) )
        // Iterate through contents (files/directories) of directory...
        for( boost::filesystem::directory_iterator it(dir);
             it!=boost::filesystem::directory_iterator();
             ++it )
        {
            if( boost::regex_match( it->leaf(), path2regex(f) ) )
                fileset.push_back(it->string());
        }
    return fileset;
}

/*
 * Determine full path.
 * If base path is empty use current_path() at the time of entry to main().
 * Resolve "." and ".." and merge with working directory
 */
fs::path realpath(fs::path ph, fs::path base)
{
    fs::path mypath;
    if(!base.has_root_directory())
        base.clear();
    /*
     * boost::filesystem2::compare() throw an assert on path names like: "//.esd.conf"
     * I don't know why but path::is_complete() returns false while path::has_root_name()
     * is true. 
     * Fix this by removing leading double slash.
     */
    while(ph.string().size() > 1 && !ph.string().compare(0,2, "//"))
      ph = ph.string().substr(1);

    while(base.string().size() > 1 && !base.string().compare(0,2, "//"))
      base = base.string().substr(1);

    /*
     * combine base and path to full path
     */
    if(base.empty())
        mypath = complete(ph); //base is current working directory
    else
        mypath = complete(ph, base);
    
    fs::path result;

    /*
     * Clean up full path by resolving ".." and "." directories.
     */
    for(fs::path::iterator it = mypath.begin();
        it != mypath.end();
        it++)
    {
        if(*it == "..")
            result = result.parent_path();
        else if(*it == ".")
            continue;
        else
            result /= (*it);
    }

    return result;
}

/*
 * Get path of file opened on file descriptor fd.
 * Call readlink() on /proc/self/fd/<fd>
 */
std::string getPathFromFd(int fd)
{
    char __filename[PATH_MAX];
    char __path2fd[1024];
    memset(__filename, '\0', PATH_MAX);
    sprintf(__path2fd, "/proc/self/fd/%d", fd);
        
    if(-1 == readlink(__path2fd, __filename, PATH_MAX))
    {
        std::stringstream ss;
        ss << _("Cannot readlink: ")
           << fd
           << ": " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
    return __filename;
}

/*
 * Read pid number from pid file
 */ 
pid_t readPidFile(const char* path)
{
    int tmp;

    FILE* file = fopen(path, "r");
    if(NULL == file)
        return 0;

    if(EOF == fscanf(file, "%d", &tmp))
        return 0;

    fclose(file);

    return tmp;
}

/*
 * Create pid file
 * Return true on success
 *        false file already exists
 */ 
bool createPidFile(const char* path)
{
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if(-1 == fd)
    {
        if(errno == EEXIST)
            return false;
        else if(errno == EROFS)
        {
            error(_("Cannot create pid file %s on read-only filesystem"), path);
            return true;
        }
        else
        {
            error(_("Cannot open pid file: %s"), strerror(errno));
            return false;
        }
    }

    FILE* file = fdopen(fd, "w");
    if(NULL == file)
    {
        error(_("Cannot open pid file %s: %s"), path, strerror(errno));
        return false;
    }

    fprintf(file, "%d", getpid());
    fclose(file);

    return true;
}


bool Interruptible::interrupted = false;

void Interruptible::interrupt()
{
    interrupted = true;
}

void Interruptible::interruptionPoint()
{
    if(interrupted)
        throw UserInterrupt();
}
