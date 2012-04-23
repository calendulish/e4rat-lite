/* e4rat-preload-lite, written by John Lindgren, 2011. */
/* Replacement for e4rat-preload, which was written by Andreas Rid, 2011. */
/* non-standard strdup function from Inkane (http://goo.gl/byuDt)  */
/* Git maintainer: Lara Maia <lara@craft.net.br> - (http://goo.gl/htS99) */


#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LIST "/var/lib/e4rat/startup.log"
#define INIT "/sbin/init"
#define EARLY 200
#define BLOCK 300
#define BUF (1024*1024)

char *strdup(const char *str)
{
int n = strlen(str) + 1;
char *dup = malloc(n);
if(dup)
{
strcpy(dup, str);
}
return dup;
}

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

static void load_list (void) {
   printf ("Loading %s.\n", LIST);
   FILE * stream = fopen (LIST, "r");
   if (! stream) {
      printf ("Error: %s.\n", strerror (errno));
      exit (EXIT_FAILURE);
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

static void exec_init (char * * argv) {
   printf ("Executing %s.\n", INIT);
   switch (fork ()) {
   case -1:
      printf ("Error: %s.\n", strerror (errno));
      exit (EXIT_FAILURE);
   case 0:
      return;
   default:
      execv (INIT, argv);
      printf ("Error: %s.\n", strerror (errno));
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

int main (int argc, char * * argv) {
   load_list ();
   printf ("Preloading %d files.\n", listlen);
   load_inodes (0, EARLY);
   load_files (0, EARLY);
   exec_init (argv);
   for (int i = EARLY; i < listlen; i += BLOCK) {
      load_inodes (i, i + BLOCK);
      load_files (i, i + BLOCK);
   }
   exit (EXIT_SUCCESS);
}
