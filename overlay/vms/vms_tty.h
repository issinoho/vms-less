/* vms_tty.h - keyboard input and screen output for less on OpenVMS
   (vms_tty.c; docs/DECISIONS.md D2).  */
#ifndef VMS_TTY_H
#define VMS_TTY_H

/* Assign the terminal channel; 0, or -1 if there is no terminal.  */
int vms_tty_open (void);

/* Read one key byte, raw and without echo: 1, 0 at end of input, -1 for
   an error, -2 if the read was cancelled.  */
int vms_tty_getc (unsigned char *c);

/* Whether a key is waiting (the driver's type-ahead count).  */
int vms_tty_ready (void);

/* Write N bytes to FD: unformatted through the terminal channel when FD
   is the terminal, otherwise with write ().  N, or -1.  */
long vms_tty_write (int fd, char const *buf, unsigned long n);

#endif
