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

#ifndef COMMON_HH
#define COMMON_HH

#include "intl.hh"

#include <linux/types.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

namespace fs = boost::filesystem;

void setStdIn2NonBlocking();

/*
 * file operations
 */
const boost::regex path2regex(std::string path);
std::vector<std::string> matchPath( const std::string & filesearch );
fs::path realpath(fs::path _path, fs::path _cwd = "");
std::string getPathFromFd(int fd);

/*
 * pid file operations
 */
pid_t readPidFile(const char* path);
bool createPidFile(const char* path);

/*
 * UserInterrupt is an exception thrown by non-multi-threaded applications.
 * The idea behind exceptions is to clean up memory and other system resources.
 */
class UserInterrupt : public std::exception
{
    public:
        UserInterrupt()
            : msg("User interrupt")
        {}
        virtual const char* what() const throw()

        {
            return msg;
        }
    private:
        const char* msg;
};

/*
 * Declare class interruptible
 */
class Interruptible
{
    public:
        static void interrupt();
    protected:
        void interruptionPoint();
    private:
        static bool interrupted;
};

void signalHandler(int signum);

#endif
