# vms-less

less 710, the pager, for OpenVMS (IA64 and x86-64), built natively with VSI C.

The repository stores only our changes over the signed upstream release:
- `patches/` holds changes to upstream files, applied in `patches/series` order.
- `overlay/` holds new files: VMS build procedures, the terminal support and the PCSI kit.

`tools/prepare.sh` combines them with the tarball into `staging/`, which is pushed to a VMS node and built there with MMS. The tooling is shared with the other ports in the family; see [vms-grep](https://github.com/issinoho/vms-grep) for the list.

## Status

Work in progress.

| Step | IA64 (V8.4-2L3) | x86-64 (E9.2-4) |
|------|-----------------|-----------------|
| Configure with VSI C (`tools/vms_configure.sh`) | done | done (same answers) |
| Build (`tools/build.sh`) | clean | clean |
| DCL smoke test, 8 checks (`tools/test.sh`) | 8/8 | 8/8 |
| Interactive test, 14 checks (`tools/ttytest.sh`) | 14/14 | 14/14 |
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

See `CLAUDE.md` for the rules and pitfalls, `docs/PORTING_LOG.md` for each problem met and
`docs/DECISIONS.md` for the design decisions.
