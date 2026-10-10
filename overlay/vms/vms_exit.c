/* vms_exit.c - exit() for less on OpenVMS.

   With _POSIX_EXIT the VSI C RTL encodes exit(n) as %X35A000 + n*8 + 1,
   which has success severity, so to DCL a failed run looks successful.
   descrip.mms defines exit as vms_exit, so exit() comes here (less has no
   gnulib <stdlib.h> to route it).  Under a Unix shell (GNV bash:
   SHELL is set and is not "DCL") keep the POSIX exit, which the shell
   decodes as $?.  Under DCL, exit code 0 is success; less's other codes (1
   for an error, 2 for an interrupt in some versions) are errors: N becomes
   an error-severity status with the message suppressed (%X1035A002 + N*8),
   so $SEVERITY is 2 and ON ERROR fires; N is (status & %X7F8) / 8.

   Part of the OpenVMS port of less (github.com/issinoho/vms-less), as in
   vms-gzip, vms-tar and vms-findutils; distributed under the GNU General
   Public License, version 3 or later.  */

#include "defines.h"

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
