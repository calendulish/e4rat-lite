/*
 * e4rat-collect.cc - Generate file list of relevant files by monitoring programs
 *
 * Copyright (C) 2011 by Andreas Rid
 * Copyright (C) 2012 by Lara Maia <lara@craft.net.br>
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
extern "C" {
    #include "config.h"
}
#include "eventcatcher.hh"
#include "logging.hh"
#include "parsefilelist.hh"

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <libintl.h>
#include <fstream>
#include <signal.h>

/* EXT2_SUPER_MAGIC */
#include <ext2fs/ext2_fs.h>

/* exec program */
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <pwd.h>

#include <boost/foreach.hpp>

#define _(x) gettext(x)

#define PID_FILE "/dev/.e4rat-lite-collect.pid"

#ifdef __STRICT_ANSI__
char *strdup(const char *str) {
    unsigned int n = strlen(str) + 1;
    char *dup = malloc(n);
    if(dup) strcpy(dup, str);
    return dup;
}
#endif

typedef struct
{
    const char* startup_log_file;
    const char* init_file;
    bool exclude_open_files;
    bool ext4_only;
    unsigned int timeout;
} configuration;

static int config_handler(void* user, const char* section, const char* name,
                   const char* value)
{
    configuration* pconfig = (configuration*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
      if (MATCH("Global", "startup_log_file")) {
        pconfig->startup_log_file = strdup(value);
    } else if (MATCH("Collect", "exclude_open_files")) {
        pconfig->exclude_open_files = strdup(value);
    } else if (MATCH("Collect", "ext4_only")) {
        pconfig->ext4_only = strdup(value);
    } else if (MATCH("Collect", "timeout")) {
        pconfig->timeout = atoi(value);
    } else if (MATCH("Global", "init_file")) {
        pconfig->init_file = strdup(value);
    } else {
        return 0; /* unknown section/name, error */
    }
    return 1;
}

bool isAuditDaemonRunning()
{
    pid_t pid = readPidFile("/var/run/auditd.pid");
    if(pid)
        if(0 == kill(pid, 0))
            return true;
    return false;
}

/*
 * Execute a command as user
 */
int system_u(const char* user, const char* command)
{
    int retval = -1;
    struct passwd* pw;

    pid_t pid = fork();
    switch(pid)
    {
        case -1:
            error(_("Fork failed: %s"), strerror(errno)); break;
        case 0: //child
            if(user)
            {
                //reset errno to indicate error
                errno = 0;
                pw = getpwnam(user);
                if(pw == NULL)
                {
                    if(errno)
                        error(_("Cannot receive user id: %s"), strerror(errno));
                    else
                        error(_("Unknown username %s"), user);
                    exit(1);
                }
                setenv("HOME", pw->pw_dir, 1);
                setuid(pw->pw_uid);
                setgid(pw->pw_gid);
            }

            execl("/bin/sh", "/bin/sh", "-c", command, NULL);
        default:
            waitpid(pid, &retval, 0);
    }
    return retval;
}

void scanOpenFiles(std::vector<FilePtr>& list)
{
    char buffer[PATH_MAX + 64];
    char path[PATH_MAX];
    ino_t ino;
    int major;
    int minor;

    size_t size_early = list.size();
    debug(_("Scan open files by calling lsof"));

    FILE* pFile = popen("lsof -w /", "r");
    if(NULL == pFile)
        return;
    //skip first line
    if(NULL == fgets (buffer , PATH_MAX+64, pFile))
        return;

    while(NULL != fgets (buffer , PATH_MAX+64, pFile))
    {
        if(EOF == sscanf(buffer, "%*s%*s%*s%*s%*s %x%*c%x %*s %llu %[^\n]s", &major, &minor, (long long unsigned int*)&ino, path))
        {
            error(_("scan lsof: %s"), strerror(errno));
        }
        FilePtr file = FilePtr(makedev(major, minor), ino, path);
        if(file.unique())
            list.push_back(file);
    }
    info(_("%*d open files"), 8, list.size() - size_early);
    pclose(pFile);
}


void excludeFileLists(std::vector<const char*>& files, std::vector<FilePtr>& list)
{
    BOOST_FOREACH(const char* fname, files)
    {
        std::vector<std::string> ex_files = matchPath(fname);
        BOOST_FOREACH(std::string filename, ex_files)
        {
            FILE* file = fopen(filename.c_str(), "r");
            if(file)
            {
                size_t size_early = list.size();
                parseInputStream(file, list);
                info(_("%*d parsed from %s"), 8, list.size() - size_early, filename.c_str());
                fclose(file);
            }
            else
                std::cerr << _("Cannot open file list: ")
                          << filename << ": " << strerror(errno) << std::endl;
        }
    }
}


void printUsage()
{
    std::cout <<
_("Usage: e4rat-lite-collect [ option(s) ] [ application name(s) ]\n"
"\n"
"    -V --version                    print version and exit\n"
"    -h --help                       print help and exit\n"
"    -v --verbose                    increment verbosity level\n"
"    -q --quiet                      set verbose level to 0\n"
"    -l --loglevel <number>          set log level\n"
"\n"
"    -k --stop                       kill running collector\n"
"    -x --execute <command>          quit after command has finished\n"
"    -u --user <username>            execute command as user\n"
"    -o --output [file]              dump generated file list to file\n"
"    -d --device <dev>               watch a specific device\n"
"                                    [example: /dev/sda1]\n"
"    -D --exclude-device <dev>       exclude device\n"
"    -p --path <path>                restrict watch on path [example: '*/bin/*']\n"
"    -P --exclude-path <path>        exclude filesystem path\n"
"    -L --exclude-list <file>        exclude paths listed in file\n\n")
        ;
}

int main(int argc, char* argv[])
{
    bool create_pid_late = false;

    configuration config;
    setlocale(LC_ALL, "");
    bindtextdomain("e4rat-lite", "/usr/share/locale");
    textdomain("e4rat-lite");

    if (ini_parse("/etc/e4rat-lite.conf", config_handler, &config) < 0) {
       printf(_("Unable to load the configuration file: %s\n"), strerror (errno));
       exit (EXIT_FAILURE);
    }

    int loglevel = 3; //FIXME
    int verbose  = 7; //FIXME

    const char* execute  = NULL;
    const char* username = NULL;
    const char* outPath  = NULL;
    FILE* outStream      = NULL;

    std::deque<FilePtr> filelist;
    std::vector<const char*> exclude_filenames ;

    std::vector<FilePtr> excludeList ;

    ScanFsAccess project;
    Listener listener;

    // excluding file list only affect only if process id is not 1
    if(0 == access(config.startup_log_file, F_OK))
        exclude_filenames.push_back(config.startup_log_file);

    static struct option long_options[] =
        {
            {"verbose",        no_argument,       0, 'v'},
            {"version",        no_argument,       0, 'V'},
            {"quiet",          no_argument,       0, 'q'},
            {"loglevel",       required_argument, 0, 'l'},
            {"help",           no_argument,       0, 'h'},
            {"exclude-device", required_argument, 0, 'D'},
            {"device",         required_argument, 0, 'd'},
            {"exclude-path",   required_argument, 0, 'P'},
            {"path",           required_argument, 0, 'p'},
            {"exclude-list",   optional_argument, 0, 'L'},
            {"execute",        required_argument, 0, 'x'},
            {"user",           required_argument, 0, 'u'},
            {"output",         required_argument, 0, 'o'},
            {"stop",           no_argument,       0, 'k'},
            {0, 0, 0, 0}
        };

    int c;
    int option_index = 0;
    opterr = 0;
    while ((c = getopt_long (argc, argv, "hVvql:o:D:d:P:p:L:x:ku:", long_options, &option_index)) != EOF)
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
                goto err1;
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
            case 'L':
                exclude_filenames.push_back(optarg);
                break;
            case 'o':
                outPath = optarg;
                break;
            case 'D':
                listener.excludeDevice(optarg);
                break;
            case 'd':
                listener.watchDevice(optarg);
                break;
            case 'P':
                listener.excludePath(optarg);
                break;
            case 'p':
                listener.watchPath(optarg);
                break;
            case 'x':
                execute = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'k':
            {
                pid_t pid = readPidFile(PID_FILE);
                if(0 == pid)
                {
                    error(_("Cannot read pid from file %s: %s"), PID_FILE, strerror(errno));
                    return 1;
                }
                kill(pid, SIGINT);
                return 0;
            }
            case '?':
                if (optopt == 'o') // optional parameter for output is missing
                    outStream = stdout;
                else if(optopt == 'L')
                    exclude_filenames.clear();
                else
                {
                    for(int i=0; long_options[i].val; i++)
                        if(long_options[i].val == optopt)
                        {
                            fprintf(stderr, _("Option requires an argument -- '%c'\n"), optopt);
                            exit(1);
                        }
                    fprintf(stderr, _("Unrecognised option -- '%c'\n"), optopt);

                    return -1;
                }
                break;
        }
    }

    logger.setVerboseLevel(verbose);
    logger.setLogLevel(loglevel);

    if(getuid() != 0)
    {
        std::cerr << _("You need root privileges to run this program.\n");
        return 1;
    }
    if(isAuditDaemonRunning())
    {
        std::cerr << _("In order to use this program you first have to stop the audit daemon auditd.\n");
        return 1;
    }
    /*
     * Register Signalhandler
     */
    struct sigaction sa;
    memset(&sa, '\0', sizeof(struct sigaction));
    sa.sa_handler = signalHandler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if(1 == getpid())
    {
        create_pid_late = true;

        outPath = config.startup_log_file;
        verbose = 0;
    }
    else
    {
        if(true == config.exclude_open_files || exclude_filenames.size())
        {
            info(_("Generating exclude file list ..."));
            try {
                if(true == config.exclude_open_files)
                    scanOpenFiles(excludeList);
                excludeFileLists(exclude_filenames, excludeList);
            }
            catch(std::exception& e)
            {
                std::cout << e.what() << std::endl;
                goto err2;
            }
            info(_("Total number of excluded files: %d"), excludeList.size());
        }

        if(!createPidFile(PID_FILE))
        {
            std::cerr << _("It seems that e4rat-lite-collect is already running.\n");
            std::cerr << _("Remove pid file ") << PID_FILE << _(" to unlock.\n");
            exit(1);
        }

        if(outStream == stdout)
            logger.redirectStdout2Stderr(true);
        else if(!outPath)
            outPath = "./e4rat-lite-collect.log";

    /*
         * Parse application list given as arguments
         */
        for ( ; optind < argc; optind++)
            project.observeApp(fs::path(argv[optind]).filename().string());

        /*
         * Parse application list on stdin
         */
        char app[PATH_MAX];
        setStdIn2NonBlocking();

        try {
            while(!std::cin.eof())
            {
                std::cin >> app;
                project.observeApp(app);
            }
        }
        catch(...)
        {}
    }

    if(config.ext4_only)
        listener.watchExt4Only();

    CONNECT(&listener, eventParsed, boost::bind(&EventCatcher::handleAuditEvent, &project, _1));


    if(execute || 1 == getpid())
    {
        sem_t* sem = (sem_t*) mmap(NULL, sizeof (sem_t),
                                   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (sem_init(sem, 1, 0) == -1)
        {
            error(_("sem_init: %s"), strerror(errno));
            goto err2;
        }

        switch(fork())
        {
            case -1:
                error(_("Fork failed: %s"), strerror(errno));
                break;
            case 0: //child process
                if(0 != prctl(PR_SET_PDEATHSIG, SIGINT))
                    error(_("Set parent death signal: %s"), strerror(errno));
                info(_("Connecting to the audit socket ..."));
                listener.connect();
                if(0 != sem_post(sem))
                    error(_("sem_post: %s"), strerror(errno));
                break;
            default:
                if(0 != sem_wait(sem))
                    error(_("sem_wait: %s"), strerror(errno));
                sem_destroy(sem);
                munmap(sem, sizeof(sem_t));
                if(execute)
                {
                    notice(_("Execute `%s' ..."), execute);
                    system_u(username, execute);
                }
                else
                {
                    notice(_("Execute `%s' ..."), config.init_file);
                    execv(config.init_file, argv);
                }
                sleep(1);
                exit(0);
         }
    }
    else
        listener.connect();

    if(create_pid_late)
    {
        bool pc = createPidFile(PID_FILE);
        unsigned int timeout = config.timeout;
        if(timeout)
        {
            sigaction(SIGALRM, &sa, NULL);
            alarm(config.timeout);
            notice(_("Stop collecting files automatically after %d seconds"), timeout);
        }
        else
        {
            if(pc == false)
                notice(_("Signal collector to stop by calling `killall %s'"));
            else
                notice(_("Signal collector to stop by calling `collect -k'"));
        }
    }
    else
        notice(_("Press 'Ctrl-C' to stop collecting files"));

    info(_("Starting event processing ..."));
    if(false == listener.start())
        goto err2;

    filelist = project.getFileList();

    notice(_("\t%d file(s) collected"), filelist.size());

    if(filelist.empty())
        goto out;
    /*
     * dump file list
     */
    if(outPath && !outStream)
    {
        outStream = fopen(outPath, "w");
        if(NULL == outStream)
        {
            error(_("Cannot open output file: %s: %s"), outPath, strerror(errno));
            goto err2;
        }
    }

    if(outStream != stdout)
        notice(_("Save file list to %s"), outPath);

    BOOST_FOREACH(FilePtr f, filelist)
        fprintf(outStream, "%u %u %s\n", (__u32)f.getDevice(), (__u32)f.getInode(), f.getPath().string().c_str());
    fclose(outStream);
out:
    unlink(PID_FILE);
    exit(0);
err1:
    printUsage();
    exit(1);
err2:
    if(getpid() == 1)
         execv(config.init_file, argv);
    unlink(PID_FILE);
    exit(1);
}
