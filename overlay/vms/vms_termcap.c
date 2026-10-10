/* vms_termcap.c - the termcap functions less needs, for OpenVMS.

   less drives the terminal through termcap (tgetent, tgetstr, tgetnum,
   tgetflag, tgoto, tputs).  OpenVMS has no termcap or terminfo library, so
   these supply them from built-in entries (docs/DECISIONS.md D1):

     vt100      VT100 and VT100-compatible terminals
     vt220      VT200, VT300, VT400 and VT500 series, DECterm: also insert
                and delete line and the editing keypad
     xterm      xterm and other ANSI emulators reached over ssh: also the
                alternate screen

   The terminal type is the TERM that less passes to tgetent: the C RTL sets
   it from the terminal's device type ("vt300-80", "vt100"), a user may set
   it to anything else ("xterm-256color").  Any other type is treated as a
   VT100, the common subset of every VMS terminal; tgetent fails only for
   "dumb" and the VT52 (no ANSI sequences), so less then works as on a dumb
   terminal.

   The screen size ("co", "li") is the terminal's own (SYS$GETDVI on
   SYS$COMMAND: DVI$_DEVBUFSIZ and DVI$_TT_PAGE), so SET TERMINAL/WIDTH and
   /PAGE are honoured; less still prefers LINES and COLUMNS when set.

   Part of the OpenVMS port of less (github.com/issinoho/vms-less);
   distributed under the GNU General Public License, version 3 or later.  */

#include <ctype.h>
#include <descrip.h>
#include <dvidef.h>
#include <starlet.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

/* One capability: its two-letter name and value.  Strings hold the bytes
   to send (escapes already decoded); numbers and flags are strings too, as
   "#n" and "" (present).  */
struct cap
{
  char const *name;
  char const *value;
};

#define ESC "\033"

/* Common to every entry.  */
static struct cap const vt100_caps[] = {
  /* Cursor and screen.  */
  {"cm", ESC "[%i%d;%dH"},      /* cursor motion */
  {"ho", ESC "[H"},             /* home */
  {"cl", ESC "[H" ESC "[J"},    /* clear screen */
  {"cd", ESC "[J"},             /* clear to end of screen */
  {"ce", ESC "[K"},             /* clear to end of line */
  {"cr", "\r"},
  {"do", "\n"},
  {"le", "\b"},
  {"bc", "\b"},
  {"nd", ESC "[C"},
  {"up", ESC "[A"},
  {"sr", ESC "M"},              /* scroll reverse */
  {"vb", ESC "[?5h" ESC "[?5l"}, /* visual bell */
  /* Attributes.  */
  {"so", ESC "[7m"},            /* standout: reverse video */
  {"se", ESC "[m"},
  {"us", ESC "[4m"},            /* underline */
  {"ue", ESC "[m"},
  {"md", ESC "[1m"},            /* bold */
  {"mb", ESC "[5m"},            /* blink */
  {"me", ESC "[m"},             /* all attributes off */
  /* Keypad: application mode, so the cursor keys send ESC O x.  */
  {"ks", ESC "[?1h" ESC "="},
  {"ke", ESC "[?1l" ESC ">"},
  {"ku", ESC "OA"},
  {"kd", ESC "OB"},
  {"kr", ESC "OC"},
  {"kl", ESC "OD"},
  {"kb", "\177"},               /* the VT <X| key sends DEL */
  {"k1", ESC "OP"},             /* PF1 */
  {"k2", ESC "OQ"},
  {"k3", ESC "OR"},
  {"k4", ESC "OS"},
  /* Flags and numbers.  */
  {"am", ""},                   /* automatic margins */
  {"xn", ""},                   /* newline glitch: VT100 wraps late */
  {"bs", ""},                   /* backspace moves left */
  {"co", "#80"},
  {"li", "#24"},
  {NULL, NULL}
};

/* VT200 and later, DECterm.  */
static struct cap const vt220_caps[] = {
  {"al", ESC "[L"},             /* insert line */
  {"dl", ESC "[M"},             /* delete line */
  {"kh", ESC "[1~"},            /* Find, as Home */
  {"@7", ESC "[4~"},            /* Select, as End */
  {"kI", ESC "[2~"},            /* Insert Here */
  {"kD", ESC "[3~"},            /* Remove */
  {"kP", ESC "[5~"},            /* Prev Screen */
  {"kN", ESC "[6~"},            /* Next Screen */
  {NULL, NULL}
};

/* xterm and other ANSI emulators.  */
static struct cap const xterm_caps[] = {
  {"al", ESC "[L"},
  {"dl", ESC "[M"},
  {"kh", ESC "OH"},
  {"@7", ESC "OF"},
  {"kI", ESC "[2~"},
  {"kD", ESC "[3~"},
  {"kP", ESC "[5~"},
  {"kN", ESC "[6~"},
  {"ti", ESC "[?1049h"},        /* alternate screen */
  {"te", ESC "[?1049l"},
  {NULL, NULL}
};

/* The current entry's extra capabilities (looked up before vt100_caps).  */
static struct cap const *extra_caps;

/* The terminal's size from its device characteristics, or 0.  */
static int
terminal_size (int item)
{
  static $DESCRIPTOR (dev, "SYS$COMMAND");
  unsigned int value = 0;
  unsigned short len = 0;
  struct
  {
    unsigned short len, code;
    void *buf;
    unsigned short *retlen;
    unsigned int end;
  } list = { sizeof value, 0, 0, 0, 0 };
  list.code = (unsigned short) item;
  list.buf = &value;
  list.retlen = &len;
  if (!(sys$getdviw (0, 0, &dev, &list, 0, 0, 0, 0, 0) & 1))
    return 0;
  return (int) value;
}

static char const *
find_cap (char const *name)
{
  struct cap const *c;
  if (extra_caps)
    for (c = extra_caps; c->name; c++)
      if (strcmp (c->name, name) == 0)
        return c->value;
  for (c = vt100_caps; c->name; c++)
    if (strcmp (c->name, name) == 0)
      return c->value;
  return NULL;
}

int
tgetent (char *bp, char const *name)
{
  (void) bp;
  extra_caps = NULL;
  if (name == NULL || *name == '\0')
    return 1;                   /* a VT100 */
  /* A VT52 has no ANSI sequences at all.  */
  if (strcasecmp (name, "dumb") == 0 || strncasecmp (name, "vt52", 4) == 0)
    return 0;
  if (strncasecmp (name, "xterm", 5) == 0 || strncasecmp (name, "ansi", 4) == 0
      || strncasecmp (name, "screen", 6) == 0 || strncasecmp (name, "tmux", 4) == 0)
    extra_caps = xterm_caps;
  else if (strncasecmp (name, "decterm", 7) == 0
           || (strncasecmp (name, "vt", 2) == 0
               && name[2] >= '2' && name[2] <= '5'
               && isdigit ((unsigned char) name[3])
               && isdigit ((unsigned char) name[4])))
    extra_caps = vt220_caps;
  return 1;
}

int
tgetflag (char const *id)
{
  char const *v = find_cap (id);
  return v != NULL && *v == '\0';
}

int
tgetnum (char const *id)
{
  char const *v;
  if (strcmp (id, "co") == 0)
    {
      int n = terminal_size (DVI$_DEVBUFSIZ);
      if (n > 0)
        return n;
    }
  else if (strcmp (id, "li") == 0)
    {
      int n = terminal_size (DVI$_TT_PAGE);
      if (n > 0)
        return n;
    }
  v = find_cap (id);
  if (v == NULL || v[0] != '#')
    return -1;
  return atoi (v + 1);
}

char *
tgetstr (char const *id, char **area)
{
  char const *v = find_cap (id);
  size_t n;
  if (v == NULL || *v == '\0' || *v == '#')
    return NULL;
  if (area == NULL || *area == NULL)
    return (char *) v;
  n = strlen (v) + 1;
  memcpy (*area, v, n);
  v = *area;
  *area += n;
  return (char *) v;
}

/* Expand a cursor-motion string: %d %2d %3d %. %+c %i %r %% (termcap).  */
char *
tgoto (char const *cm, int col, int row)
{
  static char buf[64];
  char *o = buf;
  int args[2];
  int which = 0;
  args[0] = row;
  args[1] = col;
  if (cm == NULL)
    return (char *) "OOPS";
  while (*cm && o < buf + sizeof buf - 8)
    {
      if (*cm != '%')
        {
          *o++ = *cm++;
          continue;
        }
      cm++;
      switch (*cm++)
        {
        case 'd':
          o += sprintf (o, "%d", args[which++ & 1]);
          break;
        case '2':
          o += sprintf (o, "%02d", args[which++ & 1]);
          break;
        case '3':
          o += sprintf (o, "%03d", args[which++ & 1]);
          break;
        case '.':
          *o++ = (char) args[which++ & 1];
          break;
        case '+':
          *o++ = (char) (args[which++ & 1] + *cm++);
          break;
        case 'i':
          args[0]++;
          args[1]++;
          break;
        case 'r':
          {
            int t = args[0];
            args[0] = args[1];
            args[1] = t;
          }
          break;
        case '%':
          *o++ = '%';
          break;
        default:
          return (char *) "OOPS";
        }
    }
  *o = '\0';
  return buf;
}

/* Send STR through PUTC, skipping termcap padding (leading digits, and
   "$<n>" as in terminfo).  */
int
tputs (char const *str, int affcnt, int (*putc_fn) (int))
{
  (void) affcnt;
  if (str == NULL)
    return -1;
  while (*str >= '0' && *str <= '9')
    str++;
  if (*str == '*')
    str++;
  while (*str)
    {
      if (str[0] == '$' && str[1] == '<')
        {
          char const *e = strchr (str, '>');
          if (e)
            {
              str = e + 1;
              continue;
            }
        }
      putc_fn ((unsigned char) *str++);
    }
  return 0;
}
