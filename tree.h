/* $Copyright: $
 * Copyright (c) 1996 - 2023 by Steve Baker (ice@mama.indstate.edu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif /* !_WIN32 */
#ifdef __EMX__  /* for OS/2 systems */
#  define INCL_DOSFILEMGR
#  define INCL_DOSNLS
#  include <os2.h>
#  include <sys/nls.h>
#  include <io.h>
  /* On many systems stat() function is identical to lstat() function.
   * But the OS/2 does not support symbolic links and doesn't have lstat() function.
   */
#  define         lstat          stat
#  define         strcasecmp     stricmp
  /* Following two functions, getcwd() and chdir() don't support for drive letters.
   * To implement support them, use _getcwd2() and _chdir2().
   */
#  define getcwd _getcwd2
#  define chdir _chdir2
#endif

#ifdef _WIN32
#include <errno.h>
typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned int    u_int;
typedef unsigned long   u_long;
#  define strcasecmp    _stricmp
#  define S_IFLNK       0xA000    /* symbolic link */
#  define S_IFSOCK      0xC000    /* socket */
#  define S_ISUID       0004000   /* set user id on execution */
#  define S_ISGID       0002000   /* set group id on execution */
#  define S_ISVTX       0001000   /* save swapped text even after use */
#  ifdef _LARGEFILE_SOURCE
#    define __USE_LARGEFILE     1         /* declare fseeko and ftello */
#  endif
#  ifdef _LARGEFILE64_SOURCE
#    define __USE_LARGEFILE64   1    /* declare 64-bit functions */
#  endif
#  if defined _FILE_OFFSET_BITS && _FILE_OFFSET_BITS == 64
#    define __USE_FILE_OFFSET64 1    /* replace 32-bit functions by 64-bit ones */
#  endif
#  if (__USE_LARGEFILE || __USE_LARGEFILE64) && __USE_FILE_OFFSET64
/* replace stat and seek by their large-file equivalents */
#    define stat   _stati64
#    define lstat  _stati64
#    define off_t  __int64
#endif /* LARGE_FILES */
typedef unsigned int uid_t;
typedef unsigned int gid_t;
struct group
{
  char *gr_name;
  char *gr_passwd;
  gid_t gr_gid;
  char **gr_mem;
};
struct passwd
{
  char *pw_name;
  char *pw_passwd;
  uid_t pw_uid;
  gid_t pw_gid;
  char *pw_gecos;
  char *pw_dir;
  char *pw_shell;
};
# define getpwuid(i) NULL
# define getgrgid(i) NULL
static int readlink (const char *path, char *buf, size_t len)
{
  errno = ENOSYS;
  return -1;
}
static char* realpath(const char *filename, char *resolved_name)
{
  if (resolved_name == NULL) resolved_name = malloc(PATH_MAX);
  if (resolved_name == NULL) return NULL;
  if (access(filename, F_OK) != 0) return NULL;
  if (GetFullPathNameA(filename, PATH_MAX, resolved_name, NULL) == 0) return NULL;
  return resolved_name;
}
#define nl_langinfo(...) "utf8"
#endif /* _WIN32 */

#include <locale.h>
#ifndef _WIN32
#include <langinfo.h>
#endif
#include <wchar.h>
#include <wctype.h>

#ifdef __ANDROID
#define mbstowcs(w,m,x) mbsrtowcs(w,(const char**)(& #m),x,NULL)
#endif

/* Start using PATH_MAX instead of the magic number 4096 everywhere. */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef INFO_PATH
#define INFO_PATH "/usr/share/finfo/global_info"
#endif

#ifdef __linux__
#include <fcntl.h>
# define ENV_STDDATA_FD  "STDDATA_FD"
# ifndef STDDATA_FILENO
#  define STDDATA_FILENO 3
# endif
#endif

/* Should probably use strdup(), but we like our xmalloc() */
#define scopy(x)	strcpy(xmalloc(strlen(x)+1),(x))
#define MINIT		30	/* number of dir entries to initially allocate */
#define MINC		20	/* allocation increment */

#ifndef TRUE
typedef enum {FALSE=0, TRUE} bool;
#else
typedef int bool;
#endif

struct _info {
  char *name;
  char *lnk;
  bool isdir;
  bool issok;
  bool isfifo;
  bool isexe;
  bool orphan;
  mode_t mode, lnkmode;
  uid_t uid;
  gid_t gid;
  off_t size;
  time_t atime, ctime, mtime;
  dev_t dev, ldev;
  ino_t inode, linode;
  #ifdef __EMX__
  long attr;
  #endif
  char *err;
  const char *tag;
  char **comment;
  struct _info **child, *next, *tchild;
};

/* list.c */
struct totals {
  u_long files, dirs;
  off_t size;
};

struct listingcalls {
  void (*intro)(void);
  void (*outtro)(void);
  int (*printinfo)(char *dirname, struct _info *file, int level);
  int (*printfile)(char *dirname, char *filename, struct _info *file, int descend);
  int (*error)(char *error);
  void (*newline)(struct _info *file, int level, int postdir, int needcomma);
  void (*close)(struct _info *file, int level, int needcomma);
  void (*report)(struct totals tot);
};


/* hash.c */
struct xtable {
  unsigned int xid;
  char *name;
  struct xtable *nxt;
};
struct inotable {
  ino_t inode;
  dev_t device;
  struct inotable *nxt;
};

/* color.c */
struct colortable {
  char *term_flg, *CSS_name, *font_fg, *font_bg;
};
struct extensions {
  char *ext;
  char *term_flg, *CSS_name, *web_fg, *web_bg, *web_extattr;
  struct extensions *nxt;
};
struct linedraw {
  const char **name, *vert, *vert_left, *corner, *copy;
  const char *ctop, *cbot, *cmid, *cext, *csingle;
};
struct meta_ids {
  char *name;
  char *term_flg;
};

/* filter.c */
struct pattern {
  char *pattern;
  int relative;
  struct pattern *next;
};

struct ignorefile {
  char *path;
  struct pattern *remove, *reverse;
  struct ignorefile *next;
};

/* info.c */
struct comment {
  struct pattern *pattern;
  char **desc;
  struct comment *next;
};

struct infofile {
  char *path;
  struct comment *comments;
  struct infofile *next;
};


/* Function prototypes: */
/* tree.c */
void setoutput(char *filename);
void print_version(int nl);
void usage(int);
void push_files(char *dir, struct ignorefile **ig, struct infofile **inf, bool top);
int patignore(char *name, int isdir);
int patinclude(char *name, int isdir);
struct _info **unix_getfulltree(char *d, u_long lev, dev_t dev, off_t *size, char **err);
struct _info **read_dir(char *dir, int *n, int infotop);

int filesfirst(struct _info **, struct _info **);
int dirsfirst(struct _info **, struct _info **);
int alnumsort(struct _info **, struct _info **);
int versort(struct _info **a, struct _info **b);
int reversealnumsort(struct _info **, struct _info **);
int mtimesort(struct _info **, struct _info **);
int ctimesort(struct _info **, struct _info **);
int sizecmp(off_t a, off_t b);
int fsizesort(struct _info **a, struct _info **b);

void *xmalloc(size_t), *xrealloc(void *, size_t);
char *gnu_getcwd(void);
int patmatch(char *, char *, int);
void indent(int maxlevel);
void free_dir(struct _info **);
#ifdef __EMX__
char *prot(long);
#else
char *prot(mode_t);
#endif
char *do_date(time_t);
void printit(char *);
int psize(char *buf, off_t size);
char Ftype(mode_t mode);
struct _info *stat2info(struct stat *st);
char *fillinfo(char *buf, struct _info *ent);

/* list.c */
void null_intro(void);
void null_outtro(void);
void null_close(struct _info *file, int level, int needcomma);
void emit_tree(char **dirname, bool needfulltree);
struct totals listdir(char *dirname, struct _info **dir, int lev, dev_t dev, bool hasfulltree);

/* unix.c */
int unix_printinfo(char *dirname, struct _info *file, int level);
int unix_printfile(char *dirname, char *filename, struct _info *file, int descend);
int unix_error(char *error);
void unix_newline(struct _info *file, int level, int postdir, int needcomma);
void unix_report(struct totals tot);

/* html.c */
void html_intro(void);
void html_outtro(void);
int html_printinfo(char *dirname, struct _info *file, int level);
int html_printfile(char *dirname, char *filename, struct _info *file, int descend);
int html_error(char *error);
void html_newline(struct _info *file, int level, int postdir, int needcomma);
void html_close(struct _info *file, int level, int needcomma);
void html_report(struct totals tot);
void html_encode(FILE *fd, char *s);

/* xml.c */
void xml_intro(void);
void xml_outtro(void);
int xml_printinfo(char *dirname, struct _info *file, int level);
int xml_printfile(char *dirname, char *filename, struct _info *file, int descend);
int xml_error(char *error);
void xml_newline(struct _info *file, int level, int postdir, int needcomma);
void xml_close(struct _info *file, int level, int needcomma);
void xml_report(struct totals tot);

/* json.c */
void json_indent(int maxlevel);
void json_fillinfo(struct _info *ent);
void json_intro(void);
void json_outtro(void);
int json_printinfo(char *dirname, struct _info *file, int level);
int json_printfile(char *dirname, char *filename, struct _info *file, int descend);
int json_error(char *error);
void json_newline(struct _info *file, int level, int postdir, int needcomma);
void json_close(struct _info *file, int level, int needcomma);
void json_report(struct totals tot);

/* color.c */
void parse_dir_colors(void);
int color(u_short mode, char *name, bool orphan, bool islink);
void endcolor(void);
const char *getcharset(void);
void initlinedraw(int);

/* hash.c */
char *uidtoname(uid_t uid);
char *gidtoname(gid_t gid);
int findino(ino_t, dev_t);
void saveino(ino_t, dev_t);

/* file.c */
struct _info **file_getfulltree(char *d, u_long lev, dev_t dev, off_t *size, char **err);
struct _info **tabedfile_getfulltree(char *d, u_long lev, dev_t dev, off_t *size, char **err);

/* filter.c */
void gittrim(char *s);
struct pattern *new_pattern(char *pattern);
int filtercheck(char *path, char *name, int isdir);
struct ignorefile *new_ignorefile(char *path, bool checkparents);
void push_filterstack(struct ignorefile *ig);
struct ignorefile *pop_filterstack(void);

/* info.c */
struct infofile *new_infofile(char *path, bool checkparents);
void push_infostack(struct infofile *inf);
struct infofile *pop_infostack(void);
struct comment *infocheck(char *path, char *name, int top, int isdir);
void printcomment(int line, int lines, char *s);

/* list.c */
void new_emit_unix(char **dirname, bool needfulltree);


/* We use the strverscmp.c file if we're not linux: */
#ifndef __linux__
int strverscmp (const char *s1, const char *s2);
#endif
