#!/usr/bin/env python3
"""ttytest.py <node> - drive LESS.EXE interactively on <node> and check the screen.

less is interactive, which TEST_SMOKE.COM (output to files) cannot exercise.
This logs in to the node over ssh in a pseudo-terminal (80x24, a VT500 to
VMS), runs the built LESS.EXE, sends keys, and checks what a VT emulator
(pyte) shows after each.  The test file is made on the node with DCL, so it
is a variable-length record file, the common case on VMS.

Needs pexpect and pyte on the host (python3 -m pip install pexpect pyte, or
set TTYTEST_PYTHON to a virtualenv's python; tools/ttytest.sh does that).
Output: out/tty-<node>.log; exit status 0 only if every check passes.
"""
import os
import re
import sys
import time

import pexpect
import pyte

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_conf(node):
    for line in open(os.path.join(TOP, 'tools', 'nodes.conf')):
        f = line.split()
        if f and not f[0].startswith('#') and f[0] == node:
            return f
    sys.exit('ttytest: unknown node %s' % node)


def upstream():
    conf = {}
    for line in open(os.path.join(TOP, 'upstream.conf')):
        m = re.match(r'(\w+)=(.*)', line.strip())
        if m:
            conf[m.group(1)] = m.group(2)
    return conf


class Session:
    def __init__(self, node):
        _, arch, host, port, user, workdir, _ = read_conf(node)
        self.prompt = None
        self.screen = pyte.Screen(80, 24)
        self.stream = pyte.ByteStream(self.screen)
        self.child = pexpect.spawn(
            'ssh', ['-tt', '-p', port, '-i',
                    os.path.expanduser(os.environ.get('VMS_SSH_KEY', '~/.ssh/vms_ed25519')),
                    '-o', 'BatchMode=yes', '%s@%s' % (user, host)],
            env=dict(os.environ, TERM='vt100'), dimensions=(24, 80), timeout=60)
        self.workdir = workdir
        self.arch = 'IA64' if arch == 'IA64' else 'X86_64'

    def pump(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            try:
                self.stream.feed(self.child.read_nonblocking(65536, timeout=0.2))
            except pexpect.TIMEOUT:
                pass
            except pexpect.EOF:
                return

    def send(self, text, seconds=1.5):
        self.child.send(text)
        self.pump(seconds)

    def dcl(self, command, seconds=1.0):
        self.send(command + '\r', seconds)

    def row(self, n):
        return self.screen.display[n].rstrip()

    def dump(self):
        return '\n'.join('%2d|%s' % (i, l.rstrip()) for i, l in enumerate(self.screen.display))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    node = sys.argv[1]
    up = upstream()
    tree = (up['UPSTREAM_NAME'] + '-' + up['UPSTREAM_VERSION']).replace('.', '_').upper()
    os.makedirs(os.path.join(TOP, 'out'), exist_ok=True)
    log = open(os.path.join(TOP, 'out', 'tty-%s.log' % node), 'w')
    results = []

    def check(name, ok, s):
        results.append(ok)
        line = '%s %s' % ('PASS' if ok else 'FAIL', name)
        print(line)
        log.write(line + '\n')
        if not ok:
            log.write(s.dump() + '\n')

    s = Session(node)
    s.child.expect([r'::', r'\$ ', pexpect.TIMEOUT], timeout=60)
    s.pump(2)
    s.dcl('set default %s' % s.workdir)
    s.dcl('set terminal/page=24/width=80')
    s.dcl('less :== $\'f$parse("[.%s.BIN_%s]LESS.EXE")\'' % (tree, s.arch))
    # 200 numbered lines, a variable-length record file.
    # (Interactive DCL has no GOTO: the loop is a procedure.)
    s.dcl('create ttymk.com')
    for line in ('$ open/write tt ttytest.txt', '$ i = 1',
                 '$ loop: write tt f$fao("line !3ZL", i)', '$ i = i + 1',
                 '$ if i .le. 200 then goto loop', '$ close tt'):
        s.send(line + '\r', 0.3)
    s.send('\x1a', 1)
    s.dcl('@ttymk', 3)
    s.dcl('delete/nolog ttymk.com;*')
    s.screen.reset()

    s.dcl('less ttytest.txt', 3)
    check('open', s.row(0) == 'line 001' and s.row(22) == 'line 023'
          and 'ttytest.txt' in s.row(23), s)
    s.send(' ')
    check('forward', s.row(0) == 'line 024' and s.row(23) == ':', s)
    s.send('b')
    check('back', s.row(0) == 'line 001', s)
    s.send('G')
    check('end', s.row(22) == 'line 200' and s.row(23).startswith('(END)'), s)
    s.send('g')
    check('start', s.row(0) == 'line 001', s)
    s.send('/line 150\r')
    check('search', s.row(0) == 'line 150', s)
    s.send('\x1bOB')
    check('arrow-down', s.row(0) == 'line 151', s)
    s.send('\x03')
    check('ctrl-c', s.row(23) == ':' and s.row(0) == 'line 151', s)
    s.send('!show time\r', 3)
    check('shell', s.row(21) == '!show time' and re.search(r'\d{2}:\d{2}:\d{2}', s.row(22))
          is not None and s.row(23).startswith('!done'), s)
    s.send('\r')
    check('shell-return', s.row(0) == 'line 151', s)
    s.send('h', 2)
    check('help', 'SUMMARY OF LESS COMMANDS' in '\n'.join(s.screen.display), s)
    s.send('q')
    check('help-quit', s.row(0) == 'line 151', s)
    # v: the editor (EDIT, TPU/EVE), left with CTRL/Z.
    s.send('v', 5)
    check('edit', 'Buffer: TTYTEST.TXT' in '\n'.join(s.screen.display).upper()
          or 'BUFFER: TTYTEST.TXT' in '\n'.join(s.screen.display).upper(), s)
    s.send('\x1a', 4)
    check('edit-return', s.row(0) == 'line 151', s)
    s.send('q')
    s.screen.reset()
    s.dcl('show symbol $status')
    check('status', '%X00000001' in '\n'.join(s.screen.display), s)

    # The page size is the terminal's (SYS$GETDVI), not a fixed 24.
    s.dcl('set terminal/page=12')
    s.screen.reset()
    s.dcl('less ttytest.txt', 3)
    rows = [s.row(i) for i in range(24)]
    k = rows.index('line 001') if 'line 001' in rows else -99
    check('page-size', k >= 0 and k + 11 < 24 and rows[k + 10] == 'line 011'
          and 'ttytest.txt' in rows[k + 11] and 'line 012' not in rows, s)
    s.send('q')
    s.dcl('set terminal/page=24')
    s.dcl('delete/nolog ttytest.txt;*')
    s.dcl('delete/symbol/global less')
    s.dcl('logout', 2)

    passed = sum(results)
    summary = 'TTYTEST: %d passed, %d failed (%s)' % (passed, len(results) - passed, node)
    print(summary)
    log.write(summary + '\n')
    sys.exit(0 if passed == len(results) else 1)


if __name__ == '__main__':
    main()
