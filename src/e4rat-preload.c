/* e4rat-lite: (2012) (http://goo.gl/IXP3n)
 * written by Lara Maia <lara@craft.net.br> 
 * 
 * e4rat-preload-lite: (2011)
 * written by John Lindgren
 *  
 * e4rat-preload: (2011)
 * written by Andreas Rid.
 * 
 */


#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libintl.h>

#include "config.h"

#define _(x) gettext(x)

#define EARLY 200
#define BLOCK 300
#define BUF (1024*1024)

#ifdef __STRICT_ANSI__
char *strdup(const char *str) {
	unsigned int n = strlen(str) + 1;
	char *dup = malloc(n);
	if(dup) strcpy(dup, str);
	return dup;
}
#endif

typedef struct {
   int n, dev;
   uint64_t inode;
   char * path;
} FileDesc;

static FileDesc * * list = 0;
static FileDesc * * sorted = 0;
static int listlen = 0;

static int sort_cb (const void * _a, const void * _b) {
   FileDesc * a = * (FileDesc * *) _a;
   FileDesc * b = * (FileDesc * *) _b;
   if (a->dev < b->dev)
      return -1;
   if (a->dev > b->dev)
      return 1;
   if (a->inode < b->inode)
      return -1;
   if (a->inode > b->inode)
      return 1;
   return 0;
}

static FileDesc * parse_line (int n, const char * line) {
   int dev = 0;
   while (* line >= '0' && * line <= '9')
      dev = dev * 10 + ((* line ++) - '0');
   if ((* line ++) != ' ')
      return 0;
   uint64_t inode = 0;
   while (* line >= '0' && * line <= '9')
      inode = inode * 10 + ((* line ++) - '0');
   if ((* line ++) != ' ')
      return 0;
   FileDesc * f = malloc (sizeof (FileDesc));
   f->n = n;
   f->dev = dev;
   f->inode = inode;
   f->path = strdup (line);
   return f;
}

static void printUsage () {
	printf("Usage: e4rat-lite-preload [ option(s) ]\n"
	"\n"
	"-V --version                           print version and exit\n"
	"-h --help                              print help and exit\n"
	"\n"
	"-i --initfile <path to file>           alternate init file\n"
	"-s --startuplog <path to file>         alternate startup log file"
	"\n");
}

static void load_list (const char* LIST) {
   printf (_("Loading %s.\n"), LIST);
   FILE * stream = fopen (LIST, "r");
   if (!stream) {
      printf (_("Error: %s.\n"), strerror(errno));
      exit(EXIT_FAILURE);
   }

   int listsize = 0;
   while (1) {
      char buf[512];
      if (! fgets (buf, sizeof buf, stream))
         break;
      if (buf[0] && buf[strlen (buf) - 1] == '\n')
         buf[strlen (buf) - 1] = 0;
      FileDesc * f = parse_line (listlen, buf);
      if (! f)
         continue;
      if (listlen >= listsize) {
         listsize = listsize ? listsize * 2 : 256;
         list = realloc (list, sizeof (FileDesc *) * listsize);
      }
      list[listlen ++] = f;
   }
   fclose (stream);
   list = realloc (list, sizeof (FileDesc *) * listlen);
   sorted = malloc (sizeof (FileDesc *) * listlen);
   memcpy (sorted, list, sizeof (FileDesc *) * listlen);
   qsort (sorted, listlen, sizeof (FileDesc *), sort_cb);
}

static void load_inodes (int a, int b) {
   struct stat s;
   for (int i = 0; i < listlen; i ++) {
      if (sorted[i]->n >= a && sorted[i]->n < b)
         stat (sorted[i]->path, & s);
   }
}

static void exec_init (char * * argv, const char* INIT) {
   printf (_("Running %s.\n"), INIT);
   switch (fork ()) {
   case -1:
      printf (_("Error: %s.\n"), strerror (errno));
      exit (EXIT_FAILURE);
   case 0:
      return;
   default:
      execv (INIT, argv);
      printf (_("Error: %s.\n"), strerror (errno));
      exit (EXIT_FAILURE);
   }
}

static void load_files (int a, int b) {
   void * buf = malloc (BUF);
   for (int i = a; i < b && i < listlen; i ++) {
      int handle = open (list[i]->path, O_RDONLY);
      if (handle < 0)
         continue;
      while (read (handle, buf, BUF) > 0)
         ;
      close (handle);
   }
   free (buf);
}

typedef struct
{
    const char* init_file;
    const char* startup_log_file;
} configuration;

static int config_handler(void* user, const char* section, const char* name,
                   const char* value)
{
    configuration* pconfig = (configuration*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
      if (MATCH("Global", "startup_log_file")) {
        pconfig->startup_log_file = strdup(value);
    } else if (MATCH("Global", "init_file")) {
        pconfig->init_file = strdup(value);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

int main (int argc, char * * argv) {
   configuration config;
   setlocale(LC_ALL, "");
   bindtextdomain("e4rat-lite", "/usr/share/locale");
   textdomain("e4rat-lite");
   
   if (ini_parse("/etc/e4rat-lite.conf", config_handler, &config) < 0) {
	   printf(_("Unable to load the configuration file:: %s\n"), strerror (errno));
	   exit (EXIT_FAILURE);
   }
   
   static struct option long_options[] =
	{
		{"help",        no_argument,       0, 'h'},
		{"version",     no_argument,       0, 'V'},
		{"initfile",    required_argument, 0, 'i'},
		{"startuplog",  required_argument, 0, 's'},
		{0, 0, 0, 0}
	};

    int c;
    int option_index = 0;
    const char* opt_init_file = 0;
    const char* opt_startup_log_file = 0;
    while ((c = getopt_long (argc, argv, "i:s:hV", long_options, &option_index)) != EOF)
	{
        switch(c)
        {
            case 'h':
                goto err1;
            case 'V':
				goto err2;
			case 'i':
				opt_init_file = optarg;
				break;
			case 's':
				opt_startup_log_file = optarg;
				break;
        }
    }

   if(opt_startup_log_file != 0) load_list (opt_startup_log_file);
   else load_list (config.startup_log_file);
   printf (_("Preloading %d files...\n"), listlen);
   load_inodes (0, EARLY);
   load_files (0, EARLY);
   if(opt_init_file != 0) exec_init (argv, opt_init_file);
   else exec_init (argv, config.init_file);
   for (int i = EARLY; i < listlen; i += BLOCK) {
      load_inodes (i, i + BLOCK);
      load_files (i, i + BLOCK);
   }
   exit (EXIT_SUCCESS);

err1:
    printUsage();
    exit(1);
err2:
	printf("%s %s\n", PROGRAM_NAME, VERSION);
	exit(1);
}
