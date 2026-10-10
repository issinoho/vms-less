$! LESS$STARTUP.COM - system startup for less on OpenVMS
$!
$! Installed by PCSI into SYS$STARTUP.  Defines the system logical name
$! LESS$ROOT, pointing at the installed [LESS] directory.  To run it at every
$! boot, add this line to SYS$MANAGER:SYSTARTUP_VMS.COM:
$!
$!     $ @SYS$STARTUP:LESS$STARTUP.COM
$!
$! P1 = "INSTALL": also print the post-installation tasks (PCSI runs it so).
$! P1 = "REMOVE":  deassign LESS$ROOT instead (PCSI runs it so at removal).
$!
$! Users then define the commands with
$!     $ @LESS$ROOT:[000000]LESS$SETUP.COM
$!
$ set noon
$ mode = f$edit(p1, "UPCASE")
$ if mode .eqs. "REMOVE"
$ then
$   if f$trnlnm("LESS$ROOT", "LNM$SYSTEM_TABLE") .nes. "" then -
        deassign/system/executive_mode LESS$ROOT
$   exit 1
$ endif
$!
$! This procedure sits in <destination>[SYS$STARTUP]; the product is in
$! <destination>[LESS].  Rooted logicals need the physical form:
$! DKA0:[SYS0.SYSCOMMON.SYS$STARTUP] -> DKA0:[SYS0.SYSCOMMON.LESS.]
$ proc = f$environment("PROCEDURE")
$ dev = f$parse(proc,,,"DEVICE","NO_CONCEAL")
$ dir = f$edit(f$parse(proc,,,"DIRECTORY","NO_CONCEAL"), "UPCASE") - "]["
$ root = dir - "SYS$STARTUP]" + "LESS.]"
$ if root .eqs. dir + "LESS.]"
$ then
$   write sys$error "LESS$STARTUP: expected to be in a [SYS$STARTUP] directory, not ''dir'"
$   exit 44
$ endif
$ root = root - ".000000"
$ define/system/executive_mode/translation_attributes=concealed LESS$ROOT 'dev''root'
$ if f$search("LESS$ROOT:[BIN]LESS.EXE") .eqs. ""
$ then
$   write sys$error "LESS$STARTUP: LESS.EXE not found under ''dev'''root'"
$   exit 44
$ endif
$ if mode .nes. "INSTALL" then exit 1
$ say = "write sys$output"
$ say ""
$ say "    Post-installation tasks for less"
$ say ""
$ say "    At system startup: to define LESS$ROOT at every boot, add this line to"
$ say "    SYS$MANAGER:SYSTARTUP_VMS.COM:"
$ say "    $ @SYS$STARTUP:LESS$STARTUP.COM"
$ say "    For each user: to define the commands, add this line to LOGIN.COM:"
$ say "    $ @LESS$ROOT:[000000]LESS$SETUP.COM"
$ say ""
$ say "    PRODUCT REMOVE LESS removes the product and deassigns LESS$ROOT."
$ say ""
$ exit 1
