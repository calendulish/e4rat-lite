/*
 * listener.cc - Listen to the Linux audit socket
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

#include "listener.hh"
#include "common.hh"
#include "logging.hh"
#include "device.hh"

#include <libaudit.h>
#include <auparse.h>
#include <auparse-defs.h>
#include <errno.h>
#include <libintl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <linux/limits.h>
#include <fcntl.h>

//syscall table
#include <linux/unistd.h>
//mount flags
#include <linux/fs.h>
#include <sys/vfs.h>

#include <boost/foreach.hpp>
#include <sys/utsname.h>

#include <fstream>

#define _(x) gettext(x)

std::string getProcessName(pid_t pid)
{
    std::string comm;
    char filename[PATH_MAX];
    sprintf(filename, "/proc/%d/comm", pid);

    std::fstream filestr (filename, std::fstream::in);
    if(filestr.good())
        filestr >> comm;
    else
        comm = "unknown";

    return comm;
}

class DetectAuditDaemon : public std::exception
{};

void checkSocketCaptured(pid_t audit_pid)
{
    if(getpid() != audit_pid)
    {
        std::string comm = getProcessName(audit_pid);
        error(_("Process %s [%d] has captured the audit socket."), comm.c_str(), audit_pid);
        error(_("e4rat-lite-collect is in conflict with %s. Abort"), comm.c_str());
        throw DetectAuditDaemon();
    }
}

AuditEvent::AuditEvent()
{
    ino = 0;
    dev = 0;
    readOnly = false;
    successful = false;
}

AuditListener::AuditListener()
{
    audit_fd = -1;
    ext4_only = false;
}

AuditListener::~AuditListener()
{
}

void AuditListener::excludePath(std::string path)
{
    exclude_paths.push_back(
    path2regex(realpath(path).string()));
}

void AuditListener::watchPath(std::string path)
{
    if(path == "/")
        // does not make sense and can leads to unwanted behaviour
        return;

    watch_paths.push_back(
    path2regex(realpath(path).string()));
}

void AuditListener::excludeDevice(std::string wildcard)
{
    struct stat st;
    std::vector<std::string> matches = matchPath(wildcard);

    if(matches.empty())
        error(_("%s: no such file or directory"), wildcard.c_str());

    BOOST_FOREACH(std::string d, matches)
    {
        if( 0 > stat(d.c_str(), &st))
            continue;
        exclude_devices.insert(st.st_rdev);
    }
}

void AuditListener::watchDevice(std::string wildcard)
{
    struct stat st;

    std::vector<std::string> matches = matchPath(wildcard);

    if(matches.empty())
        error(_("%s: no such file or directory"), wildcard.c_str());

    BOOST_FOREACH(std::string d, matches)
    {
        if( 0 > stat(d.c_str(), &st))
            continue;
        watch_devices.insert(st.st_rdev);
    }
}

void AuditListener::watchExt4Only(bool v)
{
    ext4_only = v;
}

void AuditListener::watchFileSystemType(long t)
{
    watch_fs_types.insert(t);
}

void addSyscall(struct audit_rule_data* rule, const char* sc, int machine)
{
    int syscall_nr;
    syscall_nr = audit_name_to_syscall(sc, machine);
    if(syscall_nr == -1)
        throw std::logic_error(_("Cannot convert syscall to number"));

    audit_rule_syscall_data(rule, syscall_nr);
}

void AuditListener::activateRules(int machine)
{
    char field[128];
    struct audit_rule_data* rule = (struct audit_rule_data*) calloc(1, sizeof(audit_rule_data));

    addSyscall(rule, "execve", machine);
    addSyscall(rule, "open", machine);
    addSyscall(rule, "openat", machine);
    addSyscall(rule, "truncate", machine);
    if(machine == MACH_X86)
        addSyscall(rule, "truncate64", machine);
    addSyscall(rule, "creat", machine);
    addSyscall(rule, "mknod", machine);
    addSyscall(rule, "fork", machine);
    addSyscall(rule, "vfork", machine);
    addSyscall(rule, "clone", machine);

#if 0
    /*
     * TODO: filetype=file works in most cases except for a stupid pid file.
     *       Its creation does not get logged when filetype=file is enabled.
     *       Don't know why.
     */
    strcpy(field, "filetype=file");
    audit_rule_fieldpair_data(&rule, field, AUDIT_FILTER_EXIT);
#endif

    /*
     * Restrict to successful syscall events
     */
    strcpy(field, "success=1");
    if(0 > audit_rule_fieldpair_data(&rule, field, AUDIT_FILTER_EXIT))
        error(_("audit_rule_fieldpair_data failed: %s"), field);

    /*
     * Specify arch
     */
    strcpy(field, "arch=");
    strcat(field, audit_machine_to_name(machine));
    if(0 > audit_rule_fieldpair_data(&rule, field, AUDIT_FILTER_EXIT))
        error(_("audit_rule_fieldpair_data failed: %s"), field);

    /*
     * Insert rule
     */
    if ( 0 >= audit_add_rule_data(audit_fd, rule, AUDIT_FILTER_EXIT, AUDIT_ALWAYS))
        if(errno != EEXIST)
            error(_("Cannot insert rules: %s"), strerror(errno));

    rule_vec.push_back(rule);
}

/*
 * Apply audit rules to AUDIT_FILTER_EXIT filter.
 * Monitor all syscalls initialize or perfrom file accesses.
 */
void AuditListener::insertAuditRules()
{
    if(audit_fd < 0)
    {
        audit_fd = audit_open();
        if (-1 == audit_fd)
            throw std::logic_error(_("Cannot open audit socket"));
    }

    struct utsname uts;
    if(-1 == uname(&uts))
        throw std::logic_error(std::string(_("Cannot receive machine hardware name: ")) + strerror(errno));

    if(0 == strcmp(uts.machine, "x86_64"))
    {
        activateRules(MACH_86_64);
        activateRules(MACH_X86);
    }
    else if(0 == strcmp(uts.machine, "ppc64"))
    {
        activateRules(MACH_PPC64);
        activateRules(MACH_PPC);
    }
    else
    {
        int machine = audit_name_to_machine(uts.machine);
        if(-1 == machine)
            throw std::logic_error(std::string(_("Unknown machine hardware name "))+ uts.machine);
        activateRules(machine);
    }
}

void AuditListener::removeAuditRules()
{
    if (audit_fd < 0)
        return;

    BOOST_FOREACH(struct audit_rule_data* rule, rule_vec)
    {
        if ( 0 > audit_delete_rule_data(audit_fd,
                                        rule,
                                        AUDIT_FILTER_EXIT,
                                        AUDIT_ALWAYS))
        {
            debug(_("Cannot remove rules: %s"), strerror(errno));
        }
        free(rule);
    }
    rule_vec.clear();
}

void AuditListener::activateAuditSocket()
{
    if(0 > audit_set_pid(audit_fd, getpid(), WAIT_YES))
        error(_("Cannot set pid to audit"));

    //set 1 to enable auditing
    //set 2 to enable auditing and lock the configuration
    if(0 > audit_set_enabled(audit_fd, 1))
        error(_("Cannot enable audit"));

    if(0 > audit_set_backlog_limit(audit_fd, 256))
        audit_request_status(audit_fd);
}

void AuditListener::closeAuditSocket()
{
    if(0 > audit_set_enabled(audit_fd, 0))
        error(_("Cannot disable audit socket"));

    if(0 > audit_set_pid(audit_fd, 0, WAIT_NO))
        error(_("Cannot disable current pid"));

    audit_close(audit_fd);
    audit_fd = -1;
}

/*
 * parse value of an audit event name=value
 */
inline std::string AuditListener::parseField(auparse_state_t* au, const char* name)
{
    if(0 == auparse_find_field(au, name))
        return std::string();

    return std::string(auparse_get_field_str(au));
}

char convertHexToChar(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    else if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    throw c;
}

std::string hexString2Ascii(std::string& hex)
{
    std::string result;

    for(unsigned int i=0; i < hex.size(); i+=2)
        result.append(1, (convertHexToChar(hex.at(i))<<4) + convertHexToChar(hex.at(i+1)) );

    return result;
}

/*
 * parse path value of an audit event name="path"
 */
inline std::string AuditListener::parsePathField(auparse_state_t* au, const char* name)
{

    std::string buf(parseField(au, name));
    if(buf.empty())
        return buf;

    if(buf[0] == '\"')
    {
        buf = buf.substr(1, buf.size() -2);

        //auparse has a bug that's why it reads sometimes too far
        size_t found = buf.find_first_of("\"");
        if(found != buf.npos)
            buf.resize(found);
    }
    else
        // path is a hex string cause it may contains spaces
    {

        if(buf == "(null)")
            return std::string();

        try {
            buf = hexString2Ascii(buf);
        }
        catch(char c)
        {
            warn(_("Cannot convert hex string `%s\' to a valid path. Unrecognised character 0x%x"), buf.c_str(), c);
            buf.clear();
        }
    }
    return buf;
}

/*
 * Listen to the audit socket.
 * The Function hangs on until it received an event or user interrupt.
 */
void AuditListener::waitForEvent(struct audit_reply* reply)
{
    fd_set read_mask;
    struct timeval tv;
    int    retval;

repeat:
    do {
        interruptionPoint();

        tv.tv_sec = 60;
        tv.tv_usec = 0;
        FD_ZERO(&read_mask);
        FD_SET(audit_fd, &read_mask);

        retval = select(audit_fd+1, &read_mask, NULL, NULL, &tv);

        if(retval == 0)
            /*
             * Timeout received.
             * This occurs when another process captured the audit socket session.
             * Request status to find out the audit session owner.
             */
            audit_request_status(audit_fd);

    } while (retval == -1 && errno == EINTR);

    retval = audit_get_reply(audit_fd, reply,
                             GET_REPLY_NONBLOCKING, 0);
    if(0 > retval)
        goto repeat;
}

/*
 * Initialize the auparse state object.
 * Ignore invalid events
 *
 * Return NULL on error
 */
auparse_state_t* AuditListener::initAuParse(struct audit_reply* reply)
{
    auparse_state_t *au;
    std::string parse_str;

    // prepend a message type header in order to use the auparse library functions
    if(reply->type == AUDIT_PATH)
        parse_str = "type=PATH msg=";
    else if(reply->type == AUDIT_CWD)
        parse_str = "type=CWD msg=";
    else
        parse_str = "type=UNKNOWN msg=";

    parse_str += reply->msg.data;
    parse_str += "\n";

    au = auparse_init(AUSOURCE_BUFFER, parse_str.c_str());
    if(au == NULL)
        error(_("cannot init auparse"));

    if (-1 == auparse_next_event(au))
        error(_("auparse_next_event: %s"), strerror(errno));
    return au;
}

/*
 * Parse Field cwd="current working directory"
 */
void AuditListener::parseCwdEvent(auparse_state_t* au, boost::shared_ptr<AuditEvent> auditEvent)
{
    auditEvent->cwd = parsePathField(au, "cwd");
}

/*
 * Parse path="filename" field.
 * It is filename the syscall event refers to.
 */
void AuditListener::parsePathEvent(auparse_state_t* au, boost::shared_ptr<AuditEvent> auditEvent)
{
    struct stat st;

    if(!auditEvent->path.empty())
        return;

    auditEvent->path = realpath(parsePathField(au, "name"),
                                 auditEvent->cwd);

    auditEvent->ino  = atoll(parseField(au, "inode").c_str());

    std::string dev_buf = parseField(au, "dev");
    size_t found = dev_buf.find(":");
    if(found == std::string::npos)
        auditEvent->dev = 0;
    else
        auditEvent->dev = makedev(strtol(dev_buf.substr(0, found).c_str(), NULL, 16),
                                  strtol(dev_buf.substr(found+1).c_str(),NULL, 16));

    if(0 > stat(auditEvent->path.string().c_str(), &st)
       || !S_ISREG(st.st_mode))
    {
        auditEvent->path.clear();
        auditEvent->ino = auditEvent->dev = 0;
    }
    else if(st.st_ino != auditEvent->ino)
    {
        // path exists but inode does not fit to the event
        // this could also happen when a file got renamed
        // this probably results of using the glib function: g_file_set_content
        // or file access out of a chroot environment
        debug("syscall %d", auditEvent->type);
        debug("exe     %s", auditEvent->exe.string().c_str());
        debug("Inode Number differ! %s i_event: %u, d_event: %u - i_real: %u, d_real: %u",
              auditEvent->path.string().c_str(),
              auditEvent->ino, (__u32)auditEvent->dev,
              st.st_ino, (__u32)st.st_dev);
        auditEvent->type = Truncate;
    }
}

/*
 * Main entry point of parsing syscall audit event
 */
void AuditListener::parseSyscallEvent(auparse_state_t* au, boost::shared_ptr<AuditEvent> auditEvent)
{
    __u64 arch;
    int machine;
    int syscall;

    //notice: you have to read audit message fields in the right order

    arch = strtoll(parseField(au, "arch").c_str(), NULL, 16);
    syscall = strtol(parseField(au, "syscall").c_str(), NULL, 10);

    machine = audit_elf_to_machine(arch);
    if(-1 == machine)
    {
        error(_("audit_elf_to_machine failed: arch=%x: %s"), arch, strerror(errno));
        auditEvent->type = Unknown;
        return;
    }

    const char* sc_name = audit_syscall_to_name(syscall, machine);

    if(NULL == sc_name)
    {
        error(_("audit_syscall_to_name failed: machine=%d arch=%x"), machine, arch);
        auditEvent->type = Unknown;
        return;
    }

    if(0 == strcmp(sc_name, "open"))
        auditEvent->type = Open;
    else if(0 == strcmp(sc_name, "clone"))
        auditEvent->type = Fork;
    else if(0 == strcmp(sc_name, "execve"))
        auditEvent->type = Execve;
    else if(0 == strcmp(sc_name, "openat"))
        auditEvent->type = Open;
    else if(0 == strcmp(sc_name, "truncate"))
        auditEvent->type = Truncate;
    else if(0 == strcmp(sc_name, "creat"))
        auditEvent->type = Creat;
    else if(0 == strcmp(sc_name, "mknod"))
        auditEvent->type = Creat;
    else if(0 == strcmp(sc_name, "fork"))
        auditEvent->type = Fork;
    else if(0 == strcmp(sc_name, "vfork"))
        auditEvent->type = Fork;
    else if(0 == strcmp(sc_name, "truncate64"))
        auditEvent->type = Truncate;
    else
    {
        debug(_("Unknown syscall: %s = %d"), sc_name, syscall);
        auditEvent->type = Unknown;
        return;
    }

    if("yes" == parseField(au, "success"))
        auditEvent->successful = true;

    if(auditEvent->type == Fork)
        auditEvent->exit = strtoll(parseField(au, "exit").c_str(), NULL, 10);

    if(auditEvent->type == Open || auditEvent->type == OpenAt)
    {
        int flags = strtol(parseField(au, "a1").c_str(), NULL, 16);

        if(!(  flags & O_WRONLY
            || flags & O_RDWR
            || flags & O_CREAT))
            auditEvent->readOnly = true;
    }

    auditEvent->ppid = strtoll(parseField(au, "ppid").c_str(), NULL, 10);
    auditEvent->pid  = strtoll(parseField(au, "pid" ).c_str(), NULL, 10);
    auditEvent->comm = parsePathField(au, "comm");
    auditEvent->exe  = parsePathField(au, "exe");
}

/*
 * test whether path does match regex
 * return true either on exactly match
 *                 or regex represents a subdirectory of path
 *        otherwise false
 */
bool doesRegexMatchPath(fs::path& p, boost::regex& filter)
{
    boost::match_results<std::string::const_iterator> what;
    if( boost::regex_search(p.string(), what, filter,
                            boost::match_default | boost::match_continuous  ))
    {
        //partial match beginning at first character found
        size_t match_end =  distance(what[0].first, what[0].second);

        //test for exact match
        if(match_end >= p.string().size())
            return true;
        //test whether partial path match points to a directory
        if('/' == p.string().at(match_end))
            return true;

    }
    return false;
}

/*
 * Test whether file is excluded by an path regex
 * Return true path is ignored
 *        otherwise false
 */
bool AuditListener::ignorePath(fs::path& p)
{
    if(!watch_paths.empty())
    {
        BOOST_FOREACH(boost::regex filter, watch_paths)
            if(doesRegexMatchPath(p,filter))
                goto path_valid;

        return true;
    }
path_valid:
    BOOST_FOREACH(boost::regex filter, exclude_paths)
        if(doesRegexMatchPath(p, filter))
            return true;

    return false;
}


/*
 * Test whether file is excluded by its device id
 */
bool AuditListener::ignoreDevice(dev_t dev)
{
    if(exclude_devices.end() != exclude_devices.find(dev))
        return true;

    if(!watch_devices.empty())
        if(watch_devices.end() != watch_devices.find(dev))
            return false;

    if(ext4_only)
    {
        try {
            Device device(dev);
            if(device.getFileSystem() == "ext4")
                watch_devices.insert(dev);
            else
            {
                std::string dev_name = device.getDevicePath();
                if(dev_name.at(0) != '/') //it's virtual fs: display mount point instead of cunfusing device name.
                    dev_name = device.getMountPoint().string();
                info(_("%s is not an ext4 filesystem."), dev_name.c_str());
                info(_("Filesystem of %s is %s"), dev_name.c_str(), device.getFileSystem().c_str());
                exclude_devices.insert(dev);
                return true;
            }
        }
        catch(std::exception& e)
        {
            // Damn! Keep going. We can drop this file later.
            info("%s", e.what());
            return false;
        }
    }
    return false;
}

/*
 * Check if filesystem type is ignored
 */
bool AuditListener::checkFileSystemType(fs::path& p)
{
    struct statfs fs;
    if(0 > statfs(p.string().c_str(), &fs))
        return false;

    if(!watch_fs_types.empty())
    {
        if(watch_fs_types.end() == watch_fs_types.find(fs.f_type))
            return false;
    }
    return true;
}

/*
 * Infinite loop of listening to the Linux audit system
 */
void AuditListener::exec()
{
    struct audit_reply reply;
    auparse_state_t *au;
    typedef std::map<__u32, boost::shared_ptr<AuditEvent> > msgdb_t;
    msgdb_t msgdb;
    msgdb_t::iterator msgdb_it;
    boost::shared_ptr<AuditEvent> auditEvent;
    __u32 msgid;

    while(1)
    {
        waitForEvent(&reply);

        reply.msg.data[reply.len] = '\0';
        au = initAuParse(&reply);

        debug("%d: %*s", reply.type, reply.len, reply.msg.data);

        msgid = auparse_get_serial(au);
        msgdb_it = msgdb.find(msgid);
        if(msgdb_it != msgdb.end())
            auditEvent = msgdb_it->second;
        else
        {
            auditEvent =  boost::shared_ptr<AuditEvent>(new AuditEvent);
            msgdb.insert(std::pair<__u32, boost::shared_ptr<AuditEvent> >(msgid, auditEvent));
        }

        switch(reply.type)
        {
            // event is syscall event
            case AUDIT_SYSCALL:
                parseSyscallEvent(au,auditEvent);
                break;

            // change working directory
            case AUDIT_CWD:
                if(auditEvent->type == Unknown
                   || !auditEvent->successful)
                    break;

                parseCwdEvent(au, auditEvent);
                break;

            // event refers to file
            case AUDIT_PATH:
                if(auditEvent->type == Unknown
                   || !auditEvent->successful)
                    break;

                parsePathEvent(au,auditEvent);
                break;

            // end of multi record event
            case AUDIT_EOE:
                if(auditEvent->type != Unknown
                   && auditEvent->successful
                   && ( auditEvent->type == Fork
                        || ( !auditEvent->path.empty()
                             && !ignorePath(auditEvent->path)
                             && !ignoreDevice(auditEvent->dev)
                             && checkFileSystemType(auditEvent->path)
                            )
                      ))
                {
                    debug(_("Parsed Event: %d %s"), auditEvent->type, auditEvent->path.string().c_str());
                    eventParsed(auditEvent);
                }

                msgdb.erase(msgdb_it);
                break;
            case AUDIT_CONFIG_CHANGE:
                auparse_first_field(au);
                if(0 == auparse_next_field(au))
                    break;

                if(0 == strcmp("audit_pid", auparse_get_field_name(au)))
                {
                    /*
                     * There is no guarantee that we get the message that someone else has
                     * captured the audit socket session. Therefore periodically the status
                     * of the netlink socket is checked as well.
                     */
                    pid_t audit_pid = strtol(auparse_get_field_str(au), NULL, 10);
                    checkSocketCaptured(audit_pid);
                }
                else
                {
                    while(auparse_next_field(au))
                    {
                        if(0 == strcmp("op", auparse_get_field_name(au)))
                        {
                            // The message does not contain what rules has been changed
                            // Test weather op field is equal to "remove rule"
                            // auparse cannot parse fields containing spaces
                            if(parseField(au, "op") == "\"remove")
                            {
                                warn(_("Audit configuration has changed. Reinserting audit rules."));
                                insertAuditRules();
                            }
                            break;
                        }
                    }
                }
                break;
            // get netlink status
            case AUDIT_GET:
                checkSocketCaptured(reply.status->pid);
                break;
            default:
                break;
        }
        auparse_reset(au);
    }
    auparse_destroy(au);
}


Listener::~Listener()
{}

void Listener::stop()
{
    Interruptible::interrupt();
}

void Listener::connect()
{
    try {
        insertAuditRules();
	activateAuditSocket();
    }
    catch(std::exception&e)
    {
        error("%s", e.what());
        interrupt();
    }
}

bool Listener::start()
{
    try{
        exec();
    }
    catch(UserInterrupt&)
    {}
    catch(DetectAuditDaemon& e)
    {
        return false;
    }
    removeAuditRules();
    closeAuditSocket();

    return true;
}
