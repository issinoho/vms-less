$! TEST_SMOKE.COM - quick non-interactive check of a built or installed LESS.EXE
$!
$! Usage:  @[.VMS]TEST_SMOKE [less-image]
$!         The default image is [.BIN_<arch>]LESS.EXE in this tree.
$! Exits with SS$_NORMAL if every test passes, otherwise reports failures.
$! Output here is never a terminal, so less copies its input, as on Unix;
$! the interactive behaviour is tested from the host by tools/ttytest.py.
$!
$ set noon
$ on control_y then goto finish
$ saved_default = f$environment("DEFAULT")
$ saved_parse = f$getjpi("", "PARSE_STYLE_PERM")
$ set process/parse_style=extended
$ proc = f$environment("PROCEDURE")
$ vmsdir = f$parse(proc,,,"DEVICE") + f$parse(proc,,,"DIRECTORY")
$ set default 'vmsdir'
$ set default [-]
$ arch = f$edit(f$getsyi("ARCH_NAME"), "UPCASE")
$ image = p1
$ if image .eqs. "" then image = f$parse("[.BIN_''arch']LESS.EXE")
$ if f$search(image) .eqs. ""
$ then
$   write sys$error "SMOKE: no image ''image'"
$   exit 44
$ endif
$! The command is the symbol LESS, as installed: less must not take the
$! symbol for its LESS options (DECISIONS D3).
$ less :== $'image'
$ pass == 0
$ fail == 0
$ skip == 0
$ if f$search("SMOKE_TMP.DIR") .eqs. "" then create/directory [.SMOKE_TMP]
$ set default [.SMOKE_TMP]
$ call emptydir
$!
$ create ref.txt
the first line
line two, a little longer than the first
line three
$ create two.txt
another file
$ reflines = "the first line|line two, a little longer than the first|line three"
$!
$ call t version   0 ""                 "less --version"
$ call t copy      0 "''reflines'"      "less ref.txt"
$ call t copy-vms  0 "''reflines'"      "less []ref.txt;"
$ call t two-files 0 "''reflines'|another file" "less ref.txt two.txt"
$ call t missing   1 "nosuch.txt: no such file or directory" "less nosuch.txt"
$!
$! Piped input is copied too.
$ if f$search("pipe.txt") .nes. "" then delete/nolog pipe.txt;*
$ pipe type ref.txt | less > pipe.txt
$ n = 0
$ open/read f pipe.txt
$pipe_loop:
$ read/end=pipe_done f line
$ n = n + 1
$ goto pipe_loop
$pipe_done:
$ close f
$ call check pipe "n .eq. 3"
$!
$! A LESS logical name is read (DECISIONS D3): -V prints the version.
$ define/user sys$output opt.txt
$ define less "-V"
$ less
$ deassign less
$ open/read f opt.txt
$ read/end=opt_done f line
$opt_done:
$ close f
$ call check options "f$locate(""less 710"", line) .eq. 0"
$!
$! Under DCL a failure must have error severity, so ON ERROR and
$! IF .NOT. $STATUS see it.
$ define/user sys$output nl:
$ define/user sys$error nl:
$ less nosuch.txt
$ sev = $severity
$ call check severity "sev .eq. 2"
$!
$finish:
$ set default 'vmsdir'
$ set default [-]
$ set process/parse_style='saved_parse'
$ write sys$output "SMOKE: ''pass' passed, ''fail' failed, ''skip' skipped (''image')"
$ set default 'saved_default'
$ if f$type(less) .nes. "" then delete/symbol/global less
$ if fail .eq. 0 .and. pass .gt. 0 then exit 1
$ exit 44
$!
$! --- T name expected-exit expected-output command [input-file] -----------
$! An expected output of "*" is not compared.
$t: subroutine
$ set noon
$ if f$search("out.txt") .nes. "" then delete/nolog out.txt;*
$ if f$search("err.txt") .nes. "" then delete/nolog err.txt;*
$! Separate files: two streams on one name would make two versions.
$ define/user sys$output out.txt
$ define/user sys$error err.txt
$ if p5 .nes. "" then define/user sys$input 'p5'
$ 'p4'
$ st = $status
$! _POSIX_EXIT: the C exit code is in bits 3..10 of the VMS status.
$ code = (st .and. %X7F8) / 8
$ got = ""
$ do = "out.txt"
$readfile:
$ if f$search(do) .eqs. "" then goto nextfile
$ open/read f 'do'
$readloop:
$ read/end=readdone f line
$ if got .nes. "" then got = got + "|"
$ got = got + f$edit(line, "TRIM,COMPRESS")
$ if f$length(got) .gt. 300 then goto readdone
$ goto readloop
$readdone:
$ close f
$nextfile:
$ if do .eqs. "err.txt" then goto compare
$ do = "err.txt"
$ goto readfile
$compare:
$ ok = code .eq. f$integer(p2)
$! (DCL upper-cases the unquoted test name.)
$ if p1 .eqs. "VERSION"
$ then
$   ok = ok .and. (f$locate("less 710", got) .eq. 0)
$ else
$   if p3 .nes. "*" then ok = ok .and. (got .eqs. f$edit(p3, "TRIM,COMPRESS"))
$ endif
$ if ok
$ then
$   write sys$output "PASS ", p1
$   pass == pass + 1
$ else
$   write sys$output "FAIL ", p1, ": exit ", code, " (want ", p2, "), output [", got, "] (want [", p3, "])"
$   fail == fail + 1
$ endif
$ exit 1
$ endsubroutine
$!
$! --- CHECK name dcl-expression ----------------------------------------
$check: subroutine
$ if 'p2'
$ then
$   write sys$output "PASS ", p1
$   pass == pass + 1
$ else
$   write sys$output "FAIL ", p1, ": ", p2
$   fail == fail + 1
$ endif
$ exit 1
$ endsubroutine
$!
$! --- EMPTYDIR: delete everything under the current directory -------------
$! Directory files must be writable and empty to go, so repeat the delete
$! until nothing is left (the tests nest three levels below [.SMOKE_TMP]).
$emptydir: subroutine
$ set noon
$ if f$search("[...]*.*;*") .eqs. "" then exit 1
$ define/user sys$output nl:
$ define/user sys$error nl:
$ set protection=(s:rwed,o:rwed) [...]*.*;*
$ n = 0
$emptyloop:
$ define/user sys$output nl:
$ define/user sys$error nl:
$ delete/nolog [...]*.*;*
$ n = n + 1
$ if f$search("[...]*.*;*") .nes. "" .and. n .lt. 6 then goto emptyloop
$ exit 1
$ endsubroutine
