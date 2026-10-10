/* vms_env.c - environment variables for less on OpenVMS, without DCL symbols.

   The VSI C RTL's getenv() looks in the C environment, then logical names,
   then DCL symbols.  The command that runs less is itself a DCL symbol,
   LESS :== $LESS$ROOT:[BIN]LESS.EXE, so getenv ("LESS") returned the
   image's file name and less took it for its options ("E" made it quit at
   the end of the file).  less's own lookup uses vms_getenv instead: the C
   environment (TERM, HOME, PATH, USER, and anything set with setenv), then
   logical names, never symbols (docs/DECISIONS.md D3).  Options go in a
   logical name, as for other VMS programs:

       $ DEFINE LESS "-i -M"

   Part of the OpenVMS port of less (github.com/issinoho/vms-less);
   distributed under the GNU General Public License, version 3 or later.  */

#include "defines.h"
#include "vms_env.h"

#include <descrip.h>
#include <lnmdef.h>
#include <starlet.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

char const *
vms_getenv (char const *name)
{
  static $DESCRIPTOR (table, "LNM$FILE_DEV");
  size_t len = strlen (name);
  char **e;
  struct dsc$descriptor_s lnm;
  char value[256];
  unsigned short value_len = 0;
  struct
  {
    unsigned short len, code;
    void *buf;
    unsigned short *retlen;
    unsigned int end;
  } items = { sizeof value - 1, LNM$_STRING, 0, 0, 0 };
  char *copy;

  for (e = environ; e && *e; e++)
    if (strncmp (*e, name, len) == 0 && (*e)[len] == '=')
      return *e + len + 1;

  lnm.dsc$w_length = (unsigned short) len;
  lnm.dsc$b_dtype = DSC$K_DTYPE_T;
  lnm.dsc$b_class = DSC$K_CLASS_S;
  lnm.dsc$a_pointer = (char *) name;
  items.buf = value;
  items.retlen = &value_len;
  if (!(sys$trnlnm (0, &table, &lnm, 0, &items) & 1))
    return NULL;
  value[value_len] = '\0';
  /* less keeps the pointer (its getenv cache), so the value must last.  */
  copy = malloc (value_len + 1);
  if (copy)
    memcpy (copy, value, value_len + 1);
  return copy;
}
