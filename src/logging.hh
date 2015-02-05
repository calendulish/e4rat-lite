/*
 * logging.hh - Display screen and or send event to klog or syslog
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
 *
 * 
 * To log events use the following macros:
 *    error (format, ...)   Error Condition. Cannot continue.
 *    warn  (format, ...)   Something stupid happen. Continue executing.
 *    notice(format, ...)   Results of a succeeded task.
 *    info  (format, ...)   Messages during a task.
 *    debug (format, ...)   Debug messages.
 *
 * According to loglevel and verbose messages will be send
 * to syslog or will shown on screen.
 *
 * Queue Messages in Logging::queue if syslog daemon is not running.
 * This helps to read logging events at early startup process. To
 * indicate syslogd is running test for existence of /dev/log.
 * Usually syslogd does not remove /dev/log when they terminated.
 * It's your own fault if you stops syslogd manually and wonder of
 * lacking log events.
 * 
 * Error and Warning messages will be send to stderr,
 * others to stdout.
 */

#ifndef LOGGING_HH
#define LOGGING_HH

#include "intl.hh"

#include <ostream>
#include <deque>

enum LogLevel
{
    Error  = 1,
    Warn   = 2,
    Notice = 4,
    Info   = 8,
    Debug  = 16
};

struct QueuedEvent
{
        QueuedEvent(LogLevel l, std::string m)
            : level(l), msg(m) {}
        LogLevel    level;
        std::string msg;
};

class Logging
{
    public:
        Logging();
        ~Logging();
        void setLogLevel(int l);
        void setVerboseLevel(int v);
        void write(LogLevel level, const char* format, ...);
        void errStream(FILE*);
        void outStream(FILE*);
        void redirectStdout2Stderr(bool);
    private:
        void dumpQueue();
        bool targetAvailable();
        void log2target(LogLevel level, const char* msg);
        bool redirectOut2Err;
        bool displayToolName;
        int loglevel;
        int verboselevel;
        /*
         * Because the heap based Config object is destroyed a little earlier
         * cache the target path to be able to dump queued messages in destructor
         */
        std::string target;
        std::deque<QueuedEvent> queue;
};

extern Logging logger;

#define dump_log(...) logger.write(__VA_ARGS__)

#ifdef DEBUG_ENABLED
    #define debug(format,args...) dump_log(Debug, "%s:%d in %s(): "format, __FILE__, __LINE__, __FUNCTION__, ## args)
#else
    #define debug(format,args...)
#endif

#define error(...) dump_log(Error, __VA_ARGS__)
#define warn(...) dump_log(Warn, __VA_ARGS__)
#define info(...) dump_log(Info, __VA_ARGS__)
#define notice(...) dump_log(Notice, __VA_ARGS__)

#endif
