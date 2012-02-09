/*
 * eventcatcher.hh - Handle audit event from Listener
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

#ifndef EVENT_CATCHER_HH
#define EVENT_CATCHER_HH

#include "fileptr.hh"
#include "common.hh"
#include "signals.hh"

#include <string>
#include <deque>
#include <set>

class AuditEvent;

class EventCatcher
{
    public:
        virtual void handleAuditEvent(boost::shared_ptr<AuditEvent> event) = 0;
};

/*
 * Scan all filesystem accessed files
 * collect all files in one list to retain access order
 *
 * ScanFsAccess resolve  file links
 */
class ScanFsAccess : public EventCatcher
{
    public:
        void observeApp(std::string);
        std::deque<FilePtr> getFileList();
    protected:
        virtual void handleAuditEvent(boost::shared_ptr<AuditEvent>);
    private:
        void insert(FilePtr&);
        fs::path readLink(fs::path& path);
        fs::path getPath2RegularFile(fs::path& path);

        std::set<std::string> observe_apps;
        std::set<pid_t> observe_pids;
        std::deque<FilePtr> list;
};

#endif
