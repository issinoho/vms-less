/* vms_tty.c - keyboard input and screen output for less on OpenVMS.

   less reads keys from the terminal in raw mode and draws the screen with
   escape sequences.  On Unix it sets raw mode with termios; the VSI C RTL
   declares the termios functions "NOT IMPLEMENTED", and its write() to a
   terminal goes through the terminal driver's formatting, which wraps
   lines at the width it believes the cursor has reached and counts escape
   sequences as printing characters.  So, as full-screen VMS programs do,
   less talks to the terminal driver directly through a channel to
   SYS$COMMAND (docs/DECISIONS.md D2):

   - a key is read with IO$_READVBLK, IO$M_NOECHO and IO$M_NOFILTR, one
     byte, and no terminators: every byte (escape sequences, ^Z, DEL)
     reaches less as typed;
   - whether a key is waiting is the driver's type-ahead count
     (IO$_SENSEMODE with IO$M_TYPEAHDCNT), in place of poll();
   - output to the terminal is written with IO$_WRITEVBLK and
     IO$M_NOFORMAT, with NL sent as CR LF as a Unix tty driver does;
   - CTRL/C is a CTRL/C AST on the channel, which does what less's SIGINT
     handler does (sigs |= S_INTERRUPT, so a long search stops) and cancels
     a pending keyboard read; without it the terminal driver would cancel
     the image.  The AST does not raise SIGINT: less's handler may
     longjmp, which an AST must not.

   Part of the OpenVMS port of less (github.com/issinoho/vms-less);
   distributed under the GNU General Public License, version 3 or later.  */

#include "less.h"
#include "vms_tty.h"

#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>
#include <starlet.h>
#include <unistd.h>

extern int sigs;                /* signal.c */

static unsigned short chan;
static int assigned;

static void arm_ctrlc (void);

/* CTRL/C: as less's SIGINT handler, without its longjmp.  */
static void
ctrlc_ast (void *arg)
{
  (void) arg;
  sigs |= S_INTERRUPT;
  sys$cancel (chan);            /* end a pending keyboard read */
  arm_ctrlc ();                 /* the AST is delivered once per request */
}

struct iosb
{
  unsigned short status;
  unsigned short count;
  unsigned int info;
};

static void
arm_ctrlc (void)
{
  struct iosb iosb;
  sys$qiow (0, chan, IO$_SETMODE | IO$M_CTRLCAST, &iosb, 0, 0,
            (void *) ctrlc_ast, 0, 0, 0, 0, 0);
}

static int
tty_chan (void)
{
  static $DESCRIPTOR (dev, "SYS$COMMAND");
  if (!assigned)
    {
      if (!(sys$assign (&dev, &chan, 0, 0, 0) & 1))
        return 0;
      assigned = 1;
      arm_ctrlc ();
    }
  return 1;
}

int
vms_tty_open (void)
{
  return tty_chan () ? 0 : -1;
}

int
vms_tty_getc (unsigned char *c)
{
  struct iosb iosb;
  unsigned int terminators[2] = { 0, 0 };  /* short form: no terminators */
  unsigned int status;

  if (!tty_chan ())
    return -1;
  status = sys$qiow (0, chan, IO$_READVBLK | IO$M_NOECHO | IO$M_NOFILTR,
                     &iosb, 0, 0, c, 1, 0, terminators, 0, 0);
  if (status & 1)
    status = iosb.status;
  if (status == SS$_ABORT || status == SS$_CANCEL || status == SS$_CONTROLC)
    return -2;                  /* interrupted */
  if (!(status & 1))
    return -1;
  return iosb.count == 1 ? 1 : 0;
}

int
vms_tty_ready (void)
{
  struct iosb iosb;
  struct
  {
    unsigned short count;
    unsigned char first;
    unsigned char reserved[5];
  } typeahead;

  if (!tty_chan ())
    return 0;
  if (!(sys$qiow (0, chan, IO$_SENSEMODE | IO$M_TYPEAHDCNT, &iosb, 0, 0,
                  &typeahead, sizeof typeahead, 0, 0, 0, 0) & 1)
      || !(iosb.status & 1))
    return 0;
  return typeahead.count > 0;
}

/* Write LEN bytes unformatted through the terminal channel.  */
static int
qio_write (char const *buf, unsigned long len)
{
  struct iosb iosb;
  unsigned int status = sys$qiow (0, chan, IO$_WRITEVBLK | IO$M_NOFORMAT,
                                  &iosb, 0, 0, (void *) buf, len, 0, 0, 0, 0);
  if (status & 1)
    status = iosb.status;
  return status & 1;
}

long
vms_tty_write (int fd, char const *buf, unsigned long n)
{
  char out[4096];
  unsigned long i, o = 0;

  if (!isatty (fd) || !tty_chan ())
    return (long) write (fd, buf, n);
  /* NL becomes CR LF, as a Unix tty driver's output processing (ONLCR)
     does, which less relies on: unformatted output has none.  */
  for (i = 0; i < n; i++)
    {
      if (o >= sizeof out - 2)
        {
          if (!qio_write (out, o))
            return -1;
          o = 0;
        }
      if (buf[i] == '\n')
        out[o++] = '\r';
      out[o++] = buf[i];
    }
  if (o > 0 && !qio_write (out, o))
    return -1;
  return (long) n;
}
