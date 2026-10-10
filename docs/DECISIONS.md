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
