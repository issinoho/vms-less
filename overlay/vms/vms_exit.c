/* vms_exit.c - exit() for GNU findutils on OpenVMS.

   With _POSIX_EXIT the VSI C RTL encodes exit(n) as %X35A000 + n*8 + 1,
   which has success severity, so to DCL a failed run looks successful.
   lib/stdlib.h (patch) routes exit() here.  Under a Unix shell (GNV bash:
   SHELL is set and is not "DCL") keep the POSIX exit, which the shell
   decodes as $?.  Under DCL, exit code 0 is success.  Every other code N
   is an error: find's 1 (a file or directory could not be processed) and
   xargs's 1 and 123-127 (a command failed, was killed, or could not be
   run) all mean the job did not do everything it was asked.  N becomes an
   error-severity status with the message suppressed (%X1035A002 + N*8), so
   $SEVERITY is 2 and ON ERROR fires; N is (status & %X7F8) / 8.

   Part of the OpenVMS port of GNU findutils
   (github.com/issinoho/vms-findutils), as in vms-gzip and vms-tar;
   distributed under the GNU General Public License, version 3 or later.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>

void decc$exit (int status);
void decc$__posix_exit (int status);

void
vms_exit (int status)
{
  const char *shell = getenv ("SHELL");
  if (shell != NULL && strcmp (shell, "DCL") != 0)
    decc$__posix_exit (status);
  if (status == 0)
    decc$exit (1);
  decc$exit (0x10000000 | 0x35A000 | ((status & 0xFF) << 3) | 2);
}
