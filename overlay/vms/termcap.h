/* termcap.h - the termcap functions of vms/vms_termcap.c (docs/DECISIONS.md
   D1).  OpenVMS has no termcap library; screen.c includes this header as
   <termcap.h> (vms-manual.site: ac_cv_header_termcap_h=yes).  */
#ifndef VMS_TERMCAP_H
#define VMS_TERMCAP_H

int tgetent (char *bp, char const *name);
int tgetflag (char const *id);
int tgetnum (char const *id);
char *tgetstr (char const *id, char **area);
char *tgoto (char const *cm, int col, int row);
int tputs (char const *str, int affcnt, int (*putc_fn) (int));

#endif
