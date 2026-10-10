/* vms_env.h - getenv () for less on OpenVMS without DCL symbols
   (vms_env.c; docs/DECISIONS.md D3).  */
#ifndef VMS_ENV_H
#define VMS_ENV_H

/* NAME's value from the C environment or a logical name, or NULL.  */
char const *vms_getenv (char const *name);

#endif
