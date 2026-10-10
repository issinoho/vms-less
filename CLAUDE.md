# CLAUDE.md

Guidance for working in this repository: a port of less, the pager, to OpenVMS (IA64 and x86-64)
that stores only our deltas over the upstream release tarball. Read README.md first; it
describes the workflow and design. This file covers the rules and the pitfalls.

## Ground rules

- **Never edit `staging/`, `cache/` or `out/`.** They are regenerated. Every VMS change is
  either a patch (`patches/NNNN-*.patch`, listed in `patches/series`) or a new file in
  `overlay/`. `tools/prepare.sh` refuses overlay files that would replace upstream files.
- **Change an upstream file with a patch.** Make a pristine copy under `a/` and an edited
  copy under `b/`, run `diff -u a/<path> b/<path>`, and put a `Subject:` line and a short
  explanation above the diff. Guard VMS-only code with `#ifdef __VMS` so the patch could go
  upstream. Patches to the same file stack, so diff against the tree as it stands after the
  earlier patches.
- **Configuration answers** go in `overlay/vms/config/vms-manual.site`, each with a comment
  explaining why. Plain `var=value` lines there override the generated
  `configure-<node>.cache`. Overriding a gnulib result often needs its `gl_cv_*` variable
  too, not just `ac_cv_*` (`mempcpy` needed `gl_cv_onwards_func_mempcpy`).
- **Committed files must not contain real node details.** Use `<ia64-host>`, `<x86-host>`
  and `DISK$USER:[USERNAME.VMS_LESS]`. The real values live only in the git-ignored
  `tools/nodes.conf`.
- Keep `docs/TESTING.md` and the README status table in step with test results.

## Commands

```sh
tools/prepare.sh                        # always first after changing patches/overlay
tools/build.sh <ia64|x86> [ALL|CLEAN] [KEEP_GOING]
tools/test.sh <node>                    # smoke test (non-interactive)
tools/ttytest.sh <node>                 # interactive test over an ssh pty (needs pexpect, pyte)
tools/vms.sh <node> dcl '<cmd>' ...     # run DCL; also run/batch/put/get
tools/kit.sh <node>                     # PCSI kit -> out/kits/ (producer ISSINOHO)
tools/installcheck.sh <node>            # install kit, verify, smoke-test, remove (changes system; ask first)
tools/vms_configure.sh <node>           # once per upstream release (slow)
```

Run configure (about an hour per node) and full builds with `run_in_background`.
MMS does not track compiler flags: after changing `ccflags.txt` or `CFLAGS` in
`descrip.mms`, run `tools/build.sh <node> CLEAN` first.

## VMS and tooling pitfalls (learned the hard way)

- **Use `tools/vms.sh`, never raw `ssh host cmd`.** Raw ssh output is often lost, and
  sessions sometimes never close. vms.sh logs to a file and waits for a completion marker.
- **Never use `WAIT` in DCL run over ssh**; it hangs (batch jobs are fine).
- **Never edit a bash script that is running.** bash reads scripts incrementally. Replace
  long-running tools atomically (write a copy, then `mv`).
- **Don't use `pkill -f` / `pgrep -f`** with a pattern that also matches your own shell's
  command line. Kill by explicit PID. On VMS, stop only processes this session started
  (they are network/batch processes of the work account); leave interactive sessions alone.
- **DCL details:**
  - `F$SEARCH` with a wildcard needs a stream id when other `F$SEARCH` calls happen in the
    same loop.
  - Batch jobs default to `/LIST` and `/MAP`.
  - DCL command lines are limited to about 4096 bytes (hence the wildcard librarian step).
  - `CALL` arguments are upper-cased unless quoted.
  - `SYS$LOGIN:[.X]` is not valid on these nodes.
- **`sftp put -r` into an existing directory nests a copy**; push.sh uploads file by file.
- **Run `tools/prepare.sh` after every change to `patches/` or `overlay/`.** build.sh and
  kit.sh push whatever is in `staging/`; forgetting this once shipped a stale kit.
- **stdout on VMS is often record-oriented** (terminal, `/OUTPUT` log, mailbox). The CRTL
  turns each `fwrite` item into a record (patch 0004), and a host-side `grep` treats
  output containing a NUL as binary (use `grep -a`).
- **Layout:** less has no gnulib and no library: its sources are at the top of the tree,
  `defines.h` (configure's header) too, and LESS.EXE is linked from them and the VMS files
  (`gen_mms.py` with an empty library list). lessecho is not built. Versions are one number
  (710): the PCSI version is V710.0-0E<level>.
- **Patches:** the family's generic patches target gnulib files, so none apply here; VMS
  changes to less go in `patches/` as for the other ports.
- **Terminal:** no termcap or terminfo library on VMS. `vms/vms_termcap.c` supplies
  tgetent/tgetstr/tgetnum/tgetflag/tgoto/tputs with VT100, VT220+ and xterm entries, from
  TERM (the C RTL sets it from the device type) and the size from SYS$GETDVI
  (docs/DECISIONS.md D1). configure runs `--without-termlib`.
- **Known porting work:** reading keys from `/dev/tty` (VMS: `TT:`), raw mode through
  termios, output to a record-oriented terminal, `!command`, `v` (editor) and LESSOPEN
  through `system()`/`popen()`, and the exit status under DCL (`vms/vms_exit.c`).
- **A runaway less starves the IA64 node** (one CPU): ssh stops answering. Find it with
  `SHOW SYSTEM/PROCESS=FTA*` (state COM), check `F$GETJPI(pid,"IMAGNAME")` is LESS.EXE,
  and `STOP/ID=` it. Run new interactive or record-file cases with a timeout first.
- `>` on the command line is not redirected; use `PIPE`.

## Commits

Commit in logical steps with messages that explain the VMS reason for each change. Don't
push without the user asking. The GitHub remote is `origin`
(github.com/issinoho/vms-less), branch `main`.
