# Porting log

One entry per build or run problem, newest last. Triage codes:
- T toolchain
- P probe or configure
- H header
- L libc (CRTL)
- F filesystem
- S shell or process
- U upstream

## 2026-10-10: scaffold (from vms-findutils)

| Code | Problem | Root cause | Fix |
|------|---------|------------|-----|
| – | Tooling assumed gnulib | less has none; its sources and `defines.h` are at the top of the tree | `prepare.sh` copies `defines.h` and lists the Makefile's OBJ; `push.sh` uploads the top-level sources; no library in `descrip.mms` |
| T | `prepare.sh` could not extract the tarball | It assumed `.tar.xz`; less ships `.tar.gz` | `tar -xf` (detects the format) |
| T | configure: empty `--build` | less's tarball has no `build-aux/config.guess` | `prepare.sh` and `vms_configure.sh` fall back to `gcc -dumpmachine` |
| T | PCSI version `V710.-0E1`, then `V710.0` (%PCSI-E-S_INVVER) | less's versions are one number; PCSI does not take 710 as a major version | 710 is V7.10-0E1: hundreds and the rest |
| P | No terminal library | VMS has no termcap or terminfo | D1: `vms_termcap.c`, configure `--without-termlib` |

## 2026-10-10: configure and first builds (IA64)

| Code | Problem | Root cause | Fix |
|------|---------|------------|-----|
| T | Concurrent `vms_configure.sh` on both nodes: IA64 stopped after "starting compile server" | Both started within a second; the IA64 one never submitted its server (not reproduced since) | Started again on its own. A run killed with `timeout` left its batch server and ssh masters: stopped with `CCSERVER.STOP` and by PID |
| P | configure: 7 min, 69 tests compiled on VMS; answers identical on both nodes | – | 5 answers left from the host run, all build-tool paths |
| P | `HAVE_TERMIOS_FUNCS` undefined although `<termios.h>` exists | VSI's `<termios.h>` comments the functions out: "NOT IMPLEMENTED" (probed on both nodes) | D2: `vms_tty.c` |
| T | `%MMS-F-GWKNOPRN` for `config.h` | gen_mms.py assumed `config.h`; less's header is `defines.h` | `GEN_MMS_CONFIG_H=defines.h` |
| H | `%CC-E-CLOSEPAREN` in decode.c | `SYSDIR` comes from less's Makefile (`-D`) | descrip.mms defines `SYSDIR="/LESS$ROOT"` (the system lesskey file is `LESS$ROOT:[000000]sysless`) |
| T | Pushed fixes not built | less's tarball unpacks read-only; the first push copied the protection, and SFTP could not replace the files later | `prepare.sh`: `chmod -R u+w` the staged tree; protections on the node reset once |
| H | `<sgtty.h>` not found; `struct termio` incomplete in `set_termio_flags` | No termios, so screen.c fell through to older APIs; VMS's `<sys/ioctl.h>` defines `TCGETA` | Patch 0001 |
| H | `tgetent` etc. implicitly declared | screen.c includes `<termcap.h>` only if configure finds one | `vms/termcap.h`; `ac_cv_header_termcap_h=yes` |
| – | exit status | less has no gnulib `<stdlib.h>` to route `exit()` | descrip.mms defines `exit=vms_exit` |

## 2026-10-10: interactive tests (IA64, ssh pseudo-terminal, VT500 series)

| Code | Problem | Root cause | Fix |
|------|---------|------------|-----|
| S | Each line started where the last ended | Unformatted output has no NL to CR LF translation (Unix: ONLCR) | `vms_tty_write` sends NL as CR LF |
| L | less quit at the end of every file | `getenv("LESS")` returned the DCL symbol `LESS :== $...LESS.EXE`, read as options (`E`) | D3: `vms_env.c`, patch 0002 |
| S | `!show time`: output overwritten by "!done" | VMS writes the command's output as records (LF, text, CR) | Patch 0003 |
| S | CTRL/C cancelled less | No CTRL/C AST, so the driver cancelled the image; then the read ended with SS$_CONTROLC, which `vms_tty_getc` took for an error (exit 1) | CTRL/C AST in `vms_tty.c`; SS$_CONTROLC is an interrupt |

## 2026-10-10: smoke test (IA64)

`[.VMS]TEST_SMOKE.COM` passes 8 of 8 on IA64.

| Code | Problem | Root cause | Fix |
|------|---------|------------|-----|
| F | `less ref.txt` (a file made with CREATE) took the CPU until stopped; the node stopped answering ssh (one CPU) | A variable-length record file reads as lines, but the C RTL cannot lseek to the byte offsets less computes, so less read the same data again forever (as vms-tar's D1: record files' sizes and offsets are not byte counts) | Patch 0004: record files are not seekable; less keeps what it read, as for a pipe. Tested interactively on a 723-file DIRECTORY listing: G, g, search, b |

## 2026-10-10: commands (IA64)

| Code | Problem | Root cause | Fix |
|------|---------|------------|-----|
| S | `v` did nothing visible | The default LESSEDIT gave `EDIT +1 file`; DCL's EDIT takes no `+line` | Patch 0005: `%E %g` on VMS |
| S | `v`: EDIT "Error parsing \[.dir\]x.c" | less quotes names for a Unix shell; DCL keeps the backslashes | Patch 0006: no shell quoting on VMS |
| – | LESSOPEN | `popen()` of a DCL command works (`DEFINE LESSOPEN "|SEARCH %s define"`) | none |
