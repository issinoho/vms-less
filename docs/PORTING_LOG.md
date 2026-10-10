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
| T | PCSI version `V710.-0E1` | less's versions are one number | A missing minor version is 0: V710.0-0E1 |
| P | No terminal library | VMS has no termcap or terminfo | D1: `vms_termcap.c`, configure `--without-termlib` |
