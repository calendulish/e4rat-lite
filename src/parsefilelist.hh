/*
 * parselist.cc - Parse file list
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

/*
 * Parse file list from file stream
 */

#include "logging.hh"
#include <sstream>
#include <libintl.h>

#define _(x) gettext(x)

int peek(FILE* in)
{
    int c = fgetc(in);
    ungetc(c, in);

    return c;
}

template<typename T>
void parseInputStream(FILE* in, std::vector<T>& filelist)
{
    bool detailed = true;
    int ret;

    int lineno = 0;
    int dev = 0;
    __u64 ino;
    char path[PATH_MAX];

    int c = peek(in);
    if(c == EOF)
        return;
    else if(c == '/')
        detailed = false;

    while(1)
    {
        lineno++;

        if(detailed)
            ret = fscanf(in, "%d %llu %[^\n]s", &dev, &ino, path);
        else
            ret = fscanf(in, "%[^\n]s", path);

        if(ret == EOF || ret == 0)
            break;

        if((detailed && ret != 3)
           || (!detailed && ret != 1))
        {
            std::stringstream ss;
            ss << _("Error while parsing ") << getPathFromFd(fileno(in)) << ".\n"
               << _("Syntax error at line ") << lineno << _(" argument ") << ret+1;
            throw std::runtime_error(ss.str());
        }
        if(!detailed)
            filelist.push_back(T(path));
        else
            filelist.push_back(T(dev,ino,path));
    }
}
