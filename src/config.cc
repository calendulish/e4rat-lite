/*
 * config.cc - global settings and config file parser
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

#include "config.hh"
#include "logging.hh"
#include "common.hh"

#include <fcntl.h>
#include <iostream>

#include <boost/property_tree/info_parser.hpp>
#include <boost/foreach.hpp>

DEFINE_SINGLETON(Config);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
char** get_argv()
{
    char** ptr = environ;
    ptr--;
    while(ptr--)
        // Assumption argument strings does not have a leading '\0' character.
        // Therefore ptr points to argc.
        if(*(unsigned int*)ptr < 0x00FFFFFF)
            return ++ptr;
    return 0;
}
#else
#pragma message "Byte order big endian not fully supported."
#endif

Config::Config()
{
    defaultProperty.put("loglevel", Error | Warn);
    defaultProperty.put("verbose",  Error | Warn | Notice);
    defaultProperty.put("ext4_only", true);
    defaultProperty.put("defrag_mode", "auto");
    defaultProperty.put("exclude_open_files", true);
    defaultProperty.put("timeout", 120);
    defaultProperty.put("log_target", "/dev/kmsg");
    defaultProperty.put("init", "/sbin/init");
    defaultProperty.put("force", false);
    defaultProperty.put("startup_log_file", "/var/lib/e4rat/startup.log");

    /*
     * Set tool name by searching for argv[0]
     */
    std::string tool_name;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    size_t found;
    char**argv = get_argv();
    if(argv == NULL)
    {
        fprintf(stderr, "Cannot get argv arguments\n");
        return;
    }
    
    tool_name = fs::path(argv[0]).filename();
    found = tool_name.find_last_of("-");
    if(found)
        defaultSection = tool_name.substr(found+1);
    else
        defaultSection = tool_name;
#else
    tool_name = PROGRAM_NAME;
#endif

    defaultProperty.put("tool_name", tool_name);    
}

Config::~Config()
{}

void Config::load()
{
    try {
        if(access("/etc/"PROGRAM_NAME".conf", F_OK))
            return;

        read_info("/etc/"PROGRAM_NAME".conf", ptree);
#if 0
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, ptree)
            if(defaultProperty.find(v.first) == defaultProperty.not_found())
                std::cerr << "parse config file: unknown option: " << v.first.c_str() << std::endl;
#endif
    }

    catch(std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}

void Config::clear()
{
    ptree.clear();
}
