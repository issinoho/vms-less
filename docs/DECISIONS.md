# Design decisions

## D1. The terminal: a termcap shim with built-in VT and xterm entries

**Status:** decided by the user (2026-10-10); `vms/vms_termcap.c`, not yet tested on VMS.

less drives the terminal through termcap or terminfo, and its configure stops without one of
those libraries. OpenVMS has neither (the C RTL's curses has no termcap API).

**Decision:** `vms/vms_termcap.c` supplies the six termcap functions less uses (tgetent,
tgetstr, tgetnum, tgetflag, tgoto, tputs) from built-in entries, and configure runs
`--without-termlib`. less's Unix screen code then runs unchanged.
- **Entries:** `vt100`; `vt220` for the VT200-VT500 series and DECterm (adds insert and
  delete line and the editing keypad); `xterm` for xterm, ANSI and screen/tmux (adds the
  alternate screen). Any other type is a VT100, the subset every VMS terminal has; `dumb`
  and the VT52 (no ANSI sequences) make tgetent fail, so less works as on a dumb terminal.
- **Type:** the TERM less passes to tgetent. The C RTL sets it from the terminal's device
  type (`vt300-80`); a user may set it to anything (`xterm-256color`).
- **Size:** `co` and `li` come from SYS$GETDVI on SYS$COMMAND (DVI$_DEVBUFSIZ,
  DVI$_TT_PAGE), so SET TERMINAL/WIDTH and /PAGE are honoured; LINES and COLUMNS still win.

**Rejected:** backing the same calls with SMG$GET_TERM_DATA and TERMTABLE (any terminal VMS
knows, but more code for terminals that are now rare); a native VMS screen backend in
`screen.c`, like less's MS-DOS and Windows ones (large patches to carry).

## D2. Keyboard and screen through the terminal driver

**Status:** forced by the C RTL (2026-10-10); `vms/vms_tty.c` and patch 0001; tested
interactively on IA64.

The VSI C RTL has no termios: its `<termios.h>` lists the functions as "NOT IMPLEMENTED"
(on IA64 and x86-64). There is no `<termio.h>` or `<sgtty.h>` either, `poll()` does not work
on a terminal, and `write()` to a terminal goes through the driver's formatting, which
wraps lines where it believes the cursor is and counts escape sequences as printing
characters. So less talks to the terminal driver through a channel to SYS$COMMAND, as
full-screen VMS programs do:
- **Keys:** `IO$_READVBLK` with `IO$M_NOECHO` and `IO$M_NOFILTR`, one byte, no terminators,
  so every byte (escape sequences, ^Z, DEL) reaches less as typed. No terminal mode is
  changed, so nothing needs restoring after a crash or CTRL/Y.
- **Typeahead:** `IO$_SENSEMODE|IO$M_TYPEAHDCNT`, in place of `poll()`.
- **Screen:** `IO$_WRITEVBLK|IO$M_NOFORMAT`, with NL sent as CR LF, as a Unix tty driver's
  output processing does; output that is not a terminal still uses `write()`.
- **CTRL/C:** a CTRL/C AST on the channel sets less's interrupt flag and cancels a pending
  read (which then ends with SS$_CONTROLC), so CTRL/C stops a search or a long move and
  less stays, as on Unix. Without it the terminal driver cancels less. The AST does not raise
  SIGINT: less's handler may `longjmp`, which an AST must not.

## D3. Environment variables: not DCL symbols

**Status:** decided while porting (2026-10-10); `vms/vms_env.c` and patch 0002.

The C RTL's `getenv()` falls back to DCL symbols, and the command that runs less is the
symbol `LESS :== $LESS$ROOT:[BIN]LESS.EXE`. less read it as its `LESS` options (the `E` of
`EXE` made it quit at the end of every file). less's own lookup (`lgetenv`) therefore uses
the C environment (TERM, HOME, PATH, USER, anything set with `setenv`) and then logical
names, never symbols. Options go in a logical name, as for other VMS programs:
`$ DEFINE LESS "-i -M"`; so do LESSOPEN, LESSCHARSET and the rest.
