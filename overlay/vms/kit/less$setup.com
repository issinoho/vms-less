$! LESS$SETUP.COM - define the less commands for a user
$!
$! Add to LOGIN.COM (or SYS$MANAGER:SYLOGIN.COM for everyone):
$!     $ @LESS$ROOT:[000000]LESS$SETUP.COM
$!
$! Quote upper-case options, or SET PROCESS/PARSE_STYLE=EXTENDED: traditional DCL
$! parsing changes the case of unquoted arguments; batch jobs use the traditional style.
$!
$ if f$trnlnm("LESS$ROOT") .eqs. ""
$ then
$   write sys$error "LESS$SETUP: LESS$ROOT is not defined; run LESS$STARTUP.COM first"
$   exit 44
$ endif
$ less :== $LESS$ROOT:[BIN]LESS.EXE
$ exit 1
