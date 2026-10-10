$! VMS_INSTALLCHECK.COM <tree-dir-name> - install the LESS kit, verify, smoke-test
$! the installed image, then remove it.  Changes the system while it runs (PCSI
$! database, SYS$COMMON:[LESS], system logical LESS$ROOT); leaves it as it was.
$ set noon
$ arch = f$edit(f$getsyi("ARCH_NAME"), "UPCASE")
$ base = "I64VMS"
$ if arch .eqs. "X86_64" then base = "X86VMS"
$ tree = f$environment("DEFAULT") - "]" + "." + p1 + "]"
$ kitdir = tree - "]" + ".KIT_''arch']"
$ write sys$output "=== INSTALL from ", kitdir
$ product install LESS /producer=ISSINOHO /base_system='base' /source='kitdir' /options=noconfirm /log
$ write sys$output "=== install status ", $status
$ product show product LESS /producer=ISSINOHO
$ write sys$output "=== VERIFY"
$ write sys$output "startup procedure: [", f$search("SYS$STARTUP:LESS$STARTUP.COM"), "]"
$ show logical LESS$ROOT
$ directory/nohead/notrail LESS$ROOT:[000000...]*.*
$ @LESS$ROOT:[000000]LESS$SETUP.COM
$ show symbol less
$ less "--version"
$ write sys$output "=== SMOKE TEST on installed image"
$ smoke = tree - "]" + ".VMS]TEST_SMOKE.COM"
$ @'smoke' LESS$ROOT:[BIN]LESS.EXE
$ write sys$output "=== REMOVE"
$ product remove LESS /producer=ISSINOHO /options=noconfirm /log
$ write sys$output "=== remove status ", $status
$ write sys$output "LESS$ROOT after removal: [", f$trnlnm("LESS$ROOT"), "]"
$ write sys$output "files after removal: [", f$search("SYS$COMMON:[LESS...]*.*"), "]"
$ write sys$output "startup after removal: [", f$search("SYS$STARTUP:LESS$STARTUP.COM"), "]"
$ product show product LESS /producer=ISSINOHO
$ delete/symbol/global less
