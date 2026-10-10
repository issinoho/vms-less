# vms-less

less 710, the pager, for OpenVMS (IA64 and x86-64), built natively with VSI C.

The repository stores only our changes over the signed upstream release:
- `patches/` holds changes to upstream files, applied in `patches/series` order.
- `overlay/` holds new files: VMS build procedures, the terminal support and the PCSI kit.

`tools/prepare.sh` combines them with the tarball into `staging/`, which is pushed to a VMS node and built there with MMS. The tooling is shared with the other ports in the family; see [vms-grep](https://github.com/issinoho/vms-grep) for the list.

## Status

| Step | IA64 (V8.4-2L3) | x86-64 (E9.2-4) |
|------|-----------------|-----------------|
| Configure with VSI C (`tools/vms_configure.sh`) | done | done (same answers) |
| Build (`tools/build.sh`) | clean | clean |
| DCL smoke test, 8 checks (`tools/test.sh`) | 8/8 | 8/8 |
| Interactive test, 16 checks (`tools/ttytest.sh`) | 16/16 | 16/16 |
| PCSI kit `ISSINOHO <base> LESS V7.10-0E1` (`tools/kit.sh`) | built | built |
| Install check (`tools/installcheck.sh`) | - | - |

## Workflow

```sh
cp tools/nodes.conf.example tools/nodes.conf   # then fill in the real nodes
tools/prepare.sh                               # fetch, verify, patch, configure, stage
tools/vms_configure.sh <node>                  # once per upstream release: configure with VSI C
tools/build.sh <node>                          # push and build with MMS
tools/test.sh <node>                           # smoke test (non-interactive, on VMS)
tools/ttytest.sh <node>                        # interactive test from the host (pexpect, pyte)
tools/kit.sh <node>                            # PCSI kit
tools/installcheck.sh <node>                   # install, verify, remove
```

## Using it

```
$ @LESS$ROOT:[000000]LESS$SETUP.COM
$ less LOGIN.COM
$ pipe directory/full | less
$ define less "-i -M"          ! options: a logical name, not a symbol
```

- **Terminals:** VT100, VT200-VT500 series, DECterm, and xterm or other ANSI emulators over
  ssh. The type comes from the terminal's device type, or from `DEFINE TERM "xterm"`; the size
  follows SET TERMINAL/WIDTH and /PAGE. See [docs/DECISIONS.md](docs/DECISIONS.md) D1.
- **Keys:** less reads the keyboard raw through the terminal driver, so the arrow keys, the
  editing keypad and PF1-PF4 work, CTRL/C stops a search without leaving less, and DEL erases
  (D2).
- **Files:** VMS text files of any record format, files named in VMS or Unix syntax, piped
  input.
- **Commands:** `!command` runs DCL, `v` runs EDIT (or `EDITOR`/`VISUAL`), and LESSOPEN can
  name a DCL command (`DEFINE LESSOPEN "|SEARCH %s ERROR"`).

**Known limits** (also in the kit's README.VMS):
- Options and other variables are read from logical names, not DCL symbols: the `LESS`
  symbol is the command itself (D3).
- `v` starts the editor at the top of the file; set LESSEDIT for an editor's own way to start
  at a line.
- lessecho is not built (less finds files by itself).
- A record file (VAR, VFC, FIX) is read like a pipe: less keeps what it has read in memory.
- `>` on the command line is not redirected by the C RTL; use `PIPE`.

## What the port changes

| Change | Why |
|---|---|
| `vms/vms_termcap.c` + `vms/termcap.h` | No termcap library on VMS: VT100, VT220+ and xterm entries ([D1](docs/DECISIONS.md)) |
| `0001-vms-terminal-io` + `vms/vms_tty.c` | No termios: keys, typeahead, screen output and CTRL/C through the terminal driver ([D2](docs/DECISIONS.md)) |
| `0002-vms-getenv-no-symbols` + `vms/vms_env.c` | The C RTL's getenv() reads DCL symbols, and `LESS` is the command's own ([D3](docs/DECISIONS.md)) |
| `0003-vms-lsystem-records` | `!command` output arrives as records: end the last line before "!done" |
| `0004-vms-record-files-not-seekable` | The C RTL cannot lseek a record file by byte offset: read it like a pipe |
| `0005-vms-edit-command` | DCL's EDIT takes no `+line` |
| `0006-vms-no-shell-quoting` | DCL has no shell quoting: names for `v`, `!` and LESSOPEN are passed as they are |
| `vms/vms_exit.c`, `exit=vms_exit` | Under DCL a failure has error severity |
| `vms/vms_crtl_init.c` | DECC$ features: ODS-5 names, Unix name reporting, argument case |

Every problem met and its fix is in [docs/PORTING_LOG.md](docs/PORTING_LOG.md); the design
decisions are in [docs/DECISIONS.md](docs/DECISIONS.md), and the rules and pitfalls for working
on the port in `CLAUDE.md`.
