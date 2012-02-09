/*
 * logging.cc - Display screen and or send event to klog or syslog
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

#include "logging.hh"
#include "config.hh"

#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

Logging logger;

void Logging::log2target(LogLevel level, const char* msg)
{
    if(target == "syslog")
    {
        if(access(_PATH_LOG, F_OK))
            throw std::runtime_error("syslog daemon is not running");
        syslog((level/2)+2, msg, NULL);
    }
    else
    {
        if(target == "/dev/kmsg")
            if(access(target.c_str(), W_OK))
                throw std::runtime_error(strerror(errno));
         
        FILE* file = fopen(target.c_str(), "a");
        if(!file)
            throw std::runtime_error(strerror(errno));
        
        fprintf(file, "[%s] %s\n", Config::get<std::string>("tool_name").c_str(), msg);
        fclose(file);
    }
}

Logging::Logging()
{
    redirectOut2Err = false;
    verboselevel = Error;
    loglevel = Error;
    openlog(NULL, LOG_PID, 0);
    
    if(getpid() == 1)
        displayToolName = true;
    else
        displayToolName = false;

    target = Config::get<std::string>("log_target");
}

Logging::~Logging()
{
    try {
        dumpQueue();
    }
    catch(std::exception& e)
    {
        fprintf(stderr, "Cannot dump log messages: %s: %s\n",
                target.c_str(),
                e.what());
    }

    if(queue.size())
        fprintf(stderr, "Discard %zu unwritten log message(s).\n", queue.size());
}

void Logging::setLogLevel(int l)
{
    loglevel = l;
}

void Logging::setVerboseLevel(int v)
{
    verboselevel = v;
}

void Logging::dumpQueue()
{
    while(queue.size())
    {
        QueuedEvent& event = queue.front();
        log2target(event.level, event.msg.c_str());
        queue.pop_front();
    }
}
void Logging::write(LogLevel level, const char* format, ...)
{
#define MSG_SIZE 8192
    char msg[MSG_SIZE];
    va_list args;
    va_start(args, format);

    vsnprintf(msg, MSG_SIZE, format, args);
    
    if((level & verboselevel))
    {
        FILE* out;
        if(redirectOut2Err || level == Error || level == Warn)
            out = stderr;
        else
            out = stdout;
        
        if(displayToolName)
            fprintf(out, "[%s] %s\n", Config::get<std::string>("tool_name").c_str(), msg);
        else
            fprintf(out, "%s\n", msg);
    }

    if(!(level & loglevel))
        goto out;

    try {
        target = Config::get<std::string>("log_target");
        dumpQueue();
        log2target(level, msg);
    }
    catch(...)
    {
        queue.push_back(QueuedEvent(level, msg));
    }
    
out:
    va_end(args);    
}

void Logging::redirectStdout2Stderr(bool s)
{
    redirectOut2Err = s;
}
